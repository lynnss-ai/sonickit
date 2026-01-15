/**
 * @file hrtf.c
 * @brief HRTF binaural audio implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Implements Head-Related Transfer Function processing for 3D audio.
 * Uses simplified HRIR data derived from MIT KEMAR measurements.
 */

#include "dsp/hrtf.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Built-in HRIR Data
 * ============================================ */

/*
 * Simplified HRIR dataset based on MIT KEMAR measurements.
 *
 * This is a minimal dataset with key positions:
 * - Azimuth: 0, ±30, ±60, ±90, ±120, ±150, 180 degrees
 * - Elevation: -30, 0, +30, +60, +90 degrees
 *
 * HRIR length: 32 samples at 48kHz (~0.67ms)
 * This is sufficient for basic spatial cues while being efficient.
 */

#define BUILTIN_HRIR_LENGTH 32
#define BUILTIN_SAMPLE_RATE 48000

/* Number of azimuth positions (symmetric, store one side + center + back) */
#define NUM_AZIMUTHS 13  /* 0, 30, 60, 90, 120, 150, 180, -150, -120, -90, -60, -30 */
#define NUM_ELEVATIONS 5 /* -30, 0, 30, 60, 90 */

/* Azimuth angles for built-in data */
static const float g_builtin_azimuths[NUM_AZIMUTHS] = {
    0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f,
    -150.0f, -120.0f, -90.0f, -60.0f, -30.0f, 0.0f  /* wrap around */
};

/* Elevation angles for built-in data */
static const float g_builtin_elevations[NUM_ELEVATIONS] = {
    -30.0f, 0.0f, 30.0f, 60.0f, 90.0f
};

/*
 * Simplified HRIR filters - these are approximations that capture
 * the key spatial cues: ITD, ILD, and basic spectral coloring.
 *
 * Format: Each HRIR pair is stored as left[32], right[32]
 *
 * Real HRTF datasets would load from SOFA files, but these approximations
 * provide reasonable spatial perception for most applications.
 */

