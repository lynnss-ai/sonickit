/**
 * @file delay_estimator.c
 * @brief GCC-PHAT Delay Estimator Implementation
 *
 * Implements Generalized Cross-Correlation with Phase Transform
 * for robust delay estimation between playback and capture signals.
 */

#include "dsp/delay_estimator.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Constants */
#define DE_MAX_FRAME_SIZE       4096
#define DE_DEFAULT_HISTORY      8
#define DE_MIN_ENERGY_THRESHOLD 1e-8f
#define DE_STABILITY_VARIANCE   100.0f  /* Max variance for "stable" */

/**
 * @brief Simple FFT size calculation (next power of 2)
 */
static size_t next_power_of_2(size_t n)
{
    size_t p = 1;
    while (p < n) p <<= 1;
    return p;
}

/**
 * @brief Delay history entry
 */
typedef struct {
    int32_t delay;
    float   confidence;
} delay_history_entry_t;

/**
 * @brief Internal delay estimator structure
 */
struct voice_delay_estimator {
    voice_delay_estimator_config_t config;

    /* FFT size */
    size_t fft_size;

    /* Working buffers */
    float *ref_buffer;          /* Zero-padded reference */
    float *cap_buffer;          /* Zero-padded capture */
    float *correlation;         /* Cross-correlation result */

    /* Simple DFT buffers (no external FFT library) */
    float *ref_real;
    float *ref_imag;
    float *cap_real;
    float *cap_imag;
    float *cross_real;
    float *cross_imag;

    /* History for smoothing */
    delay_history_entry_t *history;
    size_t history_index;
    size_t history_count;

    /* Current state */
    int32_t  current_delay;
    float    smoothed_delay;
    uint64_t total_estimates;
    uint64_t valid_estimates;

    /* Last correlation (for debug) */
    float *last_correlation;
    size_t last_correlation_size;
};

/* ============================================================================
 * Simple DFT Implementation (for portability - can be replaced with FFT)
 * ========================================================================== */

/**
 * @brief Simple DFT (O(n²) - use FFT for production with large frames)
 */
static void simple_dft(const float *input, float *real, float *imag, size_t n)
{
    const float pi2 = 2.0f * 3.14159265358979f;

    for (size_t k = 0; k < n; k++) {
        real[k] = 0.0f;
        imag[k] = 0.0f;
        for (size_t t = 0; t < n; t++) {
            float angle = pi2 * k * t / n;
            real[k] += input[t] * cosf(angle);
            imag[k] -= input[t] * sinf(angle);
        }
    }
}

/**
 * @brief Simple IDFT
 */
static void simple_idft(const float *real, const float *imag, float *output, size_t n)
{
    const float pi2 = 2.0f * 3.14159265358979f;
    const float scale = 1.0f / n;

    for (size_t t = 0; t < n; t++) {
        output[t] = 0.0f;
        for (size_t k = 0; k < n; k++) {
            float angle = pi2 * k * t / n;
            output[t] += real[k] * cosf(angle) - imag[k] * sinf(angle);
        }
        output[t] *= scale;
    }
}

/**
 * @brief Fast DFT using radix-2 Cooley-Tukey (when size is power of 2)
 *
 * 使用 SIMD 优化的蝶形运算
 */
static void fft_radix2(float *real, float *imag, size_t n, int inverse)
{
    if (n <= 1) return;

    /* Bit-reversal permutation */
    size_t j = 0;
    for (size_t i = 0; i < n - 1; i++) {
        if (i < j) {
            float tr = real[i]; real[i] = real[j]; real[j] = tr;
            float ti = imag[i]; imag[i] = imag[j]; imag[j] = ti;
        }
        size_t m = n >> 1;
        while (j >= m && m > 0) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }

    /* Cooley-Tukey with SIMD butterfly */
    const float pi = 3.14159265358979f;
    float sign = inverse ? 1.0f : -1.0f;

    for (size_t step = 2; step <= n; step <<= 1) {
        float angle = sign * 2.0f * pi / (float)step;
        float wpr = cosf(angle);
        float wpi = sinf(angle);

        /* 使用 SIMD 蝶形运算 (对于 step >= 16 效果更好) */
        voice_fft_butterfly(real, imag, n, step, 1.0f, 0.0f, wpr, wpi);
    }

    if (inverse) {
        /* 使用 SIMD 缩放 */
        float scale = 1.0f / (float)n;
        voice_apply_gain_float(real, n, scale);
        voice_apply_gain_float(imag, n, scale);
    }
}

