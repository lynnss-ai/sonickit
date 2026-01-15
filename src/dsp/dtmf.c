/**
 * @file dtmf.c
 * @brief DTMF detection and generation implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 使用 Goertzel 算法进行 DTMF 检测
 */

#include "dsp/dtmf.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * DTMF 频率表
 * ============================================ */

/* 低频组 (行) */
static const float DTMF_LOW_FREQ[4] = {
    697.0f, 770.0f, 852.0f, 941.0f
};

/* 高频组 (列) */
static const float DTMF_HIGH_FREQ[4] = {
    1209.0f, 1336.0f, 1477.0f, 1633.0f
};

/* DTMF 键盘布局 */
static const char DTMF_KEYS[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

/* ============================================
 * Goertzel 状态 - SIMD 友好布局
 * ============================================ */

typedef struct {
    float coeff;  /* Single filter coefficient */
    float s0, s1, s2;
} goertzel_state_t;

/* SIMD-friendly bank of 4 Goertzel filters */
typedef struct {
    float coeff[4];  /* 4 coefficients packed for SSE */
    float s1[4];     /* 4 s1 states packed for SSE */
    float s2[4];     /* 4 s2 states packed for SSE */
} goertzel_bank_t;

/* ============================================
 * 检测器结构
 * ============================================ */

#define DTMF_DIGIT_BUFFER_SIZE 64

struct voice_dtmf_detector_s {
    voice_dtmf_detector_config_t config;

    /* SIMD-optimized Goertzel filter banks */
    goertzel_bank_t low_bank;   /* 4 low frequency filters */
    goertzel_bank_t high_bank;  /* 4 high frequency filters */

    /* Legacy single filter pointers for compatibility */
    goertzel_state_t low_filters[4];
    goertzel_state_t high_filters[4];

    /* 检测状态 */
    voice_dtmf_digit_t current_digit;
    uint32_t on_samples;
    uint32_t off_samples;
    uint32_t min_on_samples;
    uint32_t min_off_samples;
    bool digit_active;

    /* 数字缓冲区 */
    char digit_buffer[DTMF_DIGIT_BUFFER_SIZE];
    size_t digit_count;

    /* 采样计数 */
    size_t sample_count;
    size_t block_size;
};

/* ============================================
 * 生成器结构
 * ============================================ */

struct voice_dtmf_generator_s {
    voice_dtmf_generator_config_t config;

    /* 相位累加器 */
    float low_phase;
    float high_phase;
    float low_phase_inc;
    float high_phase_inc;

    /* 状态 */
    size_t samples_remaining;
    size_t pause_remaining;
    bool in_pause;
};

/* ============================================
 * Goertzel 算法
 * ============================================ */

static void goertzel_init(goertzel_state_t *state, float freq, uint32_t sample_rate, size_t block_size) {
    float k = 0.5f + ((float)block_size * freq) / (float)sample_rate;
    state->coeff = 2.0f * cosf(2.0f * (float)M_PI * k / (float)block_size);
    state->s0 = state->s1 = state->s2 = 0.0f;
}

static void goertzel_reset(goertzel_state_t *state) {
    state->s0 = state->s1 = state->s2 = 0.0f;
}

static void goertzel_process(goertzel_state_t *state, float sample) {
    state->s0 = sample + state->coeff * state->s1 - state->s2;
    state->s2 = state->s1;
    state->s1 = state->s0;
}

static float goertzel_get_power(goertzel_state_t *state) {
    return state->s1 * state->s1 + state->s2 * state->s2 -
           state->coeff * state->s1 * state->s2;
}

/* SIMD-optimized bank initialization */
static void goertzel_bank_init(goertzel_bank_t *bank, const float *freqs,
                                uint32_t sample_rate, size_t block_size) {
    for (int i = 0; i < 4; i++) {
        float k = 0.5f + ((float)block_size * freqs[i]) / (float)sample_rate;
        bank->coeff[i] = 2.0f * cosf(2.0f * (float)M_PI * k / (float)block_size);
        bank->s1[i] = 0.0f;
        bank->s2[i] = 0.0f;
    }
}

static void goertzel_bank_reset(goertzel_bank_t *bank) {
    for (int i = 0; i < 4; i++) {
        bank->s1[i] = 0.0f;
        bank->s2[i] = 0.0f;
    }
}

#if defined(__SSE__) || defined(_M_IX86) || defined(_M_X64)
#include <xmmintrin.h>

/* SSE-optimized: process 4 Goertzel filters in parallel */
static inline void goertzel_bank_process_sse(goertzel_bank_t *bank, float sample) {
    __m128 s1 = _mm_loadu_ps(bank->s1);
    __m128 s2 = _mm_loadu_ps(bank->s2);
    __m128 coeff = _mm_loadu_ps(bank->coeff);
    __m128 samp = _mm_set1_ps(sample);

    /* s0 = sample + coeff * s1 - s2 */
    __m128 s0 = _mm_add_ps(samp, _mm_sub_ps(_mm_mul_ps(coeff, s1), s2));

    /* s2 = s1, s1 = s0 */
    _mm_storeu_ps(bank->s2, s1);
    _mm_storeu_ps(bank->s1, s0);
}

/* SSE-optimized: get power from 4 filters */
static inline void goertzel_bank_get_power_sse(goertzel_bank_t *bank, float *power) {
    __m128 s1 = _mm_loadu_ps(bank->s1);
    __m128 s2 = _mm_loadu_ps(bank->s2);
    __m128 coeff = _mm_loadu_ps(bank->coeff);

    /* power = s1*s1 + s2*s2 - coeff*s1*s2 */
    __m128 s1_sq = _mm_mul_ps(s1, s1);
    __m128 s2_sq = _mm_mul_ps(s2, s2);
    __m128 s1s2 = _mm_mul_ps(s1, s2);
    __m128 result = _mm_sub_ps(_mm_add_ps(s1_sq, s2_sq), _mm_mul_ps(coeff, s1s2));

    _mm_storeu_ps(power, result);
}

#define GOERTZEL_BANK_PROCESS(bank, sample) goertzel_bank_process_sse(bank, sample)
#define GOERTZEL_BANK_GET_POWER(bank, power) goertzel_bank_get_power_sse(bank, power)

#else
/* Scalar fallback */
static inline void goertzel_bank_process_scalar(goertzel_bank_t *bank, float sample) {
    for (int i = 0; i < 4; i++) {
        float s0 = sample + bank->coeff[i] * bank->s1[i] - bank->s2[i];
        bank->s2[i] = bank->s1[i];
        bank->s1[i] = s0;
    }
}

static inline void goertzel_bank_get_power_scalar(goertzel_bank_t *bank, float *power) {
    for (int i = 0; i < 4; i++) {
        power[i] = bank->s1[i] * bank->s1[i] + bank->s2[i] * bank->s2[i] -
                   bank->coeff[i] * bank->s1[i] * bank->s2[i];
    }
}

#define GOERTZEL_BANK_PROCESS(bank, sample) goertzel_bank_process_scalar(bank, sample)
#define GOERTZEL_BANK_GET_POWER(bank, power) goertzel_bank_get_power_scalar(bank, power)

#endif

/* ============================================
 * 检测器配置
 * ============================================ */

void voice_dtmf_detector_config_init(voice_dtmf_detector_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_dtmf_detector_config_t));

    config->sample_rate = 8000;
    config->frame_size = 160;           /* 20ms */

    config->detection_threshold = 100.0f;
    config->twist_threshold = 6.0f;     /* dB */
    config->reverse_twist_threshold = 8.0f;

    config->min_on_time_ms = 40;
    config->min_off_time_ms = 40;
}