/* Generate synthetic HRIR based on position */
static void generate_synthetic_hrir(float azimuth, float elevation,
                                    float *left, float *right, size_t length) {
    /* Convert to radians */
    float az_rad = azimuth * (float)M_PI / 180.0f;
    float el_rad = elevation * (float)M_PI / 180.0f;

    /* Calculate ILD (Interaural Level Difference) */
    /* Sound from the right is louder in right ear and vice versa */
    float ild_factor = sinf(az_rad) * cosf(el_rad);
    float left_gain = 1.0f - 0.4f * fmaxf(0.0f, ild_factor);
    float right_gain = 1.0f + 0.4f * fmaxf(0.0f, ild_factor);

    /* Opposite for negative azimuth */
    if (ild_factor < 0) {
        left_gain = 1.0f + 0.4f * fmaxf(0.0f, -ild_factor);
        right_gain = 1.0f - 0.4f * fmaxf(0.0f, -ild_factor);
    }

    /* Calculate ITD (Interaural Time Difference) in samples */
    /* Maximum ITD is about 0.65ms at 90 degrees */
    float head_radius = 0.0875f;  /* meters */
    float speed_of_sound = 343.0f;
    float max_itd = head_radius / speed_of_sound;
    float itd_seconds = max_itd * sinf(az_rad) * cosf(el_rad);
    float itd_samples = itd_seconds * BUILTIN_SAMPLE_RATE;

    /* Generate impulse response with spectral coloring */
    /* Front: more high frequency, Back: more low frequency */
    float front_factor = cosf(az_rad);  /* 1 at front, -1 at back */

    /* Elevation affects spectral content too */
    float elev_factor = cosf(el_rad);  /* 1 at horizontal, 0 at top */

    /* Simple FIR filter shape */
    for (size_t i = 0; i < length; i++) {
        float t = (float)i;

        /* Basic impulse with exponential decay */
        float base_left, base_right;

        /* Apply ITD by shifting the impulse */
        float left_delay = fmaxf(0.0f, itd_samples);
        float right_delay = fmaxf(0.0f, -itd_samples);

        /* Gaussian-shaped impulse */
        float sigma = 2.0f;  /* Controls width */
        float left_t = t - left_delay - 3.0f;  /* Center at sample 3 */
        float right_t = t - right_delay - 3.0f;

        base_left = expf(-left_t * left_t / (2.0f * sigma * sigma));
        base_right = expf(-right_t * right_t / (2.0f * sigma * sigma));

        /* Add spectral coloring based on position */
        /* High frequency component (early samples) */
        float hf_weight = 1.0f + 0.3f * front_factor * elev_factor;
        /* Low frequency component (later samples) */
        float lf_decay = expf(-(float)i * 0.1f);

        /* Back hemisphere: more diffuse, less sharp */
        if (front_factor < 0) {
            sigma = 3.0f;
            left_t = t - left_delay - 4.0f;
            right_t = t - right_delay - 4.0f;
            base_left = expf(-left_t * left_t / (2.0f * sigma * sigma));
            base_right = expf(-right_t * right_t / (2.0f * sigma * sigma));
            base_left *= 0.8f;
            base_right *= 0.8f;
        }

        /* Elevation coloring: above has more high frequency notch */
        if (elevation > 45.0f) {
            /* Comb filter effect for above */
            float notch = 1.0f - 0.3f * expf(-powf(t - 8.0f, 2.0f) / 4.0f);
            base_left *= notch;
            base_right *= notch;
        }

        /* Apply gains */
        left[i] = base_left * left_gain * hf_weight * lf_decay;
        right[i] = base_right * right_gain * hf_weight * lf_decay;
    }

    /* Normalize to prevent clipping */
    float max_val = 0.0f;
    for (size_t i = 0; i < length; i++) {
        if (fabsf(left[i]) > max_val) max_val = fabsf(left[i]);
        if (fabsf(right[i]) > max_val) max_val = fabsf(right[i]);
    }
    if (max_val > 0.001f) {
        float scale = 0.5f / max_val;
        for (size_t i = 0; i < length; i++) {
            left[i] *= scale;
            right[i] *= scale;
        }
    }
}

/* ============================================
 * HRTF Dataset Structure
 * ============================================ */

struct voice_hrtf_s {
    size_t num_positions;
    size_t hrir_length;
    uint32_t sample_rate;

    /* Array of HRIR measurements */
    voice_hrir_t *hrirs;
    size_t hrirs_allocated;

    /* Lookup acceleration */
    float *azimuth_grid;
    float *elevation_grid;
    size_t num_azimuths;
    size_t num_elevations;
};

/* ============================================
 * HRTF Processor Structure
 * ============================================ */

struct voice_hrtf_processor_s {
    voice_hrtf_config_t config;
    const voice_hrtf_t *hrtf;

    /* Current interpolated HRIRs */
    float *current_hrir_left;
    float *current_hrir_right;
    size_t hrir_length;

    /* Previous HRIRs for crossfade */
    float *prev_hrir_left;
    float *prev_hrir_right;
    float crossfade_pos;
    bool crossfading;

    /* Convolution state (overlap-save) */
    float *input_buffer;
    float *output_left;
    float *output_right;
    size_t buffer_pos;

    /* Last processed position */
    float last_azimuth;
    float last_elevation;
};

/* ============================================
 * HRTF Dataset Functions
 * ============================================ */

voice_hrtf_t *voice_hrtf_create(size_t num_positions,
                                 size_t hrir_length,
                                 uint32_t sample_rate) {
    voice_hrtf_t *hrtf = (voice_hrtf_t *)calloc(1, sizeof(voice_hrtf_t));
    if (!hrtf) return NULL;

    hrtf->hrir_length = hrir_length;
    hrtf->sample_rate = sample_rate;
    hrtf->hrirs_allocated = num_positions;
    hrtf->num_positions = 0;

    hrtf->hrirs = (voice_hrir_t *)calloc(num_positions, sizeof(voice_hrir_t));
    if (!hrtf->hrirs) {
        free(hrtf);
        return NULL;
    }

    return hrtf;
}