/* ============================================================================
 * GCC-PHAT Implementation
 * ========================================================================== */

/**
 * @brief Compute GCC-PHAT cross-correlation
 */
static void gcc_phat(
    voice_delay_estimator_t *de,
    const float *reference,
    const float *capture,
    size_t sample_count,
    bool use_phat)
{
    size_t n = de->fft_size;

    /* Zero-pad inputs */
    memset(de->ref_buffer, 0, n * sizeof(float));
    memset(de->cap_buffer, 0, n * sizeof(float));
    memcpy(de->ref_buffer, reference, sample_count * sizeof(float));
    memcpy(de->cap_buffer, capture, sample_count * sizeof(float));

    /* Initialize complex arrays */
    memset(de->ref_imag, 0, n * sizeof(float));
    memset(de->cap_imag, 0, n * sizeof(float));
    memcpy(de->ref_real, de->ref_buffer, n * sizeof(float));
    memcpy(de->cap_real, de->cap_buffer, n * sizeof(float));

    /* Forward FFT */
    fft_radix2(de->ref_real, de->ref_imag, n, 0);
    fft_radix2(de->cap_real, de->cap_imag, n, 0);

    /* Cross-spectrum: Cap * conj(Ref) - 使用 SIMD 优化 */
    voice_complex_mul_conj(de->cap_real, de->cap_imag,
                           de->ref_real, de->ref_imag,
                           de->cross_real, de->cross_imag, n);

    if (use_phat) {
        /* PHAT weighting: normalize by magnitude - 使用 SIMD 优化 */
        voice_complex_normalize(de->cross_real, de->cross_imag, n, DE_MIN_ENERGY_THRESHOLD);
    }

    /* Inverse FFT to get correlation */
    fft_radix2(de->cross_real, de->cross_imag, n, 1);

    /* Copy real part as correlation */
    memcpy(de->correlation, de->cross_real, n * sizeof(float));
}

/**
 * @brief Find peak in correlation
 */
