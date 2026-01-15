/**
 * @file time_stretcher.c
 * @brief WSOLA Time Stretcher Implementation
 *
 * Implements Waveform Similarity Overlap-Add algorithm for
 * time-scale modification without pitch change.
 */

#include "dsp/time_stretcher.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* SIMD intrinsics */
#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
    #if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        #ifdef __AVX2__
            #include <immintrin.h>
        #elif defined(__SSE2__)
            #include <emmintrin.h>
        #endif
    #elif defined(__ARM_NEON) || defined(__ARM_NEON__)
        #include <arm_neon.h>
    #endif
#endif

/* Constants */
#define TS_MIN_RATE         0.5f
#define TS_MAX_RATE         2.0f
#define TS_DEFAULT_RATE     1.0f
#define TS_MAX_CHANNELS     2
#define TS_BUFFER_MARGIN    4       /* Extra frames for safety */

/**
 * @brief Internal time stretcher structure
 */
struct voice_time_stretcher {
    voice_time_stretcher_config_t config;

    /* Derived parameters */
    uint32_t frame_samples;         /* Analysis frame size in samples */
    uint32_t overlap_samples;       /* Overlap size in samples */
    uint32_t search_samples;        /* Search range in samples */
    uint32_t hop_in;                /* Input hop size */
    uint32_t hop_out;               /* Output hop size (modified by rate) */

    /* Current state */
    float    current_rate;
    uint64_t input_total;
    uint64_t output_total;

    /* Internal buffers */
    float   *input_buffer;          /* Accumulated input samples */
    size_t   input_buffer_size;     /* Buffer capacity */
    size_t   input_fill;            /* Current fill level */

    float   *output_buffer;         /* Previous output frame for OLA */
    size_t   output_buffer_size;

    float   *window;                /* Hann window coefficients */
    float   *prev_frame;            /* Previous frame for correlation */

    /* Pre-allocated buffers for int16 conversion */
    float   *conv_input;
    float   *conv_output;
    size_t   conv_capacity;


    bool     first_frame;           /* First frame flag */
};

/* ============================================================================
 * Helper Functions
 * ========================================================================== */

/**
 * @brief Generate Hann window coefficients
 */
static void generate_hann_window(float *window, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * 3.14159265358979f * i / (length - 1)));
    }
}

/**
 * @brief Calculate cross-correlation at a given lag (SIMD optimized)
 */
static float cross_correlation(const float *a, const float *b, size_t length)
{
    /* 使用 SIMD 计算能量和点积 */
    float energy_a = voice_compute_energy_float(a, length) * (float)length;
    float energy_b = voice_compute_energy_float(b, length) * (float)length;

    /* 点积计算 - AVX2 优化 */
#if defined(__AVX2__) || defined(__AVX__)
    float sum = 0.0f;
    size_t i = 0;

    #ifdef __AVX2__
    __m256 sum_vec = _mm256_setzero_ps();
    for (; i + 8 <= length; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        sum_vec = _mm256_fmadd_ps(va, vb, sum_vec);
    }
    /* 水平归约 */
    __m128 lo = _mm256_castps256_ps128(sum_vec);
    __m128 hi = _mm256_extractf128_ps(sum_vec, 1);
    __m128 sum_128 = _mm_add_ps(lo, hi);
    sum_128 = _mm_add_ps(sum_128, _mm_shuffle_ps(sum_128, sum_128, 0x4E));
    sum_128 = _mm_add_ps(sum_128, _mm_shuffle_ps(sum_128, sum_128, 0xB1));
    sum = _mm_cvtss_f32(sum_128);
    #endif

    for (; i < length; i++) {
        sum += a[i] * b[i];
    }
#else
    float sum = 0.0f;
    for (size_t i = 0; i < length; i++) {
        sum += a[i] * b[i];
    }
#endif

    float denom = sqrtf(energy_a * energy_b);
    if (denom < 1e-10f) {
        return 0.0f;
    }

    return sum / denom;
}

/**
 * @brief Find best match offset using cross-correlation (WSOLA core)
 */
static int find_best_offset(const float *input, size_t input_len,
                            const float *prev_tail, size_t match_len,
                            int search_range)
{
    if (!prev_tail || match_len == 0) {
        return 0;
    }

    int best_offset = 0;
    float best_corr = -1.0f;

    /* Search for best matching position */
    for (int offset = -search_range; offset <= search_range; offset++) {
        int pos = offset;
        if (pos < 0 || (size_t)(pos + match_len) > input_len) {
            continue;
        }

        float corr = cross_correlation(input + pos, prev_tail, match_len);
        if (corr > best_corr) {
            best_corr = corr;
            best_offset = offset;
        }
    }

    return best_offset;
}

