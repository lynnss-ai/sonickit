/**
 * @file equalizer.c
 * @brief Multi-band parametric equalizer implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 使用 Biquad 滤波器实现各类均衡器
 */

#include "dsp/equalizer.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Biquad 滤波器系数
 * ============================================ */

typedef struct {
    float b0, b1, b2;   /* 分子系数 */
    float a1, a2;       /* 分母系数 (a0 = 1) */
} biquad_coeffs_t;

typedef struct {
    float z1, z2;       /* 状态变量 */
} biquad_state_t;

/* ============================================
 * 内部结构
 * ============================================ */

#define MAX_EQ_BANDS 10

struct voice_eq_s {
    uint32_t sample_rate;
    uint32_t num_bands;

    voice_eq_band_t bands[MAX_EQ_BANDS];
    biquad_coeffs_t coeffs[MAX_EQ_BANDS];
    biquad_state_t states[MAX_EQ_BANDS];

    float master_gain;  /* 线性 */
};

/* ============================================
 * Biquad 系数计算
 * ============================================ */

static void compute_biquad_coeffs(
    biquad_coeffs_t *c,
    voice_eq_filter_type_t type,
    float freq,
    float gain_db,
    float q,
    uint32_t sample_rate
) {
    float A = powf(10.0f, gain_db / 40.0f);  /* sqrt of amplitude */
    float w0 = 2.0f * (float)M_PI * freq / (float)sample_rate;
    float cos_w0 = cosf(w0);
    float sin_w0 = sinf(w0);
    float alpha = sin_w0 / (2.0f * q);

    float b0, b1, b2, a0, a1, a2;

    switch (type) {
        case VOICE_EQ_LOWPASS:
            b0 = (1.0f - cos_w0) / 2.0f;
            b1 = 1.0f - cos_w0;
            b2 = b0;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;

        case VOICE_EQ_HIGHPASS:
            b0 = (1.0f + cos_w0) / 2.0f;
            b1 = -(1.0f + cos_w0);
            b2 = b0;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;

        case VOICE_EQ_BANDPASS:
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_w0;
            a2 = 1.0f - alpha;
            break;

        case VOICE_EQ_NOTCH:
            b0 = 1.0f;
            b1 = -2.0f * cos_w0;
            b2 = 1.0f;
            a0 = 1.0f + alpha;
            a1 = b1;
            a2 = 1.0f - alpha;
            break;

        case VOICE_EQ_PEAK:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cos_w0;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = b1;
            a2 = 1.0f - alpha / A;
            break;

        case VOICE_EQ_LOWSHELF:
            {
                float sq_A = sqrtf(A);
                b0 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sq_A * alpha);
                b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                b2 = A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sq_A * alpha);
                a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sq_A * alpha;
                a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                a2 = (A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sq_A * alpha;
            }
            break;

        case VOICE_EQ_HIGHSHELF:
            {
                float sq_A = sqrtf(A);
                b0 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * sq_A * alpha);
                b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0);
                b2 = A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * sq_A * alpha);
                a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * sq_A * alpha;
                a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0);
                a2 = (A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * sq_A * alpha;
            }
            break;

        default:
            b0 = b1 = b2 = a0 = a1 = a2 = 0.0f;
            b0 = a0 = 1.0f;
            break;
    }

    /* 归一化 */
    c->b0 = b0 / a0;
    c->b1 = b1 / a0;
    c->b2 = b2 / a0;
    c->a1 = a1 / a0;
    c->a2 = a2 / a0;
}

static float process_biquad(biquad_coeffs_t *c, biquad_state_t *s, float x) {
    float y = c->b0 * x + s->z1;
    s->z1 = c->b1 * x - c->a1 * y + s->z2;
    s->z2 = c->b2 * x - c->a2 * y;
    return y;
}

/* ============================================
 * 配置初始化
 * ============================================ */

void voice_eq_config_init(voice_eq_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_eq_config_t));
    config->sample_rate = 16000;
    config->num_bands = 0;
    config->master_gain_db = 0.0f;
}

