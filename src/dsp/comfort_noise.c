/**
 * @file comfort_noise.c
 * @brief Comfort Noise Generation implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/comfort_noise.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * Random Number Generation
 * ============================================ */

static uint32_t g_cng_seed = 12345;

static uint32_t cng_rand(void) {
    g_cng_seed = g_cng_seed * 1103515245 + 12345;
    return g_cng_seed;
}

static float cng_randf(void) {
    return ((float)(cng_rand() & 0x7FFFFFFF) / (float)0x7FFFFFFF) * 2.0f - 1.0f;
}

/* ============================================
 * Internal Structure
 * ============================================ */

#define CNG_SPECTRAL_BANDS 8

struct voice_cng_s {
    voice_cng_config_t config;

    /* Noise level */
    float noise_level;              /**< Linear */
    float target_level;

    /* Spectral matching */
    float spectral_energy[CNG_SPECTRAL_BANDS];
    float filter_state[CNG_SPECTRAL_BANDS * 2];

    /* Pink noise state */
    float pink_state[7];

    /* Brown noise state */
    float brown_state;

    /* Smoothing */
    float transition_coeff;
    float current_level;
};

/* ============================================
 * 配置
 * ============================================ */

void voice_cng_config_init(voice_cng_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_cng_config_t));

    config->sample_rate = 16000;
    config->frame_size = 160;
    config->noise_type = VOICE_CNG_WHITE;

    config->noise_level_db = -50.0f;
    config->auto_level = true;

    config->transition_time_ms = 20.0f;

    config->enable_spectral_match = false;
    config->spectral_bands = CNG_SPECTRAL_BANDS;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_cng_t *voice_cng_create(const voice_cng_config_t *config) {
    voice_cng_t *cng = (voice_cng_t *)calloc(1, sizeof(voice_cng_t));
    if (!cng) return NULL;

    if (config) {
        cng->config = *config;
    } else {
        voice_cng_config_init(&cng->config);
    }

    /* Initialize level */
    cng->noise_level = powf(10.0f, cng->config.noise_level_db / 20.0f);
    cng->target_level = cng->noise_level;
    cng->current_level = cng->noise_level;

    /* Initialize spectral energy (flat) */
    for (int i = 0; i < CNG_SPECTRAL_BANDS; i++) {
        cng->spectral_energy[i] = 1.0f / CNG_SPECTRAL_BANDS;
    }

    /* 计算过渡系数 */
    float samples_per_ms = cng->config.sample_rate / 1000.0f;
    float transition_samples = cng->config.transition_time_ms * samples_per_ms;
    cng->transition_coeff = 1.0f / transition_samples;

    return cng;
}

void voice_cng_destroy(voice_cng_t *cng) {
    if (cng) {
        free(cng);
    }
}

/* ============================================
 * Noise Generation
 * ============================================ */

static float generate_white_noise(void) {
    return cng_randf();
}

static float generate_pink_noise(voice_cng_t *cng) {
    /* Voss-McCartney 算法 */
    static const float weights[7] = {
        0.99886f, 0.99332f, 0.96900f, 0.86650f,
        0.55000f, -0.7616f, 0.115926f
    };

    float white = cng_randf();
    float pink = 0.0f;

    for (int i = 0; i < 7; i++) {
        cng->pink_state[i] = weights[i] * cng->pink_state[i] +
                             (1.0f - fabsf(weights[i])) * white;
        pink += cng->pink_state[i];
    }

    return pink * 0.125f;
}

static float generate_brown_noise(voice_cng_t *cng) {
    /* 布朗噪声 = 白噪声积分 */
    float white = cng_randf();
    cng->brown_state += white * 0.02f;

    /* 限制范围 */
    if (cng->brown_state > 1.0f) cng->brown_state = 1.0f;
    if (cng->brown_state < -1.0f) cng->brown_state = -1.0f;

    return cng->brown_state;
}

/* ============================================
 * API 实现
 * ============================================ */

voice_error_t voice_cng_analyze(
    voice_cng_t *cng,
    const int16_t *samples,
    size_t num_samples
) {
    if (!cng || !samples) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Use SIMD-optimized energy computation */
    float energy = voice_compute_energy_int16(samples, num_samples);
    float rms = sqrtf(energy);

    /* 平滑更新目标电平 */
    if (cng->config.auto_level) {
        const float alpha = 0.1f;
        cng->target_level = cng->target_level * (1.0f - alpha) + rms * alpha;

        /* 限制范围 */
        float min_level = powf(10.0f, -60.0f / 20.0f);
        float max_level = powf(10.0f, -30.0f / 20.0f);

        if (cng->target_level < min_level) cng->target_level = min_level;
        if (cng->target_level > max_level) cng->target_level = max_level;
    }

    /* TODO: 频谱分析 */

    return VOICE_SUCCESS;
}