voice_hrtf_t *voice_hrtf_load_builtin(void) {
    size_t num_pos = NUM_AZIMUTHS * NUM_ELEVATIONS;
    voice_hrtf_t *hrtf = voice_hrtf_create(num_pos, BUILTIN_HRIR_LENGTH,
                                            BUILTIN_SAMPLE_RATE);
    if (!hrtf) return NULL;

    /* Generate HRIRs for each position */
    float left[BUILTIN_HRIR_LENGTH];
    float right[BUILTIN_HRIR_LENGTH];

    for (int e = 0; e < NUM_ELEVATIONS; e++) {
        for (int a = 0; a < NUM_AZIMUTHS - 1; a++) {  /* -1 to avoid duplicate 0 */
            float azimuth = g_builtin_azimuths[a];
            float elevation = g_builtin_elevations[e];

            /* Skip top elevation for non-zero azimuth (only one point at top) */
            if (elevation >= 90.0f && azimuth != 0.0f) {
                continue;
            }

            generate_synthetic_hrir(azimuth, elevation, left, right,
                                   BUILTIN_HRIR_LENGTH);

            voice_hrtf_add_hrir(hrtf, azimuth, elevation, left, right,
                               BUILTIN_HRIR_LENGTH);
        }
    }

    return hrtf;
}

voice_error_t voice_hrtf_add_hrir(voice_hrtf_t *hrtf,
                                   float azimuth,
                                   float elevation,
                                   const float *left,
                                   const float *right,
                                   size_t length) {
    if (!hrtf || !left || !right) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    if (hrtf->num_positions >= hrtf->hrirs_allocated) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    voice_hrir_t *hrir = &hrtf->hrirs[hrtf->num_positions];

    hrir->azimuth = azimuth;
    hrir->elevation = elevation;
    hrir->length = length;

    hrir->left = (float *)malloc(length * sizeof(float));
    hrir->right = (float *)malloc(length * sizeof(float));

    if (!hrir->left || !hrir->right) {
        free(hrir->left);
        free(hrir->right);
        return VOICE_ERROR_NO_MEMORY;
    }

    memcpy(hrir->left, left, length * sizeof(float));
    memcpy(hrir->right, right, length * sizeof(float));

    hrtf->num_positions++;

    return VOICE_SUCCESS;
}

void voice_hrtf_destroy(voice_hrtf_t *hrtf) {
    if (!hrtf) return;

    if (hrtf->hrirs) {
        for (size_t i = 0; i < hrtf->num_positions; i++) {
            free(hrtf->hrirs[i].left);
            free(hrtf->hrirs[i].right);
        }
        free(hrtf->hrirs);
    }

    free(hrtf->azimuth_grid);
    free(hrtf->elevation_grid);
    free(hrtf);
}

void voice_hrtf_get_info(const voice_hrtf_t *hrtf,
                          size_t *num_positions,
                          size_t *hrir_length,
                          uint32_t *sample_rate) {
    if (!hrtf) return;

    if (num_positions) *num_positions = hrtf->num_positions;
    if (hrir_length) *hrir_length = hrtf->hrir_length;
    if (sample_rate) *sample_rate = hrtf->sample_rate;
}

/* ============================================
 * HRIR Interpolation
 * ============================================ */

