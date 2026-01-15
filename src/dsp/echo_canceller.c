/**
 * @file echo_canceller.c
 * @brief Acoustic Echo Cancellation (AEC) implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1 Enhancement:
 * - FDAF (Frequency-Domain Adaptive Filter) implementation
 * - DTD (Double-Talk Detection) using normalized cross-correlation
 * - GCC-PHAT delay estimator integration
 * - Time-domain NLMS fallback
 */

#include "dsp/echo_canceller.h"
#include "dsp/delay_estimator.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Constants
 * ============================================ */

#define AEC_MAX_FILTER_LENGTH   4096    /* Max filter length (samples) */
#define AEC_MAX_FRAME_SIZE      512     /* Max frame size */
#define AEC_FFT_SIZE            512     /* FFT size */
#define AEC_PLAYBACK_BUFFER_MS  500     /* Playback buffer length (ms) */
#define AEC_DTD_HANGOVER        10      /* DTD hangover frames */
#define AEC_ERLE_SMOOTH         0.98f   /* ERLE smoothing factor */
#define AEC_CONVERGENCE_THRESH  0.7f    /* Convergence threshold */

/* ============================================
 * Internal Types (Split Real/Imag for SIMD)
 * ============================================ */

/** Split complex buffer (separate real/imag arrays for SIMD optimization) */
typedef struct {
    float *real;                /* Real parts array */
    float *imag;                /* Imaginary parts array */
    size_t size;                /* Array size */
} split_complex_t;

/** FDAF filter block (using split complex format) */
typedef struct {
    split_complex_t coeffs;     /* Frequency-domain filter coefficients */
    float *power;               /* Frequency power spectrum estimate */
} fdaf_block_t;

/** DTD detector */
typedef struct {
    float far_energy;           /* Far-end energy (smoothed) */
    float near_energy;          /* Near-end energy (smoothed) */
    float error_energy;         /* Error energy */
    float cross_corr;           /* Cross-correlation */
    voice_dtd_state_t state;    /* Current state */
    int hangover;               /* Hangover count */
} dtd_detector_t;

/* ============================================
 * Internal Structure (Split Complex Format)
 * ============================================ */

struct voice_aec_s {
    voice_aec_ext_config_t config;
    bool enabled;
    uint64_t frames_processed;

    /* Playback buffer (for async mode) */
    int16_t *playback_buffer;
    size_t playback_buffer_size;
    size_t playback_write_pos;
    size_t playback_read_pos;
    size_t playback_count;              /* Samples in buffer */

    /* FDAF filter */
    size_t num_blocks;                  /* Number of filter blocks */
    size_t fft_size;                    /* FFT size */
    fdaf_block_t *filter_blocks;        /* Filter block array */
    split_complex_t fft_buffer;         /* FFT work buffer (split format) */
    float *window;                      /* Analysis window */

    /* Input buffers */
    float *far_buffer;                  /* Far-end input buffer */
    float *near_buffer;                 /* Near-end input buffer */
    float *error_buffer;                /* Error buffer */
    split_complex_t far_freq;           /* Far-end frequency domain */
    split_complex_t near_freq;          /* Near-end frequency domain */
    split_complex_t error_freq;         /* Error frequency domain */
    split_complex_t echo_est_freq;      /* Echo estimate frequency domain */

    /* Time-domain NLMS (fallback) */
    float *nlms_weights;                /* NLMS weights */
    float *nlms_x_buffer;               /* NLMS input buffer */

    /* Delay estimator */
    voice_delay_estimator_t *delay_est;
    int known_delay_samples;            /* Known delay (if set) */
    bool use_known_delay;

    /* DTD detection */
    dtd_detector_t dtd;

    /* Statistics */
    float erle_db;                      /* ERLE (dB) */
    float convergence;                  /* Convergence level */
};

/* ============================================
 * Internal Function Declarations
 * ============================================ */

static void fft_forward_split(float *real, float *imag, size_t n);
static void fft_inverse_split(float *real, float *imag, size_t n);
static float compute_energy(const float *data, size_t n);
static void dtd_update(dtd_detector_t *dtd, float far_energy,
                      float near_energy, float error_energy, float threshold);
static void fdaf_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size);
static void nlms_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size);
static void generate_hann_window(float *window, size_t n);