/* ============================================
 * 检测器实现
 * ============================================ */

voice_dtmf_detector_t *voice_dtmf_detector_create(
    const voice_dtmf_detector_config_t *config
) {
    voice_dtmf_detector_t *det = (voice_dtmf_detector_t *)calloc(1, sizeof(voice_dtmf_detector_t));
    if (!det) return NULL;

    if (config) {
        det->config = *config;
    } else {
        voice_dtmf_detector_config_init(&det->config);
    }

    /* 计算块大小 (约 10ms) */
    det->block_size = det->config.sample_rate / 100;

    /* Initialize SIMD-optimized Goertzel filter banks */
    goertzel_bank_init(&det->low_bank, DTMF_LOW_FREQ,
                       det->config.sample_rate, det->block_size);
    goertzel_bank_init(&det->high_bank, DTMF_HIGH_FREQ,
                       det->config.sample_rate, det->block_size);

    /* Also initialize legacy filters for compatibility */
    for (int i = 0; i < 4; i++) {
        goertzel_init(&det->low_filters[i], DTMF_LOW_FREQ[i],
                      det->config.sample_rate, det->block_size);
        goertzel_init(&det->high_filters[i], DTMF_HIGH_FREQ[i],
                      det->config.sample_rate, det->block_size);
    }

    /* 计算最小样本数 */
    det->min_on_samples = det->config.min_on_time_ms * det->config.sample_rate / 1000;
    det->min_off_samples = det->config.min_off_time_ms * det->config.sample_rate / 1000;

    det->current_digit = VOICE_DTMF_NONE;

    return det;
}

void voice_dtmf_detector_destroy(voice_dtmf_detector_t *detector) {
    if (detector) {
        free(detector);
    }
}