static void find_correlation_peak(
    voice_delay_estimator_t *de,
    voice_delay_estimate_t *result)
{
    size_t n = de->fft_size;
    int32_t max_delay = (int32_t)de->config.max_delay_samples;
    int32_t min_delay = (int32_t)de->config.min_delay_samples;

    float max_val = -1e10f;
    int32_t max_idx = 0;

    /* Search positive delays (capture lags reference) */
    for (int32_t lag = min_delay; lag <= max_delay && (size_t)lag < n; lag++) {
        float val = de->correlation[lag];
        if (val > max_val) {
            max_val = val;
            max_idx = lag;
        }
    }

    /* Search negative delays (reference lags capture) */
    for (int32_t lag = min_delay; lag <= max_delay && (size_t)lag < n; lag++) {
        size_t idx = n - lag;
        if (idx < n) {
            float val = de->correlation[idx];
            if (val > max_val) {
                max_val = val;
                max_idx = -lag;
            }
        }
    }

    result->delay_samples = max_idx;
    result->delay_ms = (float)max_idx / de->config.sample_rate * 1000.0f;
    result->correlation_peak = max_val;

    /* Estimate confidence based on peak sharpness */
    float second_max = -1e10f;
    for (size_t i = 0; i < n; i++) {
        int32_t dist = (int32_t)i - (max_idx >= 0 ? max_idx : (int32_t)(n + max_idx));
        if (dist < 0) dist = -dist;
        if (dist > 10) {  /* At least 10 samples away */
            if (de->correlation[i] > second_max) {
                second_max = de->correlation[i];
            }
        }
    }

    if (second_max > DE_MIN_ENERGY_THRESHOLD) {
        result->confidence = max_val / (max_val + second_max);
    } else {
        result->confidence = (max_val > DE_MIN_ENERGY_THRESHOLD) ? 1.0f : 0.0f;
    }

    result->valid = (result->confidence >= de->config.confidence_threshold);
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

void voice_delay_estimator_config_init(voice_delay_estimator_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->sample_rate = 16000;
    config->frame_size = 512;
    config->max_delay_samples = 4800;   /* 300ms at 16kHz */
    config->min_delay_samples = 0;
    config->history_size = DE_DEFAULT_HISTORY;
    config->confidence_threshold = 0.3f;
    config->use_phat = true;
}

voice_delay_estimator_t *voice_delay_estimator_create(
    const voice_delay_estimator_config_t *config)
{
    if (!config) return NULL;
    if (config->frame_size > DE_MAX_FRAME_SIZE) return NULL;

    voice_delay_estimator_t *de = calloc(1, sizeof(voice_delay_estimator_t));
    if (!de) return NULL;

    de->config = *config;

    /* Calculate FFT size (must be power of 2, at least 2x frame for correlation) */
    de->fft_size = next_power_of_2(config->frame_size * 2);
    if (de->fft_size < config->max_delay_samples * 2) {
        de->fft_size = next_power_of_2(config->max_delay_samples * 2);
    }

    size_t n = de->fft_size;

    /* Allocate buffers */
    de->ref_buffer = calloc(n, sizeof(float));
    de->cap_buffer = calloc(n, sizeof(float));
    de->correlation = calloc(n, sizeof(float));
    de->ref_real = calloc(n, sizeof(float));
    de->ref_imag = calloc(n, sizeof(float));
    de->cap_real = calloc(n, sizeof(float));
    de->cap_imag = calloc(n, sizeof(float));
    de->cross_real = calloc(n, sizeof(float));
    de->cross_imag = calloc(n, sizeof(float));
    de->last_correlation = calloc(n, sizeof(float));

    if (!de->ref_buffer || !de->cap_buffer || !de->correlation ||
        !de->ref_real || !de->ref_imag || !de->cap_real || !de->cap_imag ||
        !de->cross_real || !de->cross_imag || !de->last_correlation) {
        voice_delay_estimator_destroy(de);
        return NULL;
    }

    de->last_correlation_size = n;

    /* Allocate history */
    de->history = calloc(config->history_size, sizeof(delay_history_entry_t));
    if (!de->history) {
        voice_delay_estimator_destroy(de);
        return NULL;
    }

    return de;
}

void voice_delay_estimator_destroy(voice_delay_estimator_t *de)
{
    if (!de) return;

    free(de->ref_buffer);
    free(de->cap_buffer);
    free(de->correlation);
    free(de->ref_real);
    free(de->ref_imag);
    free(de->cap_real);
    free(de->cap_imag);
    free(de->cross_real);
    free(de->cross_imag);
    free(de->last_correlation);
    free(de->history);
    free(de);
}

voice_error_t voice_delay_estimator_reset(voice_delay_estimator_t *de)
{
    if (!de) return VOICE_ERROR_INVALID_PARAM;

    de->current_delay = 0;
    de->smoothed_delay = 0.0f;
    de->total_estimates = 0;
    de->valid_estimates = 0;
    de->history_index = 0;
    de->history_count = 0;

    memset(de->history, 0, de->config.history_size * sizeof(delay_history_entry_t));

    return VOICE_SUCCESS;
}

voice_error_t voice_delay_estimator_estimate_float(
    voice_delay_estimator_t *de,
    const float *reference,
    const float *capture,
    size_t sample_count,
    voice_delay_estimate_t *result)
{
    if (!de || !reference || !capture || !result) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Check signal energy */
    float ref_energy = 0.0f, cap_energy = 0.0f;
    for (size_t i = 0; i < sample_count; i++) {
        ref_energy += reference[i] * reference[i];
        cap_energy += capture[i] * capture[i];
    }

    if (ref_energy < DE_MIN_ENERGY_THRESHOLD || cap_energy < DE_MIN_ENERGY_THRESHOLD) {
        result->delay_samples = de->current_delay;
        result->delay_ms = (float)de->current_delay / de->config.sample_rate * 1000.0f;
        result->confidence = 0.0f;
        result->correlation_peak = 0.0f;
        result->valid = false;
        return VOICE_SUCCESS;
    }

    /* Compute GCC-PHAT */
    gcc_phat(de, reference, capture, sample_count, de->config.use_phat);

    /* Find peak */
    find_correlation_peak(de, result);

    /* Save correlation for debug */
    memcpy(de->last_correlation, de->correlation, de->fft_size * sizeof(float));

    /* Update history */
    de->history[de->history_index].delay = result->delay_samples;
    de->history[de->history_index].confidence = result->confidence;
    de->history_index = (de->history_index + 1) % de->config.history_size;
    if (de->history_count < de->config.history_size) {
        de->history_count++;
    }

    /* Update smoothed estimate (weighted average by confidence) */
    if (result->valid) {
        float total_weight = 0.0f;
        float weighted_sum = 0.0f;

        for (size_t i = 0; i < de->history_count; i++) {
            if (de->history[i].confidence > de->config.confidence_threshold) {
                weighted_sum += de->history[i].delay * de->history[i].confidence;
                total_weight += de->history[i].confidence;
            }
        }

        if (total_weight > 0.0f) {
            de->smoothed_delay = weighted_sum / total_weight;
            de->current_delay = (int32_t)(de->smoothed_delay + 0.5f);
        }

        de->valid_estimates++;
    }

    de->total_estimates++;

    return VOICE_SUCCESS;
}

voice_error_t voice_delay_estimator_estimate(
    voice_delay_estimator_t *de,
    const int16_t *reference,
    const int16_t *capture,
    size_t sample_count,
    voice_delay_estimate_t *result)
{
    if (!de || !reference || !capture || !result) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Convert to float */
    float *ref_float = malloc(sample_count * sizeof(float));
    float *cap_float = malloc(sample_count * sizeof(float));
    if (!ref_float || !cap_float) {
        free(ref_float);
        free(cap_float);
        return VOICE_ERROR_NO_MEMORY;
    }

    const float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < sample_count; i++) {
        ref_float[i] = reference[i] * scale;
        cap_float[i] = capture[i] * scale;
    }

    voice_error_t err = voice_delay_estimator_estimate_float(
        de, ref_float, cap_float, sample_count, result
    );

    free(ref_float);
    free(cap_float);
    return err;
}