/* Helper functions for split complex buffer management */
static bool split_complex_alloc(split_complex_t *sc, size_t size);
static void split_complex_free(split_complex_t *sc);
static void split_complex_zero(split_complex_t *sc);

/* ============================================
 * Split Complex Buffer Management
 * ============================================ */

static bool split_complex_alloc(split_complex_t *sc, size_t size) {
    sc->real = (float *)calloc(size, sizeof(float));
    sc->imag = (float *)calloc(size, sizeof(float));
    sc->size = size;
    return (sc->real != NULL && sc->imag != NULL);
}

static void split_complex_free(split_complex_t *sc) {
    if (sc->real) { free(sc->real); sc->real = NULL; }
    if (sc->imag) { free(sc->imag); sc->imag = NULL; }
    sc->size = 0;
}

static void split_complex_zero(split_complex_t *sc) {
    if (sc->real) memset(sc->real, 0, sc->size * sizeof(float));
    if (sc->imag) memset(sc->imag, 0, sc->size * sizeof(float));
}

/* ============================================
 * FFT Implementation (Split Format, SIMD-friendly)
 * ============================================ */

static void fft_forward_split(float *real, float *imag, size_t n) {
    /* Bit-reversal permutation */
    size_t j = 0;
    for (size_t i = 0; i < n - 1; i++) {
        if (i < j) {
            float temp_r = real[i];
            float temp_i = imag[i];
            real[i] = real[j];
            imag[i] = imag[j];
            real[j] = temp_r;
            imag[j] = temp_i;
        }
        size_t k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    /* Cooley-Tukey butterfly operations */
    for (size_t len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * (float)M_PI / (float)len;
        float w_len_r = cosf(angle);
        float w_len_i = sinf(angle);

        for (size_t i = 0; i < n; i += len) {
            float w_r = 1.0f;
            float w_i = 0.0f;
            size_t half = len >> 1;
            for (size_t k = 0; k < half; k++) {
                size_t idx1 = i + k;
                size_t idx2 = i + k + half;

                /* t = w * data[idx2] */
                float t_r = w_r * real[idx2] - w_i * imag[idx2];
                float t_i = w_r * imag[idx2] + w_i * real[idx2];

                /* Butterfly */
                float u_r = real[idx1];
                float u_i = imag[idx1];
                real[idx1] = u_r + t_r;
                imag[idx1] = u_i + t_i;
                real[idx2] = u_r - t_r;
                imag[idx2] = u_i - t_i;

                /* Update twiddle factor */
                float new_w_r = w_r * w_len_r - w_i * w_len_i;
                float new_w_i = w_r * w_len_i + w_i * w_len_r;
                w_r = new_w_r;
                w_i = new_w_i;
            }
        }
    }
}

static void fft_inverse_split(float *real, float *imag, size_t n) {
    /* Conjugate */
    for (size_t i = 0; i < n; i++) {
        imag[i] = -imag[i];
    }

    /* Forward FFT */
    fft_forward_split(real, imag, n);

    /* Conjugate and normalize */
    float scale = 1.0f / (float)n;
    for (size_t i = 0; i < n; i++) {
        real[i] *= scale;
        imag[i] = -imag[i] * scale;
    }
}

/* ============================================
 * Utility Functions
 * ============================================ */

static float compute_energy(const float *data, size_t n) {
    /* Use SIMD-optimized energy calculation (returns sum, not mean) */
    return voice_compute_energy_float(data, n) * (float)n;
}

static void generate_hann_window(float *window, size_t n) {
    for (size_t i = 0; i < n; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1)));
    }
}

/* ============================================
 * DTD Detection
 * ============================================ */