/**
 * @brief Generate comfort noise (int16 version)
 * @details Generates synthetic comfort noise with the configured characteristics.
 *          Supports multiple noise types (white, pink, brown, spectral) and
 *          smoothly transitions between level changes.
 *
 *          Algorithm:
 *          1. Smooth transition to target level
 *          2. Generate noise sample based on configured type
 *          3. Apply current level scaling
 *          4. Saturate to int16 range
 *
 * @param[in] cng CNG processor instance
 * @param[out] output Output buffer for generated noise
 * @param[in] num_samples Number of samples to generate
 * @return VOICE_SUCCESS on success, error code on failure
 *
 * @note Level transitions are smoothed per-sample to avoid artifacts.
 *       White noise: uniform distribution.
 *       Pink noise: 1/f spectrum using Voss-McCartney algorithm.
 *       Brown noise: integrated white noise (1/f² spectrum).
 */
voice_error_t voice_cng_generate(
    voice_cng_t *cng,
    int16_t *output,
    size_t num_samples
) {
    if (!cng || !output) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < num_samples; i++) {
        /* 平滑电平过渡 */
        if (cng->current_level < cng->target_level) {
            cng->current_level += cng->transition_coeff;
            if (cng->current_level > cng->target_level) {
                cng->current_level = cng->target_level;
            }
        } else if (cng->current_level > cng->target_level) {
            cng->current_level -= cng->transition_coeff;
            if (cng->current_level < cng->target_level) {
                cng->current_level = cng->target_level;
            }
        }

        /* 生成噪声 */
        float noise = 0.0f;

        switch (cng->config.noise_type) {
            case VOICE_CNG_WHITE:
                noise = generate_white_noise();
                break;
            case VOICE_CNG_PINK:
                noise = generate_pink_noise(cng);
                break;
            case VOICE_CNG_BROWN:
                noise = generate_brown_noise(cng);
                break;
            case VOICE_CNG_SPECTRAL:
                /* 基于频谱的噪声 (简化版) */
                noise = generate_pink_noise(cng);
                break;
        }

        /* 应用电平 */
        float sample = noise * cng->current_level * 32768.0f;

        /* 限制范围 */
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;

        output[i] = (int16_t)sample;
    }

    return VOICE_SUCCESS;
}

voice_error_t voice_cng_generate_float(
    voice_cng_t *cng,
    float *output,
    size_t num_samples
) {
    if (!cng || !output) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    for (size_t i = 0; i < num_samples; i++) {
        /* 平滑电平 */
        float diff = cng->target_level - cng->current_level;
        cng->current_level += diff * cng->transition_coeff;

        /* 生成噪声 */
        float noise = 0.0f;

        switch (cng->config.noise_type) {
            case VOICE_CNG_WHITE:
                noise = generate_white_noise();
                break;
            case VOICE_CNG_PINK:
                noise = generate_pink_noise(cng);
                break;
            case VOICE_CNG_BROWN:
                noise = generate_brown_noise(cng);
                break;
            case VOICE_CNG_SPECTRAL:
                noise = generate_pink_noise(cng);
                break;
        }

        output[i] = noise * cng->current_level;
    }

    return VOICE_SUCCESS;
}

voice_error_t voice_cng_set_level(voice_cng_t *cng, float level_db) {
    if (!cng) return VOICE_ERROR_INVALID_PARAM;

    cng->config.noise_level_db = level_db;
    cng->target_level = powf(10.0f, level_db / 20.0f);

    return VOICE_SUCCESS;
}

float voice_cng_get_level(voice_cng_t *cng) {
    if (!cng) return -96.0f;
    return 20.0f * log10f(cng->current_level);
}

voice_error_t voice_cng_encode_sid(
    voice_cng_t *cng,
    voice_sid_frame_t *sid
) {
    if (!cng || !sid) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    memset(sid, 0, sizeof(voice_sid_frame_t));

    /* RFC 3389: noise_level = -dBov */
    float dbov = 20.0f * log10f(cng->current_level);
    int level = (int)(-dbov);
    if (level < 0) level = 0;
    if (level > 127) level = 127;

    sid->noise_level = (uint8_t)level;
    sid->param_count = 0;

    return VOICE_SUCCESS;
}

voice_error_t voice_cng_decode_sid(
    voice_cng_t *cng,
    const voice_sid_frame_t *sid
) {
    if (!cng || !sid) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    float dbov = -(float)sid->noise_level;
    cng->target_level = powf(10.0f, dbov / 20.0f);

    return VOICE_SUCCESS;
}

void voice_cng_reset(voice_cng_t *cng) {
    if (!cng) return;

    cng->current_level = cng->noise_level;
    cng->target_level = cng->noise_level;

    memset(cng->pink_state, 0, sizeof(cng->pink_state));
    cng->brown_state = 0.0f;
    memset(cng->filter_state, 0, sizeof(cng->filter_state));
}