voice_error_t voice_eq_config_from_preset(
    voice_eq_config_t *config,
    voice_eq_preset_t preset,
    uint32_t sample_rate
) {
    if (!config) return VOICE_ERROR_INVALID_PARAM;

    voice_eq_config_init(config);
    config->sample_rate = sample_rate;

    static voice_eq_band_t preset_bands[8];

    switch (preset) {
        case VOICE_EQ_PRESET_FLAT:
            config->num_bands = 0;
            break;

        case VOICE_EQ_PRESET_VOICE_ENHANCE:
            config->num_bands = 3;
            preset_bands[0] = (voice_eq_band_t){true, VOICE_EQ_HIGHPASS, 80.0f, 0.0f, 0.7f};
            preset_bands[1] = (voice_eq_band_t){true, VOICE_EQ_PEAK, 2500.0f, 3.0f, 1.5f};
            preset_bands[2] = (voice_eq_band_t){true, VOICE_EQ_LOWPASS, 7000.0f, 0.0f, 0.7f};
            config->bands = preset_bands;
            break;

        case VOICE_EQ_PRESET_TELEPHONE:
            config->num_bands = 2;
            preset_bands[0] = (voice_eq_band_t){true, VOICE_EQ_HIGHPASS, 300.0f, 0.0f, 0.7f};
            preset_bands[1] = (voice_eq_band_t){true, VOICE_EQ_LOWPASS, 3400.0f, 0.0f, 0.7f};
            config->bands = preset_bands;
            break;

        case VOICE_EQ_PRESET_BASS_BOOST:
            config->num_bands = 1;
            preset_bands[0] = (voice_eq_band_t){true, VOICE_EQ_LOWSHELF, 200.0f, 6.0f, 0.7f};
            config->bands = preset_bands;
            break;

        case VOICE_EQ_PRESET_TREBLE_BOOST:
            config->num_bands = 1;
            preset_bands[0] = (voice_eq_band_t){true, VOICE_EQ_HIGHSHELF, 4000.0f, 6.0f, 0.7f};
            config->bands = preset_bands;
            break;

        case VOICE_EQ_PRESET_REDUCE_NOISE:
            config->num_bands = 2;
            preset_bands[0] = (voice_eq_band_t){true, VOICE_EQ_HIGHPASS, 100.0f, 0.0f, 0.7f};
            preset_bands[1] = (voice_eq_band_t){true, VOICE_EQ_HIGHSHELF, 4000.0f, -6.0f, 0.7f};
            config->bands = preset_bands;
            break;

        case VOICE_EQ_PRESET_CLARITY:
            config->num_bands = 2;
            preset_bands[0] = (voice_eq_band_t){true, VOICE_EQ_PEAK, 2000.0f, 4.0f, 1.0f};
            preset_bands[1] = (voice_eq_band_t){true, VOICE_EQ_HIGHSHELF, 6000.0f, 3.0f, 0.7f};
            config->bands = preset_bands;
            break;

        default:
            break;
    }

    return VOICE_SUCCESS;
}

/* ============================================
 * 生命周期
 * ============================================ */

voice_eq_t *voice_eq_create(const voice_eq_config_t *config) {
    voice_eq_t *eq = (voice_eq_t *)calloc(1, sizeof(voice_eq_t));
    if (!eq) return NULL;

    if (config) {
        eq->sample_rate = config->sample_rate;
        eq->master_gain = powf(10.0f, config->master_gain_db / 20.0f);

        uint32_t num_bands = config->num_bands;
        if (num_bands > MAX_EQ_BANDS) num_bands = MAX_EQ_BANDS;
        eq->num_bands = num_bands;

        for (uint32_t i = 0; i < num_bands && config->bands; i++) {
            eq->bands[i] = config->bands[i];

            compute_biquad_coeffs(
                &eq->coeffs[i],
                eq->bands[i].type,
                eq->bands[i].frequency,
                eq->bands[i].gain_db,
                eq->bands[i].q,
                eq->sample_rate
            );
        }
    } else {
        eq->sample_rate = 16000;
        eq->master_gain = 1.0f;
    }

    return eq;
}

void voice_eq_destroy(voice_eq_t *eq) {
    if (eq) {
        free(eq);
    }
}

/* ============================================
 * 处理
 * ============================================ */

voice_error_t voice_eq_process(
    voice_eq_t *eq,
    int16_t *samples,
    size_t num_samples
) {
    if (!eq || !samples) return VOICE_ERROR_INVALID_PARAM;

    /* 分配临时浮点缓冲区 */
    float *float_buf = (float *)voice_aligned_alloc(num_samples * sizeof(float), 64);
    if (!float_buf) {
        /* 回退到标量处理 */
        for (size_t i = 0; i < num_samples; i++) {
            float x = (float)samples[i] / 32768.0f;
            for (uint32_t b = 0; b < eq->num_bands; b++) {
                if (eq->bands[b].enabled) {
                    x = process_biquad(&eq->coeffs[b], &eq->states[b], x);
                }
            }
            x *= eq->master_gain;
            if (x > 1.0f) x = 1.0f;
            if (x < -1.0f) x = -1.0f;
            samples[i] = (int16_t)(x * 32767.0f);
        }
        return VOICE_SUCCESS;
    }

    /* SIMD 批量转换: int16 -> float */
    voice_int16_to_float(samples, float_buf, num_samples);

    /* IIR 滤波器处理 (样本依赖，无法向量化) */
    for (size_t i = 0; i < num_samples; i++) {
        float x = float_buf[i];
        for (uint32_t b = 0; b < eq->num_bands; b++) {
            if (eq->bands[b].enabled) {
                x = process_biquad(&eq->coeffs[b], &eq->states[b], x);
            }
        }
        float_buf[i] = x;
    }

    /* SIMD 批量应用主增益 */
    voice_apply_gain_float(float_buf, eq->master_gain, num_samples);

    /* SIMD 批量转换: float -> int16 (包含限制) */
    voice_float_to_int16(float_buf, samples, num_samples);

    voice_aligned_free(float_buf);
    return VOICE_SUCCESS;
}