static void dtd_update(dtd_detector_t *dtd, float far_energy,
                      float near_energy, float error_energy, float threshold) {
    const float smooth = 0.9f;

    /* Smooth energy values */
    dtd->far_energy = smooth * dtd->far_energy + (1.0f - smooth) * far_energy;
    dtd->near_energy = smooth * dtd->near_energy + (1.0f - smooth) * near_energy;
    dtd->error_energy = smooth * dtd->error_energy + (1.0f - smooth) * error_energy;

    /* Compute correlation metric */
    float eps = 1e-10f;

    /* Echo Return Loss Enhancement ratio */
    float echo_ratio = dtd->error_energy / (dtd->near_energy + eps);

    /* Double-talk detection: if error energy >> expected echo, near-end is speaking */
    bool far_active = dtd->far_energy > 1e-6f;
    bool near_active = dtd->near_energy > 1e-6f;
    bool double_talk = echo_ratio > threshold && far_active && near_active;

    /* State machine */
    if (double_talk) {
        dtd->state = VOICE_DTD_DOUBLE_TALK;
        dtd->hangover = AEC_DTD_HANGOVER;
    } else if (dtd->hangover > 0) {
        dtd->hangover--;
        /* Maintain current state */
    } else if (far_active && !near_active) {
        dtd->state = VOICE_DTD_FAR_END;
    } else if (!far_active && near_active) {
        dtd->state = VOICE_DTD_NEAR_END;
    } else {
        dtd->state = VOICE_DTD_IDLE;
    }
}

/* ============================================
 * FDAF Filter (SIMD-Optimized Split Complex)
 * ============================================ */

static void fdaf_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size) {
    size_t fft_size = aec->fft_size;
    size_t half_fft = fft_size / 2;

    /* Shift far-end buffer and add new frame */
    memmove(aec->far_buffer, aec->far_buffer + frame_size,
            (fft_size - frame_size) * sizeof(float));
    memcpy(aec->far_buffer + fft_size - frame_size, far, frame_size * sizeof(float));

    /* Prepare FFT input (windowed) */
    for (size_t i = 0; i < fft_size; i++) {
        aec->fft_buffer.real[i] = aec->far_buffer[i] * aec->window[i];
        aec->fft_buffer.imag[i] = 0.0f;
    }
    fft_forward_split(aec->fft_buffer.real, aec->fft_buffer.imag, fft_size);

    /* Copy to far_freq */
    memcpy(aec->far_freq.real, aec->fft_buffer.real, fft_size * sizeof(float));
    memcpy(aec->far_freq.imag, aec->fft_buffer.imag, fft_size * sizeof(float));

    /* Shift filter blocks */
    for (size_t b = aec->num_blocks - 1; b > 0; b--) {
        memcpy(aec->filter_blocks[b].coeffs.real, aec->filter_blocks[b - 1].coeffs.real,
               fft_size * sizeof(float));
        memcpy(aec->filter_blocks[b].coeffs.imag, aec->filter_blocks[b - 1].coeffs.imag,
               fft_size * sizeof(float));
    }
    memcpy(aec->filter_blocks[0].coeffs.real, aec->far_freq.real, fft_size * sizeof(float));
    memcpy(aec->filter_blocks[0].coeffs.imag, aec->far_freq.imag, fft_size * sizeof(float));

    /* Compute echo estimate: Y = sum(H_k * X_k) using SIMD complex multiply */
    split_complex_zero(&aec->echo_est_freq);

    /* Temporary buffer for intermediate results */
    float temp_real[AEC_FFT_SIZE];
    float temp_imag[AEC_FFT_SIZE];

    for (size_t b = 0; b < aec->num_blocks; b++) {
        /* Use SIMD-optimized complex multiplication */
        voice_complex_mul(
            aec->filter_blocks[b].coeffs.real, aec->filter_blocks[b].coeffs.imag,
            aec->far_freq.real, aec->far_freq.imag,
            temp_real, temp_imag,
            fft_size
        );

        /* Accumulate results */
        for (size_t i = 0; i < fft_size; i++) {
            aec->echo_est_freq.real[i] += temp_real[i];
            aec->echo_est_freq.imag[i] += temp_imag[i];
        }
    }

    /* IFFT to get time-domain echo estimate */
    memcpy(aec->fft_buffer.real, aec->echo_est_freq.real, fft_size * sizeof(float));
    memcpy(aec->fft_buffer.imag, aec->echo_est_freq.imag, fft_size * sizeof(float));
    fft_inverse_split(aec->fft_buffer.real, aec->fft_buffer.imag, fft_size);

    /* Extract second half of echo estimate */
    float echo_estimate[AEC_MAX_FRAME_SIZE];
    for (size_t i = 0; i < frame_size && i < AEC_MAX_FRAME_SIZE; i++) {
        echo_estimate[i] = aec->fft_buffer.real[half_fft + i];
    }

    /* Compute error */
    for (size_t i = 0; i < frame_size; i++) {
        output[i] = near[i] - echo_estimate[i];
    }

    /* DTD detection */
    float far_energy = compute_energy(far, frame_size);
    float near_energy = compute_energy(near, frame_size);
    float error_energy = compute_energy(output, frame_size);
    dtd_update(&aec->dtd, far_energy, near_energy, error_energy, aec->config.dtd_threshold);

    /* Compute ERLE */
    float eps = 1e-10f;
    float erle_linear = near_energy / (error_energy + eps);
    aec->erle_db = AEC_ERLE_SMOOTH * aec->erle_db +
                   (1.0f - AEC_ERLE_SMOOTH) * 10.0f * log10f(erle_linear + eps);

    /* Adaptive update (unless double-talk) */
    if (aec->dtd.state != VOICE_DTD_DOUBLE_TALK &&
        aec->dtd.state != VOICE_DTD_NEAR_END) {

        float mu = aec->config.nlms_step_size;

        /* Prepare error in frequency domain */
        for (size_t i = 0; i < fft_size; i++) {
            if (i < half_fft) {
                aec->fft_buffer.real[i] = 0.0f;
            } else {
                aec->fft_buffer.real[i] = output[i - half_fft] * aec->window[i];
            }
            aec->fft_buffer.imag[i] = 0.0f;
        }
        fft_forward_split(aec->fft_buffer.real, aec->fft_buffer.imag, fft_size);
        memcpy(aec->error_freq.real, aec->fft_buffer.real, fft_size * sizeof(float));
        memcpy(aec->error_freq.imag, aec->fft_buffer.imag, fft_size * sizeof(float));

        /* Update filter: H = H + mu * E * conj(X) / |X|^2 */
        /* Use SIMD complex multiply conjugate */
        float update_real[AEC_FFT_SIZE];
        float update_imag[AEC_FFT_SIZE];
        voice_complex_mul_conj(
            aec->error_freq.real, aec->error_freq.imag,
            aec->far_freq.real, aec->far_freq.imag,
            update_real, update_imag,
            fft_size
        );

        /* Normalize by power and apply step size */
        for (size_t i = 0; i < fft_size; i++) {
            float power = aec->far_freq.real[i] * aec->far_freq.real[i] +
                         aec->far_freq.imag[i] * aec->far_freq.imag[i] + eps;
            float inv_power = 1.0f / power;

            aec->filter_blocks[0].coeffs.real[i] += mu * update_real[i] * inv_power;
            aec->filter_blocks[0].coeffs.imag[i] += mu * update_imag[i] * inv_power;
        }

        /* Update convergence estimate */
        aec->convergence = aec->convergence * 0.99f + 0.01f *
            (aec->erle_db > 10.0f ? 1.0f : aec->erle_db / 10.0f);
    }
}