voice_dtmf_digit_t voice_dtmf_detector_process(
    voice_dtmf_detector_t *detector,
    const int16_t *samples,
    size_t num_samples,
    voice_dtmf_result_t *result
) {
    if (!detector || !samples) {
        return VOICE_DTMF_NONE;
    }

    voice_dtmf_digit_t detected = VOICE_DTMF_NONE;

    for (size_t i = 0; i < num_samples; i++) {
        float sample = (float)samples[i] / 32768.0f;

        /* Update all Goertzel filters using SIMD (4 filters at once) */
        GOERTZEL_BANK_PROCESS(&detector->low_bank, sample);
        GOERTZEL_BANK_PROCESS(&detector->high_bank, sample);

        detector->sample_count++;

        /* Detect at end of each block */
        if (detector->sample_count >= detector->block_size) {
            /* Get power from all 8 filters using SIMD */
            float low_power[4], high_power[4];
            GOERTZEL_BANK_GET_POWER(&detector->low_bank, low_power);
            GOERTZEL_BANK_GET_POWER(&detector->high_bank, high_power);

            float max_low = 0.0f, max_high = 0.0f;
            int low_idx = -1, high_idx = -1;

            for (int j = 0; j < 4; j++) {
                if (low_power[j] > max_low) {
                    max_low = low_power[j];
                    low_idx = j;
                }
                if (high_power[j] > max_high) {
                    max_high = high_power[j];
                    high_idx = j;
                }
            }

            /* Reset filter banks */
            goertzel_bank_reset(&detector->low_bank);
            goertzel_bank_reset(&detector->high_bank);

            detector->sample_count = 0;

            /* 检查阈值 */
            if (max_low > detector->config.detection_threshold &&
                max_high > detector->config.detection_threshold &&
                low_idx >= 0 && high_idx >= 0) {

                /* 检查 twist */
                float twist = 10.0f * log10f(max_high / max_low);

                if (twist >= -detector->config.reverse_twist_threshold &&
                    twist <= detector->config.twist_threshold) {

                    voice_dtmf_digit_t digit = DTMF_KEYS[low_idx][high_idx];

                    if (digit == detector->current_digit) {
                        detector->on_samples += detector->block_size;
                    } else {
                        detector->current_digit = digit;
                        detector->on_samples = detector->block_size;
                        detector->off_samples = 0;
                    }

                    /* 检查最小持续时间 */
                    if (detector->on_samples >= detector->min_on_samples &&
                        !detector->digit_active) {
                        detector->digit_active = true;
                        detected = digit;

                        /* 添加到缓冲区 */
                        if (detector->digit_count < DTMF_DIGIT_BUFFER_SIZE - 1) {
                            detector->digit_buffer[detector->digit_count++] = (char)digit;
                            detector->digit_buffer[detector->digit_count] = '\0';
                        }

                        /* 回调 */
                        if (detector->config.on_digit) {
                            uint32_t duration_ms = detector->on_samples * 1000 /
                                                   detector->config.sample_rate;
                            detector->config.on_digit(digit, duration_ms,
                                                     detector->config.callback_user_data);
                        }
                    }

                    if (result) {
                        result->digit = digit;
                        result->valid = true;
                        result->low_freq_energy = max_low;
                        result->high_freq_energy = max_high;
                        result->twist = twist;
                    }
                } else {
                    detector->off_samples += detector->block_size;
                }
            } else {
                detector->off_samples += detector->block_size;

                if (detector->off_samples >= detector->min_off_samples) {
                    detector->current_digit = VOICE_DTMF_NONE;
                    detector->digit_active = false;
                    detector->on_samples = 0;
                }
            }
        }
    }

    return detected;
}

void voice_dtmf_detector_reset(voice_dtmf_detector_t *detector) {
    if (!detector) return;

    for (int i = 0; i < 4; i++) {
        goertzel_reset(&detector->low_filters[i]);
        goertzel_reset(&detector->high_filters[i]);
    }

    detector->current_digit = VOICE_DTMF_NONE;
    detector->on_samples = 0;
    detector->off_samples = 0;
    detector->digit_active = false;
    detector->sample_count = 0;
}

size_t voice_dtmf_detector_get_digits(
    voice_dtmf_detector_t *detector,
    char *buffer,
    size_t buffer_size
) {
    if (!detector || !buffer || buffer_size == 0) return 0;

    size_t count = detector->digit_count;
    if (count >= buffer_size) count = buffer_size - 1;

    memcpy(buffer, detector->digit_buffer, count);
    buffer[count] = '\0';

    return count;
}

void voice_dtmf_detector_clear_digits(voice_dtmf_detector_t *detector) {
    if (!detector) return;

    detector->digit_count = 0;
    detector->digit_buffer[0] = '\0';
}

/* ============================================
 * 生成器配置
 * ============================================ */

void voice_dtmf_generator_config_init(voice_dtmf_generator_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_dtmf_generator_config_t));

    config->sample_rate = 8000;
    config->amplitude = 0.5f;
    config->low_freq_gain = 1.0f;
    config->high_freq_gain = 1.0f;
    config->tone_duration_ms = 100;
    config->pause_duration_ms = 100;
}