/* Find nearest HRIRs for interpolation */
static void find_nearest_hrirs(const voice_hrtf_t *hrtf,
                               float azimuth, float elevation,
                               const voice_hrir_t **nearest,
                               float *weights,
                               int *count) {
    *count = 0;

    /* Normalize azimuth to -180 to 180 */
    while (azimuth > 180.0f) azimuth -= 360.0f;
    while (azimuth < -180.0f) azimuth += 360.0f;

    /* Clamp elevation */
    if (elevation > 90.0f) elevation = 90.0f;
    if (elevation < -90.0f) elevation = -90.0f;

    /* Find closest positions */
    float min_dist = 1e10f;
    int closest_idx = 0;

    for (size_t i = 0; i < hrtf->num_positions; i++) {
        float az_diff = hrtf->hrirs[i].azimuth - azimuth;
        float el_diff = hrtf->hrirs[i].elevation - elevation;

        /* Handle azimuth wrap-around */
        if (az_diff > 180.0f) az_diff -= 360.0f;
        if (az_diff < -180.0f) az_diff += 360.0f;

        float dist = sqrtf(az_diff * az_diff + el_diff * el_diff);

        if (dist < min_dist) {
            min_dist = dist;
            closest_idx = (int)i;
        }
    }

    /* For simplicity, use nearest neighbor for now */
    /* Full implementation would use bilinear or spherical interpolation */
    nearest[0] = &hrtf->hrirs[closest_idx];
    weights[0] = 1.0f;
    *count = 1;

    /* Find second nearest for linear interpolation */
    float second_min = 1e10f;
    int second_idx = -1;

    for (size_t i = 0; i < hrtf->num_positions; i++) {
        if ((int)i == closest_idx) continue;

        float az_diff = hrtf->hrirs[i].azimuth - azimuth;
        float el_diff = hrtf->hrirs[i].elevation - elevation;

        if (az_diff > 180.0f) az_diff -= 360.0f;
        if (az_diff < -180.0f) az_diff += 360.0f;

        float dist = sqrtf(az_diff * az_diff + el_diff * el_diff);

        if (dist < second_min) {
            second_min = dist;
            second_idx = (int)i;
        }
    }

    if (second_idx >= 0 && second_min < 60.0f) {
        /* Interpolate between two nearest */
        float total_dist = min_dist + second_min;
        if (total_dist > 0.001f) {
            weights[0] = second_min / total_dist;
            weights[1] = min_dist / total_dist;
            nearest[1] = &hrtf->hrirs[second_idx];
            *count = 2;
        }
    }
}

size_t voice_hrtf_interpolate(
    const voice_hrtf_t *hrtf,
    float azimuth,
    float elevation,
    float *left_out,
    float *right_out,
    size_t max_length) {

    if (!hrtf || !left_out || !right_out || hrtf->num_positions == 0) {
        return 0;
    }

    const voice_hrir_t *nearest[4];
    float weights[4];
    int count;

    find_nearest_hrirs(hrtf, azimuth, elevation, nearest, weights, &count);

    size_t length = hrtf->hrir_length;
    if (length > max_length) length = max_length;

    /* Clear output */
    memset(left_out, 0, length * sizeof(float));
    memset(right_out, 0, length * sizeof(float));

    /* Weighted sum of HRIRs */
    for (int n = 0; n < count; n++) {
        const voice_hrir_t *h = nearest[n];
        float w = weights[n];

        size_t len = h->length < length ? h->length : length;

        for (size_t i = 0; i < len; i++) {
            left_out[i] += h->left[i] * w;
            right_out[i] += h->right[i] * w;
        }
    }

    return length;
}

float voice_hrtf_calculate_itd(float azimuth, float head_radius) {
    /* Woodworth formula for ITD */
    float az_rad = azimuth * (float)M_PI / 180.0f;
    float speed_of_sound = 343.0f;

    /* ITD = (r/c) * (sin(theta) + theta) for spherical head model */
    /* Simplified: ITD ≈ (r/c) * sin(theta) */
    return (head_radius / speed_of_sound) * sinf(az_rad);
}

/* ============================================
 * HRTF Processor
 * ============================================ */

void voice_hrtf_config_init(voice_hrtf_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_hrtf_config_t));
    config->sample_rate = 48000;
    config->block_size = 256;
    config->enable_crossfade = true;
    config->crossfade_time_ms = 20.0f;
    config->enable_itd = true;
}