/* ============================================
 * NLMS Filter (Fallback)
 * ============================================ */

static void nlms_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size) {
    size_t filter_len = aec->config.filter_length;
    if (filter_len > AEC_MAX_FILTER_LENGTH) {
        filter_len = AEC_MAX_FILTER_LENGTH;
    }

    float mu = aec->config.nlms_step_size;
    float eps = 1e-10f;

    for (size_t n = 0; n < frame_size; n++) {
        /* Shift input buffer */
        memmove(aec->nlms_x_buffer + 1, aec->nlms_x_buffer,
                (filter_len - 1) * sizeof(float));
        aec->nlms_x_buffer[0] = far[n];

        /* Compute echo estimate */
        float echo_est = 0.0f;
        for (size_t i = 0; i < filter_len; i++) {
            echo_est += aec->nlms_weights[i] * aec->nlms_x_buffer[i];
        }

        /* Error */
        float error = near[n] - echo_est;
        output[n] = error;

        /* Compute input power */
        float power = 0.0f;
        for (size_t i = 0; i < filter_len; i++) {
            power += aec->nlms_x_buffer[i] * aec->nlms_x_buffer[i];
        }

        /* Update weights */
        float norm_mu = mu / (power + eps);
        for (size_t i = 0; i < filter_len; i++) {
            aec->nlms_weights[i] += norm_mu * error * aec->nlms_x_buffer[i];
        }
    }

    /* Update DTD */
    float far_energy = compute_energy(far, frame_size);
    float near_energy = compute_energy(near, frame_size);
    float error_energy = compute_energy(output, frame_size);
    dtd_update(&aec->dtd, far_energy, near_energy, error_energy, aec->config.dtd_threshold);
}