/**
 * @brief Overlap-add two frames (SIMD optimized)
 */
static void overlap_add(float *output, const float *prev, const float *next,
                        const float *window, size_t length)
{
#if defined(__AVX2__) || defined(__AVX__)
    size_t i = 0;
    __m256 ones = _mm256_set1_ps(1.0f);

    for (; i + 8 <= length; i += 8) {
        __m256 w = _mm256_loadu_ps(window + i);
        __m256 p = _mm256_loadu_ps(prev + i);
        __m256 n = _mm256_loadu_ps(next + i);
        __m256 one_minus_w = _mm256_sub_ps(ones, w);

        /* output = prev * (1-w) + next * w */
        __m256 result = _mm256_mul_ps(p, one_minus_w);
        result = _mm256_fmadd_ps(n, w, result);

        _mm256_storeu_ps(output + i, result);
    }

    for (; i < length; i++) {
        float w = window[i];
        output[i] = prev[i] * (1.0f - w) + next[i] * w;
    }
#elif defined(__SSE2__)
    size_t i = 0;
    __m128 ones = _mm_set1_ps(1.0f);

    for (; i + 4 <= length; i += 4) {
        __m128 w = _mm_loadu_ps(window + i);
        __m128 p = _mm_loadu_ps(prev + i);
        __m128 n = _mm_loadu_ps(next + i);
        __m128 one_minus_w = _mm_sub_ps(ones, w);

        __m128 result = _mm_add_ps(_mm_mul_ps(p, one_minus_w), _mm_mul_ps(n, w));
        _mm_storeu_ps(output + i, result);
    }

    for (; i < length; i++) {
        float w = window[i];
        output[i] = prev[i] * (1.0f - w) + next[i] * w;
    }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
    size_t i = 0;
    float32x4_t ones = vdupq_n_f32(1.0f);

    for (; i + 4 <= length; i += 4) {
        float32x4_t w = vld1q_f32(window + i);
        float32x4_t p = vld1q_f32(prev + i);
        float32x4_t n = vld1q_f32(next + i);
        float32x4_t one_minus_w = vsubq_f32(ones, w);

        float32x4_t result = vmlaq_f32(vmulq_f32(p, one_minus_w), n, w);
        vst1q_f32(output + i, result);
    }

    for (; i < length; i++) {
        float w = window[i];
        output[i] = prev[i] * (1.0f - w) + next[i] * w;
    }
#else
    for (size_t i = 0; i < length; i++) {
        float w = window[i];
        output[i] = prev[i] * (1.0f - w) + next[i] * w;
    }
#endif
}

/* ============================================================================
 * Public API Implementation
 * ========================================================================== */

void voice_time_stretcher_config_init(voice_time_stretcher_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->sample_rate = 16000;
    config->channels = 1;
    config->frame_size_ms = 20;
    config->overlap_ms = 10;
    config->search_range_ms = 15;
    config->initial_rate = TS_DEFAULT_RATE;
}

voice_time_stretcher_t *voice_time_stretcher_create(const voice_time_stretcher_config_t *config)
{
    if (!config) return NULL;
    if (config->channels < 1 || config->channels > TS_MAX_CHANNELS) return NULL;
    if (config->sample_rate < 8000 || config->sample_rate > 48000) return NULL;

    voice_time_stretcher_t *ts = calloc(1, sizeof(voice_time_stretcher_t));
    if (!ts) return NULL;

    /* Copy config */
    ts->config = *config;

    /* Calculate derived parameters */
    ts->frame_samples = (config->sample_rate * config->frame_size_ms) / 1000;
    ts->overlap_samples = (config->sample_rate * config->overlap_ms) / 1000;
    ts->search_samples = (config->sample_rate * config->search_range_ms) / 1000;
    ts->hop_in = ts->frame_samples - ts->overlap_samples;
    ts->hop_out = ts->hop_in;  /* Initially 1:1 */

    /* Ensure overlap doesn't exceed frame */
    if (ts->overlap_samples >= ts->frame_samples) {
        ts->overlap_samples = ts->frame_samples / 2;
    }

    /* Initialize rate */
    float rate = config->initial_rate;
    if (rate < TS_MIN_RATE) rate = TS_MIN_RATE;
    if (rate > TS_MAX_RATE) rate = TS_MAX_RATE;
    ts->current_rate = rate;

    /* Allocate input buffer (enough for multiple frames + search range) */
    ts->input_buffer_size = (ts->frame_samples + ts->search_samples) * TS_BUFFER_MARGIN * config->channels;
    ts->input_buffer = calloc(ts->input_buffer_size, sizeof(float));
    if (!ts->input_buffer) goto fail;

    /* Allocate conversion buffers (MTU sized margin) */
    ts->conv_capacity = ts->input_buffer_size;
    ts->conv_input = calloc(ts->conv_capacity, sizeof(float));
    ts->conv_output = calloc(ts->conv_capacity, sizeof(float));
    if (!ts->conv_input || !ts->conv_output) goto fail;

    /* Allocate output/OLA buffer */
    ts->output_buffer_size = ts->frame_samples * config->channels;
    ts->output_buffer = calloc(ts->output_buffer_size, sizeof(float));
    if (!ts->output_buffer) goto fail;

    /* Allocate window */
    ts->window = calloc(ts->overlap_samples, sizeof(float));
    if (!ts->window) goto fail;
    generate_hann_window(ts->window, ts->overlap_samples);

    /* Allocate previous frame for correlation matching */
    ts->prev_frame = calloc(ts->overlap_samples * config->channels, sizeof(float));
    if (!ts->prev_frame) goto fail;

    ts->first_frame = true;

    return ts;

fail:
    voice_time_stretcher_destroy(ts);
    return NULL;
}