voice_error_t voice_eq_process_float(
    voice_eq_t *eq,
    float *samples,
    size_t num_samples
) {
    if (!eq || !samples) return VOICE_ERROR_INVALID_PARAM;

    for (size_t i = 0; i < num_samples; i++) {
        float x = samples[i];

        for (uint32_t b = 0; b < eq->num_bands; b++) {
            if (eq->bands[b].enabled) {
                x = process_biquad(&eq->coeffs[b], &eq->states[b], x);
            }
        }

        samples[i] = x * eq->master_gain;
    }

    return VOICE_SUCCESS;
}

/* ============================================
 * 频段控制
 * ============================================ */

voice_error_t voice_eq_set_band(
    voice_eq_t *eq,
    uint32_t band_index,
    const voice_eq_band_t *band
) {
    if (!eq || !band || band_index >= eq->num_bands) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    eq->bands[band_index] = *band;

    compute_biquad_coeffs(
        &eq->coeffs[band_index],
        band->type,
        band->frequency,
        band->gain_db,
        band->q,
        eq->sample_rate
    );

    /* 重置状态 */
    eq->states[band_index].z1 = 0.0f;
    eq->states[band_index].z2 = 0.0f;

    return VOICE_SUCCESS;
}

voice_error_t voice_eq_get_band(
    voice_eq_t *eq,
    uint32_t band_index,
    voice_eq_band_t *band
) {
    if (!eq || !band || band_index >= eq->num_bands) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    *band = eq->bands[band_index];
    return VOICE_SUCCESS;
}

voice_error_t voice_eq_enable_band(
    voice_eq_t *eq,
    uint32_t band_index,
    bool enable
) {
    if (!eq || band_index >= eq->num_bands) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    eq->bands[band_index].enabled = enable;
    return VOICE_SUCCESS;
}

voice_error_t voice_eq_set_master_gain(voice_eq_t *eq, float gain_db) {
    if (!eq) return VOICE_ERROR_INVALID_PARAM;

    eq->master_gain = powf(10.0f, gain_db / 20.0f);
    return VOICE_SUCCESS;
}

voice_error_t voice_eq_apply_preset(
    voice_eq_t *eq,
    voice_eq_preset_t preset
) {
    if (!eq) return VOICE_ERROR_INVALID_PARAM;

    voice_eq_config_t config;
    voice_eq_config_from_preset(&config, preset, eq->sample_rate);

    eq->num_bands = config.num_bands;

    for (uint32_t i = 0; i < config.num_bands && config.bands; i++) {
        eq->bands[i] = config.bands[i];
        compute_biquad_coeffs(
            &eq->coeffs[i],
            eq->bands[i].type,
            eq->bands[i].frequency,
            eq->bands[i].gain_db,
            eq->bands[i].q,
            eq->sample_rate
        );
    }

    voice_eq_reset(eq);

    return VOICE_SUCCESS;
}

/* ============================================
 * 状态
 * ============================================ */

voice_error_t voice_eq_get_frequency_response(
    voice_eq_t *eq,
    const float *frequencies,
    float *responses,
    size_t count
) {
    if (!eq || !frequencies || !responses) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < count; i++) {
        float w = 2.0f * (float)M_PI * frequencies[i] / (float)eq->sample_rate;
        float cos_w = cosf(w);
        float cos_2w = cosf(2.0f * w);
        float sin_w = sinf(w);
        float sin_2w = sinf(2.0f * w);

        float magnitude = 1.0f;

        for (uint32_t b = 0; b < eq->num_bands; b++) {
            if (!eq->bands[b].enabled) continue;

            biquad_coeffs_t *c = &eq->coeffs[b];

            /* H(e^jw) = (b0 + b1*e^-jw + b2*e^-2jw) / (1 + a1*e^-jw + a2*e^-2jw) */
            float num_re = c->b0 + c->b1 * cos_w + c->b2 * cos_2w;
            float num_im = -(c->b1 * sin_w + c->b2 * sin_2w);
            float den_re = 1.0f + c->a1 * cos_w + c->a2 * cos_2w;
            float den_im = -(c->a1 * sin_w + c->a2 * sin_2w);

            float num_mag = sqrtf(num_re * num_re + num_im * num_im);
            float den_mag = sqrtf(den_re * den_re + den_im * den_im);

            if (den_mag > 0.0001f) {
                magnitude *= num_mag / den_mag;
            }
        }

        magnitude *= eq->master_gain;

        /* 转换为 dB */
        responses[i] = 20.0f * log10f(magnitude > 0.0001f ? magnitude : 0.0001f);
    }

    return VOICE_SUCCESS;
}

void voice_eq_reset(voice_eq_t *eq) {
    if (!eq) return;

    for (uint32_t i = 0; i < MAX_EQ_BANDS; i++) {
        eq->states[i].z1 = 0.0f;
        eq->states[i].z2 = 0.0f;
    }
}