/* ============================================
 * 生成器实现
 * ============================================ */

voice_dtmf_generator_t *voice_dtmf_generator_create(
    const voice_dtmf_generator_config_t *config
) {
    voice_dtmf_generator_t *gen = (voice_dtmf_generator_t *)calloc(1, sizeof(voice_dtmf_generator_t));
    if (!gen) return NULL;

    if (config) {
        gen->config = *config;
    } else {
        voice_dtmf_generator_config_init(&gen->config);
    }

    return gen;
}

void voice_dtmf_generator_destroy(voice_dtmf_generator_t *generator) {
    if (generator) {
        free(generator);
    }
}

size_t voice_dtmf_generator_generate(
    voice_dtmf_generator_t *generator,
    voice_dtmf_digit_t digit,
    int16_t *output,
    size_t num_samples
) {
    if (!generator || !output || digit == VOICE_DTMF_NONE) {
        return 0;
    }

    /* 查找频率 */
    float low_freq = 0.0f, high_freq = 0.0f;
    voice_dtmf_get_frequencies(digit, &low_freq, &high_freq);

    if (low_freq == 0.0f || high_freq == 0.0f) {
        return 0;
    }

    /* 计算相位增量 */
    float low_inc = 2.0f * (float)M_PI * low_freq / generator->config.sample_rate;
    float high_inc = 2.0f * (float)M_PI * high_freq / generator->config.sample_rate;

    /* 生成样本数 */
    size_t tone_samples = generator->config.tone_duration_ms * generator->config.sample_rate / 1000;
    if (tone_samples > num_samples) tone_samples = num_samples;

    float low_phase = 0.0f;
    float high_phase = 0.0f;

    for (size_t i = 0; i < tone_samples; i++) {
        float low_val = sinf(low_phase) * generator->config.low_freq_gain;
        float high_val = sinf(high_phase) * generator->config.high_freq_gain;

        float sample = (low_val + high_val) * 0.5f * generator->config.amplitude * 32767.0f;

        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;

        output[i] = (int16_t)sample;

        low_phase += low_inc;
        high_phase += high_inc;

        if (low_phase > 2.0f * (float)M_PI) low_phase -= 2.0f * (float)M_PI;
        if (high_phase > 2.0f * (float)M_PI) high_phase -= 2.0f * (float)M_PI;
    }

    return tone_samples;
}

size_t voice_dtmf_generator_generate_sequence(
    voice_dtmf_generator_t *generator,
    const char *digits,
    int16_t *output,
    size_t max_samples
) {
    if (!generator || !digits || !output) {
        return 0;
    }

    size_t total = 0;
    size_t tone_samples = generator->config.tone_duration_ms * generator->config.sample_rate / 1000;
    size_t pause_samples = generator->config.pause_duration_ms * generator->config.sample_rate / 1000;

    for (const char *p = digits; *p != '\0'; p++) {
        if (!voice_dtmf_is_valid_digit(*p)) continue;

        /* 检查空间 */
        if (total + tone_samples + pause_samples > max_samples) break;

        /* 生成音调 */
        size_t generated = voice_dtmf_generator_generate(
            generator,
            (voice_dtmf_digit_t)*p,
            output + total,
            tone_samples
        );
        total += generated;

        /* 添加静音间隔 */
        if (total + pause_samples <= max_samples) {
            memset(output + total, 0, pause_samples * sizeof(int16_t));
            total += pause_samples;
        }
    }

    return total;
}

void voice_dtmf_generator_reset(voice_dtmf_generator_t *generator) {
    if (!generator) return;

    generator->low_phase = 0.0f;
    generator->high_phase = 0.0f;
    generator->samples_remaining = 0;
    generator->pause_remaining = 0;
    generator->in_pause = false;
}

/* ============================================
 * 辅助函数
 * ============================================ */

bool voice_dtmf_is_valid_digit(char c) {
    return (c >= '0' && c <= '9') ||
           c == '*' || c == '#' ||
           (c >= 'A' && c <= 'D') ||
           (c >= 'a' && c <= 'd');
}

void voice_dtmf_get_frequencies(
    voice_dtmf_digit_t digit,
    float *low_freq,
    float *high_freq
) {
    if (!low_freq || !high_freq) return;

    *low_freq = 0.0f;
    *high_freq = 0.0f;

    /* 查找键位 */
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            char key = DTMF_KEYS[row][col];
            char d = (char)digit;

            /* 处理大小写 */
            if (d >= 'a' && d <= 'd') d -= 32;

            if (key == d) {
                *low_freq = DTMF_LOW_FREQ[row];
                *high_freq = DTMF_HIGH_FREQ[col];
                return;
            }
        }
    }
}