void voice_time_stretcher_destroy(voice_time_stretcher_t *ts)
{
    if (!ts) return;

    free(ts->input_buffer);
    free(ts->output_buffer);
    free(ts->window);
    free(ts->prev_frame);
    free(ts->conv_input);
    free(ts->conv_output);
    free(ts);
}

voice_error_t voice_time_stretcher_reset(voice_time_stretcher_t *ts)
{
    if (!ts) return VOICE_ERROR_INVALID_PARAM;

    ts->input_fill = 0;
    ts->input_total = 0;
    ts->output_total = 0;
    ts->first_frame = true;

    if (ts->input_buffer) {
        memset(ts->input_buffer, 0, ts->input_buffer_size * sizeof(float));
    }
    if (ts->output_buffer) {
        memset(ts->output_buffer, 0, ts->output_buffer_size * sizeof(float));
    }
    if (ts->prev_frame) {
        memset(ts->prev_frame, 0, ts->overlap_samples * ts->config.channels * sizeof(float));
    }

    return VOICE_SUCCESS;
}

voice_error_t voice_time_stretcher_set_rate(voice_time_stretcher_t *ts, float rate)
{
    if (!ts) return VOICE_ERROR_INVALID_PARAM;

    if (rate < TS_MIN_RATE) rate = TS_MIN_RATE;
    if (rate > TS_MAX_RATE) rate = TS_MAX_RATE;

    ts->current_rate = rate;

    /* Recalculate output hop size based on rate */
    /* rate > 1.0 = speed up = consume more input per output frame */
    ts->hop_out = (uint32_t)(ts->hop_in / rate);
    if (ts->hop_out < 1) ts->hop_out = 1;

    return VOICE_SUCCESS;
}

float voice_time_stretcher_get_rate(voice_time_stretcher_t *ts)
{
    if (!ts) return TS_DEFAULT_RATE;
    return ts->current_rate;
}