int32_t voice_delay_estimator_get_delay(voice_delay_estimator_t *de)
{
    if (!de) return 0;
    return de->current_delay;
}

float voice_delay_estimator_get_delay_ms(voice_delay_estimator_t *de)
{
    if (!de) return 0.0f;
    return (float)de->current_delay / de->config.sample_rate * 1000.0f;
}

voice_error_t voice_delay_estimator_set_delay(
    voice_delay_estimator_t *de,
    int32_t delay_samples)
{
    if (!de) return VOICE_ERROR_INVALID_PARAM;

    de->current_delay = delay_samples;
    de->smoothed_delay = (float)delay_samples;

    /* Fill history with this value */
    for (size_t i = 0; i < de->config.history_size; i++) {
        de->history[i].delay = delay_samples;
        de->history[i].confidence = 1.0f;
    }
    de->history_count = de->config.history_size;

    return VOICE_SUCCESS;
}

voice_error_t voice_delay_estimator_get_state(
    voice_delay_estimator_t *de,
    voice_delay_estimator_state_t *state)
{
    if (!de || !state) return VOICE_ERROR_INVALID_PARAM;

    state->current_delay = de->current_delay;
    state->average_delay_ms = de->smoothed_delay / de->config.sample_rate * 1000.0f;
    state->total_estimates = (uint32_t)de->total_estimates;
    state->valid_estimates = (uint32_t)de->valid_estimates;

    /* Calculate variance */
    if (de->history_count > 1) {
        float mean = de->smoothed_delay;
        float variance = 0.0f;
        for (size_t i = 0; i < de->history_count; i++) {
            float diff = de->history[i].delay - mean;
            variance += diff * diff;
        }
        state->delay_variance = variance / de->history_count;
    } else {
        state->delay_variance = 0.0f;
    }

    return VOICE_SUCCESS;
}

bool voice_delay_estimator_is_stable(voice_delay_estimator_t *de)
{
    if (!de || de->history_count < de->config.history_size / 2) {
        return false;
    }

    voice_delay_estimator_state_t state;
    voice_delay_estimator_get_state(de, &state);

    return state.delay_variance < DE_STABILITY_VARIANCE;
}

voice_error_t voice_delay_estimator_get_correlation(
    voice_delay_estimator_t *de,
    float *correlation,
    size_t max_lags,
    size_t *actual_lags)
{
    if (!de || !correlation || !actual_lags) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    size_t count = (max_lags < de->last_correlation_size) ? max_lags : de->last_correlation_size;
    memcpy(correlation, de->last_correlation, count * sizeof(float));
    *actual_lags = count;

    return VOICE_SUCCESS;
}