/* ============================================
 * API Implementation
 * ============================================ */

void voice_aec_ext_config_init(voice_aec_ext_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->sample_rate = 16000;
    config->frame_size = 160;        /* 10ms @ 16kHz */
    config->filter_length = 2560;    /* 160ms @ 16kHz */
    config->echo_suppress_db = -40;
    config->echo_suppress_active_db = -15;
    config->enable_residual_echo_suppress = true;
    config->enable_comfort_noise = false;

    /* Phase 1 defaults */
    config->algorithm = VOICE_AEC_ALG_FDAF;
    config->enable_delay_estimation = true;
    config->enable_dtd = true;
    config->tail_length_ms = 200;
    config->nlms_step_size = 0.3f;
    config->dtd_threshold = 0.65f;
}

voice_aec_t *voice_aec_create(const voice_aec_ext_config_t *config) {
    if (!config) return NULL;

    voice_aec_t *aec = (voice_aec_t *)calloc(1, sizeof(voice_aec_t));
    if (!aec) return NULL;

    aec->config = *config;
    aec->enabled = true;
    aec->frames_processed = 0;

    /* Calculate FFT size (2x frame size, power of 2) */
    size_t fft_size = 256;
    while (fft_size < config->frame_size * 2) {
        fft_size *= 2;
    }
    if (fft_size > AEC_FFT_SIZE) {
        fft_size = AEC_FFT_SIZE;
    }
    aec->fft_size = fft_size;

    /* Calculate number of filter blocks */
    size_t tail_samples = (config->tail_length_ms * config->sample_rate) / 1000;
    aec->num_blocks = (tail_samples + config->frame_size - 1) / config->frame_size;
    if (aec->num_blocks < 1) aec->num_blocks = 1;
    if (aec->num_blocks > 16) aec->num_blocks = 16;

    /* Allocate playback buffer */
    aec->playback_buffer_size = (config->sample_rate * AEC_PLAYBACK_BUFFER_MS) / 1000;
    aec->playback_buffer = (int16_t *)calloc(aec->playback_buffer_size, sizeof(int16_t));
    if (!aec->playback_buffer) goto cleanup;
    aec->playback_write_pos = 0;
    aec->playback_read_pos = 0;
    aec->playback_count = 0;

    /* Allocate FDAF resources (using split complex format) */
    if (config->algorithm == VOICE_AEC_ALG_FDAF) {
        aec->filter_blocks = (fdaf_block_t *)calloc(aec->num_blocks, sizeof(fdaf_block_t));
        if (!aec->filter_blocks) goto cleanup;

        for (size_t i = 0; i < aec->num_blocks; i++) {
            if (!split_complex_alloc(&aec->filter_blocks[i].coeffs, fft_size)) goto cleanup;
            aec->filter_blocks[i].power = (float *)calloc(fft_size, sizeof(float));
            if (!aec->filter_blocks[i].power) goto cleanup;
        }

        /* Allocate split complex buffers */
        if (!split_complex_alloc(&aec->fft_buffer, fft_size)) goto cleanup;
        if (!split_complex_alloc(&aec->far_freq, fft_size)) goto cleanup;
        if (!split_complex_alloc(&aec->near_freq, fft_size)) goto cleanup;
        if (!split_complex_alloc(&aec->error_freq, fft_size)) goto cleanup;
        if (!split_complex_alloc(&aec->echo_est_freq, fft_size)) goto cleanup;

        aec->window = (float *)calloc(fft_size, sizeof(float));
        aec->far_buffer = (float *)calloc(fft_size, sizeof(float));
        aec->near_buffer = (float *)calloc(fft_size, sizeof(float));
        aec->error_buffer = (float *)calloc(fft_size, sizeof(float));

        if (!aec->window || !aec->far_buffer ||
            !aec->near_buffer || !aec->error_buffer) {
            goto cleanup;
        }

        generate_hann_window(aec->window, fft_size);
    }

    /* Allocate NLMS resources */
    if (config->algorithm == VOICE_AEC_ALG_NLMS) {
        size_t filter_len = config->filter_length;
        if (filter_len > AEC_MAX_FILTER_LENGTH) {
            filter_len = AEC_MAX_FILTER_LENGTH;
        }
        aec->nlms_weights = (float *)calloc(filter_len, sizeof(float));
        aec->nlms_x_buffer = (float *)calloc(filter_len, sizeof(float));
        if (!aec->nlms_weights || !aec->nlms_x_buffer) goto cleanup;
    }

    /* Create delay estimator */
    if (config->enable_delay_estimation) {
        voice_delay_estimator_config_t de_config;
        voice_delay_estimator_config_init(&de_config);
        de_config.sample_rate = config->sample_rate;
        de_config.frame_size = config->frame_size;
        de_config.max_delay_samples = tail_samples;
        aec->delay_est = voice_delay_estimator_create(&de_config);
        /* Delay estimator creation failure is not fatal */
    }

    /* Initialize DTD */
    memset(&aec->dtd, 0, sizeof(dtd_detector_t));
    aec->dtd.state = VOICE_DTD_IDLE;

    /* Initialize state */
    aec->erle_db = 0.0f;
    aec->convergence = 0.0f;
    aec->known_delay_samples = 0;
    aec->use_known_delay = false;

    return aec;

cleanup:
    voice_aec_destroy(aec);
    return NULL;
}