voice_error_t voice_time_stretcher_process_float(
    voice_time_stretcher_t *ts,
    const float *input,
    size_t input_count,
    float *output,
    size_t output_capacity,
    size_t *output_count)
{
    if (!ts || !input || !output || !output_count) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    *output_count = 0;

    uint32_t channels = ts->config.channels;
    size_t samples_per_channel = input_count / channels;

    /* Append input to internal buffer */
    size_t space = ts->input_buffer_size - ts->input_fill;
    size_t to_copy = (input_count < space) ? input_count : space;
    memcpy(ts->input_buffer + ts->input_fill, input, to_copy * sizeof(float));
    ts->input_fill += to_copy;
    ts->input_total += to_copy / channels;

    size_t output_pos = 0;
    size_t frame_samples = ts->frame_samples * channels;
    size_t overlap_samples = ts->overlap_samples * channels;
    size_t hop_in_samples = ts->hop_in * channels;

    /* Process complete frames */
    while (ts->input_fill >= frame_samples + ts->search_samples * channels) {
        /* Find best matching position using WSOLA */
        int offset = 0;
        if (!ts->first_frame) {
            offset = find_best_offset(
                ts->input_buffer,
                ts->input_fill,
                ts->prev_frame,
                ts->overlap_samples,
                (int)ts->search_samples
            );
            if (offset < 0) offset = 0;
        }

        /* Check output capacity */
        if (output_pos + frame_samples > output_capacity) {
            break;
        }

        /* Get current frame */
        const float *current_frame = ts->input_buffer + (size_t)offset * channels;

        if (ts->first_frame) {
            /* First frame: copy directly */
            memcpy(output + output_pos, current_frame, frame_samples * sizeof(float));
            ts->first_frame = false;
        } else {
            /* Overlap-add with previous frame tail */
            overlap_add(
                output + output_pos,
                ts->output_buffer,
                current_frame,
                ts->window,
                overlap_samples
            );

            /* Copy non-overlapped portion */
            memcpy(
                output + output_pos + overlap_samples,
                current_frame + overlap_samples,
                (frame_samples - overlap_samples) * sizeof(float)
            );
        }

        /* Save tail for next OLA */
        memcpy(
            ts->output_buffer,
            current_frame + frame_samples - overlap_samples,
            overlap_samples * sizeof(float)
        );

        /* Save tail for correlation matching */
        memcpy(
            ts->prev_frame,
            current_frame + frame_samples - overlap_samples,
            overlap_samples * sizeof(float)
        );

        output_pos += frame_samples - overlap_samples;
        ts->output_total += (frame_samples - overlap_samples) / channels;

        /* Advance input buffer based on rate */
        size_t consume = (size_t)(hop_in_samples * ts->current_rate);
        if (consume > ts->input_fill) consume = ts->input_fill;

        memmove(ts->input_buffer, ts->input_buffer + consume,
                (ts->input_fill - consume) * sizeof(float));
        ts->input_fill -= consume;
    }

    *output_count = output_pos;
    return VOICE_SUCCESS;
}

voice_error_t voice_time_stretcher_process(
    voice_time_stretcher_t *ts,
    const int16_t *input,
    size_t input_count,
    int16_t *output,
    size_t output_capacity,
    size_t *output_count)
{
    if (!ts || !input || !output || !output_count) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Convert int16 to float */
    if (input_count > ts->conv_capacity || output_capacity > ts->conv_capacity) {
        return VOICE_ERROR_OVERFLOW;
    }

    float *float_input = ts->conv_input;
    float *float_output = ts->conv_output;

    voice_int16_to_float(input, float_input, input_count);

    /* Process */
    size_t float_output_count;
    voice_error_t err = voice_time_stretcher_process_float(
        ts, float_input, input_count,
        float_output, output_capacity, &float_output_count
    );

    if (err == VOICE_SUCCESS) {
        /* Convert float back to int16 using SIMD */
        voice_float_to_int16(float_output, output, float_output_count);
        *output_count = float_output_count;
    }

    return err;
}

size_t voice_time_stretcher_get_buffered(voice_time_stretcher_t *ts)
{
    if (!ts) return 0;
    return ts->input_fill / ts->config.channels;
}

voice_error_t voice_time_stretcher_flush(
    voice_time_stretcher_t *ts,
    int16_t *output,
    size_t output_capacity,
    size_t *output_count)
{
    if (!ts || !output || !output_count) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Output remaining buffered samples as-is */
    size_t remaining = ts->input_fill;
    if (remaining > output_capacity) {
        remaining = output_capacity;
    }

    const float scale = 32768.0f;
    for (size_t i = 0; i < remaining; i++) {
        float sample = ts->input_buffer[i] * scale;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        output[i] = (int16_t)sample;
    }

    *output_count = remaining;
    ts->input_fill = 0;

    return VOICE_SUCCESS;
}

voice_error_t voice_time_stretcher_get_state(
    voice_time_stretcher_t *ts,
    voice_time_stretcher_state_t *state)
{
    if (!ts || !state) return VOICE_ERROR_INVALID_PARAM;

    state->current_rate = ts->current_rate;
    state->input_samples = (uint32_t)ts->input_total;
    state->output_samples = (uint32_t)ts->output_total;
    state->latency_ms = (float)ts->input_fill / ts->config.channels / ts->config.sample_rate * 1000.0f;

    return VOICE_SUCCESS;
}

size_t voice_time_stretcher_get_output_size(size_t input_count, float rate)
{
    if (rate < TS_MIN_RATE) rate = TS_MIN_RATE;
    if (rate > TS_MAX_RATE) rate = TS_MAX_RATE;

    /* Output size is inversely proportional to rate, plus margin */
    size_t base_size = (size_t)(input_count / rate);
    return base_size + base_size / 4 + 256;  /* 25% margin + constant */
}