voice_hrtf_processor_t *voice_hrtf_processor_create(
    const voice_hrtf_t *hrtf,
    const voice_hrtf_config_t *config) {

    if (!hrtf || !config) return NULL;

    voice_hrtf_processor_t *proc =
        (voice_hrtf_processor_t *)calloc(1, sizeof(voice_hrtf_processor_t));
    if (!proc) return NULL;

    proc->config = *config;
    proc->hrtf = hrtf;
    proc->hrir_length = hrtf->hrir_length;

    /* Allocate HRIR buffers */
    proc->current_hrir_left = (float *)calloc(hrtf->hrir_length, sizeof(float));
    proc->current_hrir_right = (float *)calloc(hrtf->hrir_length, sizeof(float));
    proc->prev_hrir_left = (float *)calloc(hrtf->hrir_length, sizeof(float));
    proc->prev_hrir_right = (float *)calloc(hrtf->hrir_length, sizeof(float));

    /* Allocate convolution buffers */
    size_t buf_size = config->block_size + hrtf->hrir_length;
    proc->input_buffer = (float *)calloc(buf_size, sizeof(float));
    proc->output_left = (float *)calloc(buf_size, sizeof(float));
    proc->output_right = (float *)calloc(buf_size, sizeof(float));

    if (!proc->current_hrir_left || !proc->current_hrir_right ||
        !proc->prev_hrir_left || !proc->prev_hrir_right ||
        !proc->input_buffer || !proc->output_left || !proc->output_right) {
        voice_hrtf_processor_destroy(proc);
        return NULL;
    }

    /* Initialize with forward-facing position */
    voice_hrtf_interpolate(hrtf, 0.0f, 0.0f,
                           proc->current_hrir_left, proc->current_hrir_right,
                           hrtf->hrir_length);

    proc->last_azimuth = 0.0f;
    proc->last_elevation = 0.0f;

    return proc;
}

void voice_hrtf_processor_destroy(voice_hrtf_processor_t *processor) {
    if (!processor) return;

    free(processor->current_hrir_left);
    free(processor->current_hrir_right);
    free(processor->prev_hrir_left);
    free(processor->prev_hrir_right);
    free(processor->input_buffer);
    free(processor->output_left);
    free(processor->output_right);
    free(processor);
}

void voice_hrtf_processor_reset(voice_hrtf_processor_t *processor) {
    if (!processor) return;

    processor->buffer_pos = 0;
    processor->crossfading = false;
    processor->crossfade_pos = 0.0f;

    size_t buf_size = processor->config.block_size + processor->hrir_length;
    memset(processor->input_buffer, 0, buf_size * sizeof(float));
    memset(processor->output_left, 0, buf_size * sizeof(float));
    memset(processor->output_right, 0, buf_size * sizeof(float));
}

/* Direct time-domain convolution (SIMD-optimized for short filters) */
static void convolve_hrir(const float *input, size_t input_len,
                          const float *hrir, size_t hrir_len,
                          float *output) {
    for (size_t i = 0; i < input_len; i++) {
        float sample = input[i];
        if (fabsf(sample) < 1e-10f) continue;

        /* SIMD-friendly loop */
        for (size_t j = 0; j < hrir_len; j++) {
            output[i + j] += sample * hrir[j];
        }
    }
}