void voice_aec_destroy(voice_aec_t *aec) {
    if (!aec) return;

    /* Free playback buffer */
    if (aec->playback_buffer) {
        free(aec->playback_buffer);
    }

    /* Free FDAF resources (split complex format) */
    if (aec->filter_blocks) {
        for (size_t i = 0; i < aec->num_blocks; i++) {
            split_complex_free(&aec->filter_blocks[i].coeffs);
            if (aec->filter_blocks[i].power) free(aec->filter_blocks[i].power);
        }
        free(aec->filter_blocks);
    }
    split_complex_free(&aec->fft_buffer);
    split_complex_free(&aec->far_freq);
    split_complex_free(&aec->near_freq);
    split_complex_free(&aec->error_freq);
    split_complex_free(&aec->echo_est_freq);

    if (aec->window) free(aec->window);
    if (aec->far_buffer) free(aec->far_buffer);
    if (aec->near_buffer) free(aec->near_buffer);
    if (aec->error_buffer) free(aec->error_buffer);

    /* Free NLMS resources */
    if (aec->nlms_weights) free(aec->nlms_weights);
    if (aec->nlms_x_buffer) free(aec->nlms_x_buffer);

    /* Destroy delay estimator */
    if (aec->delay_est) {
        voice_delay_estimator_destroy(aec->delay_est);
    }

    free(aec);
}

voice_error_t voice_aec_process(
    voice_aec_t *aec,
    const int16_t *mic_input,
    const int16_t *speaker_ref,
    int16_t *output,
    size_t frame_count
) {
    if (!aec || !mic_input || !output) return VOICE_ERROR_NULL_POINTER;

    if (!aec->enabled) {
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }

    /* Convert to float */
    float far_float[AEC_MAX_FRAME_SIZE];
    float near_float[AEC_MAX_FRAME_SIZE];
    float out_float[AEC_MAX_FRAME_SIZE];

    size_t process_size = frame_count;
    if (process_size > AEC_MAX_FRAME_SIZE) {
        process_size = AEC_MAX_FRAME_SIZE;
    }

    const float scale_in = 1.0f / 32768.0f;
    for (size_t i = 0; i < process_size; i++) {
        near_float[i] = (float)mic_input[i] * scale_in;
        far_float[i] = speaker_ref ? (float)speaker_ref[i] * scale_in : 0.0f;
    }

    /* Delay estimation */
    if (aec->delay_est && aec->config.enable_delay_estimation && speaker_ref) {
        voice_delay_estimate_t delay_result;
        voice_delay_estimator_estimate_float(aec->delay_est, far_float, near_float,
                                              process_size, &delay_result);
    }

    /* Select filter based on algorithm */
    switch (aec->config.algorithm) {
        case VOICE_AEC_ALG_FDAF:
            fdaf_filter(aec, far_float, near_float, out_float, process_size);
            break;

        case VOICE_AEC_ALG_NLMS:
            nlms_filter(aec, far_float, near_float, out_float, process_size);
            break;

        case VOICE_AEC_ALG_SPEEX:
            /* Fallback to direct copy (SpeexDSP not integrated) */
            memcpy(out_float, near_float, process_size * sizeof(float));
            break;

        default:
            memcpy(out_float, near_float, process_size * sizeof(float));
            break;
    }

    /* Convert back to int16 */
    for (size_t i = 0; i < process_size; i++) {
        float sample = out_float[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        output[i] = (int16_t)sample;
    }

    /* Process remaining samples */
    for (size_t i = process_size; i < frame_count; i++) {
        output[i] = mic_input[i];
    }

    aec->frames_processed++;
    return VOICE_OK;
}

voice_error_t voice_aec_playback(
    voice_aec_t *aec,
    const int16_t *speaker_data,
    size_t frame_count
) {
    if (!aec || !speaker_data) return VOICE_ERROR_NULL_POINTER;

    /* Write to playback buffer */
    for (size_t i = 0; i < frame_count; i++) {
        aec->playback_buffer[aec->playback_write_pos] = speaker_data[i];
        aec->playback_write_pos = (aec->playback_write_pos + 1) % aec->playback_buffer_size;

        if (aec->playback_count < aec->playback_buffer_size) {
            aec->playback_count++;
        } else {
            /* Buffer full, move read pointer */
            aec->playback_read_pos = (aec->playback_read_pos + 1) % aec->playback_buffer_size;
        }
    }

    return VOICE_OK;
}

voice_error_t voice_aec_capture(
    voice_aec_t *aec,
    const int16_t *mic_input,
    int16_t *output,
    size_t frame_count
) {
    if (!aec || !mic_input || !output) return VOICE_ERROR_NULL_POINTER;

    if (!aec->enabled) {
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }

    /* Get delay */
    int delay_samples;
    if (aec->use_known_delay) {
        delay_samples = aec->known_delay_samples;
    } else if (aec->delay_est) {
        delay_samples = voice_delay_estimator_get_delay(aec->delay_est);
    } else {
        delay_samples = (int)(aec->config.frame_size * 2);
    }

    /* Read far-end reference from playback buffer */
    int16_t far_ref[AEC_MAX_FRAME_SIZE];
    size_t read_count = frame_count;
    if (read_count > AEC_MAX_FRAME_SIZE) {
        read_count = AEC_MAX_FRAME_SIZE;
    }

    /* Calculate read position (considering delay) */
    size_t read_pos = aec->playback_read_pos;
    if (delay_samples > 0 && (size_t)delay_samples < aec->playback_count) {
        read_pos = (aec->playback_write_pos + aec->playback_buffer_size - (size_t)delay_samples)
                   % aec->playback_buffer_size;
    }

    for (size_t i = 0; i < read_count; i++) {
        if (aec->playback_count > 0) {
            far_ref[i] = aec->playback_buffer[read_pos];
            read_pos = (read_pos + 1) % aec->playback_buffer_size;
        } else {
            far_ref[i] = 0;
        }
    }

    /* Call synchronous processing */
    return voice_aec_process(aec, mic_input, far_ref, output, frame_count);
}

voice_error_t voice_aec_set_suppress(
    voice_aec_t *aec,
    int suppress_db,
    int suppress_active_db
) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->config.echo_suppress_db = suppress_db;
    aec->config.echo_suppress_active_db = suppress_active_db;

    return VOICE_OK;
}

voice_error_t voice_aec_set_enabled(voice_aec_t *aec, bool enabled) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->enabled = enabled;

    return VOICE_OK;
}

bool voice_aec_is_enabled(voice_aec_t *aec) {
    if (!aec) return false;

    return aec->enabled;
}