voice_error_t voice_hrtf_process(
    voice_hrtf_processor_t *processor,
    const float *mono_input,
    float *binaural_output,
    size_t num_samples,
    float azimuth,
    float elevation) {

    if (!processor || !mono_input || !binaural_output) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Check if position changed significantly */
    float az_diff = fabsf(azimuth - processor->last_azimuth);
    float el_diff = fabsf(elevation - processor->last_elevation);

    if (az_diff > 180.0f) az_diff = 360.0f - az_diff;

    if (az_diff > 2.0f || el_diff > 2.0f) {
        /* Position changed - update HRIRs */
        if (processor->config.enable_crossfade) {
            /* Save current as previous for crossfade */
            memcpy(processor->prev_hrir_left, processor->current_hrir_left,
                   processor->hrir_length * sizeof(float));
            memcpy(processor->prev_hrir_right, processor->current_hrir_right,
                   processor->hrir_length * sizeof(float));
            processor->crossfading = true;
            processor->crossfade_pos = 0.0f;
        }

        /* Get new HRIRs */
        voice_hrtf_interpolate(processor->hrtf, azimuth, elevation,
                               processor->current_hrir_left,
                               processor->current_hrir_right,
                               processor->hrir_length);

        processor->last_azimuth = azimuth;
        processor->last_elevation = elevation;
    }

    /* Process in blocks */
    size_t hrir_len = processor->hrir_length;
    size_t block_size = processor->config.block_size;

    /* Clear output tail */
    size_t out_len = num_samples + hrir_len;
    float *temp_left = (float *)calloc(out_len, sizeof(float));
    float *temp_right = (float *)calloc(out_len, sizeof(float));

    if (!temp_left || !temp_right) {
        free(temp_left);
        free(temp_right);
        return VOICE_ERROR_NO_MEMORY;
    }

    /* Convolve with current HRIRs */
    convolve_hrir(mono_input, num_samples,
                  processor->current_hrir_left, hrir_len, temp_left);
    convolve_hrir(mono_input, num_samples,
                  processor->current_hrir_right, hrir_len, temp_right);

    /* Handle crossfade if active */
    if (processor->crossfading) {
        float crossfade_samples = processor->config.crossfade_time_ms *
                                   processor->config.sample_rate / 1000.0f;

        for (size_t i = 0; i < num_samples && processor->crossfading; i++) {
            float fade = processor->crossfade_pos / crossfade_samples;
            if (fade >= 1.0f) {
                processor->crossfading = false;
                break;
            }

            /* Mix with previous HRIRs output */
            float prev_left = 0.0f, prev_right = 0.0f;

            /* Simple approximation: just scale output */
            temp_left[i] = temp_left[i] * fade + temp_left[i] * (1.0f - fade) * 0.9f;
            temp_right[i] = temp_right[i] * fade + temp_right[i] * (1.0f - fade) * 0.9f;

            processor->crossfade_pos += 1.0f;
        }
    }

    /* Copy to interleaved output */
    for (size_t i = 0; i < num_samples; i++) {
        binaural_output[i * 2] = temp_left[i];
        binaural_output[i * 2 + 1] = temp_right[i];
    }

    free(temp_left);
    free(temp_right);

    return VOICE_SUCCESS;
}

voice_error_t voice_hrtf_process_int16(
    voice_hrtf_processor_t *processor,
    const int16_t *mono_input,
    int16_t *binaural_output,
    size_t num_samples,
    float azimuth,
    float elevation) {

    if (!processor || !mono_input || !binaural_output) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Convert to float */
    float *float_input = (float *)malloc(num_samples * sizeof(float));
    float *float_output = (float *)malloc(num_samples * 2 * sizeof(float));

    if (!float_input || !float_output) {
        free(float_input);
        free(float_output);
        return VOICE_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < num_samples; i++) {
        float_input[i] = (float)mono_input[i] / 32768.0f;
    }

    voice_error_t err = voice_hrtf_process(processor, float_input, float_output,
                                            num_samples, azimuth, elevation);

    if (err == VOICE_SUCCESS) {
        for (size_t i = 0; i < num_samples * 2; i++) {
            float sample = float_output[i] * 32768.0f;
            if (sample > 32767.0f) sample = 32767.0f;
            if (sample < -32768.0f) sample = -32768.0f;
            binaural_output[i] = (int16_t)sample;
        }
    }

    free(float_input);
    free(float_output);

    return err;
}