void voice_aec_reset(voice_aec_t *aec) {
    if (!aec) return;

    /* Reset playback buffer */
    if (aec->playback_buffer) {
        memset(aec->playback_buffer, 0, aec->playback_buffer_size * sizeof(int16_t));
    }
    aec->playback_write_pos = 0;
    aec->playback_read_pos = 0;
    aec->playback_count = 0;

    /* Reset FDAF filter (using split complex format) */
    if (aec->filter_blocks) {
        for (size_t i = 0; i < aec->num_blocks; i++) {
            split_complex_zero(&aec->filter_blocks[i].coeffs);
            if (aec->filter_blocks[i].power) {
                memset(aec->filter_blocks[i].power, 0, aec->fft_size * sizeof(float));
            }
        }
    }

    /* Reset FDAF buffers */
    if (aec->far_buffer) memset(aec->far_buffer, 0, aec->fft_size * sizeof(float));
    if (aec->near_buffer) memset(aec->near_buffer, 0, aec->fft_size * sizeof(float));
    if (aec->error_buffer) memset(aec->error_buffer, 0, aec->fft_size * sizeof(float));

    /* Reset NLMS */
    if (aec->nlms_weights) {
        memset(aec->nlms_weights, 0, aec->config.filter_length * sizeof(float));
    }
    if (aec->nlms_x_buffer) {
        memset(aec->nlms_x_buffer, 0, aec->config.filter_length * sizeof(float));
    }

    /* Reset delay estimator */
    if (aec->delay_est) {
        voice_delay_estimator_reset(aec->delay_est);
    }

    /* Reset DTD */
    memset(&aec->dtd, 0, sizeof(dtd_detector_t));
    aec->dtd.state = VOICE_DTD_IDLE;

    /* Reset state */
    aec->erle_db = 0.0f;
    aec->convergence = 0.0f;
    aec->frames_processed = 0;
}

int voice_aec_get_delay(voice_aec_t *aec) {
    if (!aec) return 0;

    if (aec->use_known_delay) {
        return aec->known_delay_samples;
    }

    if (aec->delay_est) {
        return voice_delay_estimator_get_delay(aec->delay_est);
    }

    /* Return default estimated delay */
    return (int)aec->config.frame_size * 2;
}

/* ============================================
 * Phase 1 Extended API
 * ============================================ */

voice_error_t voice_aec_set_delay(voice_aec_t *aec, int delay_samples) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    if (delay_samples < 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    aec->known_delay_samples = delay_samples;
    aec->use_known_delay = (delay_samples > 0);

    return VOICE_OK;
}

voice_error_t voice_aec_get_state(voice_aec_t *aec, voice_aec_state_t *state) {
    if (!aec || !state) return VOICE_ERROR_NULL_POINTER;

    state->dtd_state = aec->dtd.state;
    state->erle_db = aec->erle_db;
    state->convergence = aec->convergence;
    state->frames_processed = aec->frames_processed;

    if (aec->delay_est) {
        state->estimated_delay_samples = voice_delay_estimator_get_delay(aec->delay_est);
        state->estimated_delay_ms = (float)state->estimated_delay_samples * 1000.0f /
                                    (float)aec->config.sample_rate;
        state->delay_stable = voice_delay_estimator_is_stable(aec->delay_est);
    } else if (aec->use_known_delay) {
        state->estimated_delay_samples = aec->known_delay_samples;
        state->estimated_delay_ms = (float)aec->known_delay_samples * 1000.0f /
                                    (float)aec->config.sample_rate;
        state->delay_stable = true;
    } else {
        state->estimated_delay_samples = 0;
        state->estimated_delay_ms = 0.0f;
        state->delay_stable = false;
    }

    return VOICE_OK;
}

voice_dtd_state_t voice_aec_get_dtd_state(voice_aec_t *aec) {
    if (!aec) return VOICE_DTD_IDLE;
    return aec->dtd.state;
}

voice_error_t voice_aec_enable_delay_estimation(voice_aec_t *aec, bool enabled) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->config.enable_delay_estimation = enabled;

    /* If enabling and delay estimator doesn't exist, create it */
    if (enabled && !aec->delay_est) {
        voice_delay_estimator_config_t de_config;
        voice_delay_estimator_config_init(&de_config);
        de_config.sample_rate = aec->config.sample_rate;
        de_config.frame_size = aec->config.frame_size;
        de_config.max_delay_samples = (aec->config.tail_length_ms * aec->config.sample_rate) / 1000;
        aec->delay_est = voice_delay_estimator_create(&de_config);
    }

    return VOICE_OK;
}
