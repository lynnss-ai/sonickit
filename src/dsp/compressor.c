/**
 * @file compressor.c
 * @brief Dynamic range compressor implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/compressor.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * Internal Structure
 * ============================================ */

struct voice_compressor_s {
    voice_compressor_config_t config;

    /* State */
    float envelope;         /**< Envelope detector state */
    float gain;             /**< Current gain */
    float hold_counter;     /**< Hold counter */

    /* Time constants */
    float attack_coeff;
    float release_coeff;
    float hold_samples;

    /* 阈值 (线性) */
    float threshold;

    /* 补偿增益 (线性) */
    float makeup_gain;

    /* 统计 */
    float input_level;
    float output_level;
    float gain_reduction;
    bool is_compressing;

    /* 侧链高通滤波器状态 */
    float sc_hp_z1, sc_hp_z2;
    float sc_hp_b0, sc_hp_b1, sc_hp_b2;
    float sc_hp_a1, sc_hp_a2;

    /* Lookahead buffer */
    float *lookahead_buffer;
    size_t lookahead_samples;
    size_t lookahead_pos;
};

/* ============================================
 * Helper Functions
 * ============================================ */

static float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

static float linear_to_db(float linear) {
    if (linear < 0.000001f) return -120.0f;
    return 20.0f * log10f(linear);
}

static float time_to_coeff(float time_ms, uint32_t sample_rate) {
    if (time_ms <= 0.0f) return 1.0f;
    float samples = time_ms * sample_rate / 1000.0f;
    return 1.0f - expf(-1.0f / samples);
}

/* Soft knee transfer function */
static float compute_gain_db(
    voice_compressor_config_t *config,
    float input_db
) {
    float threshold = config->threshold_db;
    float ratio = config->ratio;
    float knee_width = config->knee_width_db;

    if (config->knee_type == VOICE_DRC_SOFT_KNEE && knee_width > 0.0f) {
        /* Soft knee */
        float knee_start = threshold - knee_width / 2.0f;
        float knee_end = threshold + knee_width / 2.0f;

        if (input_db < knee_start) {
            return 0.0f;  /* No compression */
        } else if (input_db > knee_end) {
            /* Full compression */
            return (threshold + (input_db - threshold) / ratio) - input_db;
        } else {
            /* Knee region - smooth transition */
            float x = input_db - knee_start;
            float slope = (1.0f - 1.0f / ratio) / (2.0f * knee_width);
            return -slope * x * x;
        }
    } else {
        /* Hard knee */
        if (input_db < threshold) {
            return 0.0f;
        } else {
            if (config->type == VOICE_DRC_LIMITER) {
                return threshold - input_db;
            } else if (config->type == VOICE_DRC_GATE) {
                /* Gate: large attenuation below threshold */
                return -80.0f;  /* Large attenuation */
            } else {
                return (threshold + (input_db - threshold) / ratio) - input_db;
            }
        }
    }
}

/* ============================================
 * 配置初始化
 * ============================================ */

void voice_compressor_config_init(voice_compressor_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_compressor_config_t));

    config->type = VOICE_DRC_COMPRESSOR;
    config->sample_rate = 16000;

    config->threshold_db = -20.0f;
    config->ratio = 4.0f;
    config->knee_width_db = 6.0f;
    config->knee_type = VOICE_DRC_SOFT_KNEE;

    config->attack_ms = 10.0f;
    config->release_ms = 100.0f;
    config->hold_ms = 0.0f;

    config->makeup_gain_db = 0.0f;
    config->auto_makeup = true;

    config->detection = VOICE_DRC_RMS;
    config->lookahead_ms = 0.0f;

    config->enable_sidechain = false;
    config->sidechain_hpf = 0.0f;
}

/**
 * @brief Initialize limiter configuration
 * @details Sets up a fast-acting peak limiter for preventing clipping.
 *          Uses near-infinite ratio (100:1) and hard knee for brick-wall limiting.
 *
 * @param[out] config Configuration structure to initialize
 *
 * @note Limiter settings:
 *       - Threshold: -1 dBFS (just below 0dBFS)
 *       - Ratio: 100:1 (approximates infinity)
 *       - Attack: 0.5ms (very fast)
 *       - Release: 50ms
 *       - Detection: TRUE_PEAK (most accurate)
 */
void voice_limiter_config_init(voice_compressor_config_t *config) {
    voice_compressor_config_init(config);

    config->type = VOICE_DRC_LIMITER;
    config->threshold_db = -1.0f;
    config->ratio = 100.0f;  /* 近似无限 */
    config->attack_ms = 0.5f;
    config->release_ms = 50.0f;
    config->knee_type = VOICE_DRC_HARD_KNEE;
    config->detection = VOICE_DRC_TRUE_PEAK;
    config->auto_makeup = false;
}

/**
 * @brief Initialize noise gate configuration
 * @details Sets up a gate for suppressing low-level noise and room ambience.
 *          Uses expander-style reduction with soft knee for natural transitions.
 *
 * @param[out] config Configuration structure to initialize
 *
 * @note Gate settings:
 *       - Threshold: -40 dBFS
 *       - Ratio: 10:1 (expansion)
 *       - Attack: 1ms (fast)
 *       - Release: 50ms
 *       - Hold: 20ms (prevents chattering)
 *       - Soft knee: 6dB width
 */
void voice_gate_config_init(voice_compressor_config_t *config) {
    voice_compressor_config_init(config);

    config->type = VOICE_DRC_GATE;
    config->threshold_db = -40.0f;
    config->ratio = 10.0f;
    config->attack_ms = 1.0f;
    config->release_ms = 50.0f;
    config->hold_ms = 20.0f;
    config->knee_type = VOICE_DRC_SOFT_KNEE;
    config->knee_width_db = 6.0f;
}

/* ============================================
 * Lifecycle
 * ============================================ */

voice_compressor_t *voice_compressor_create(const voice_compressor_config_t *config) {
    voice_compressor_t *comp = (voice_compressor_t *)calloc(1, sizeof(voice_compressor_t));
    if (!comp) return NULL;

    if (config) {
        comp->config = *config;
    } else {
        voice_compressor_config_init(&comp->config);
    }

    /* 计算时间常数 */
    comp->attack_coeff = time_to_coeff(comp->config.attack_ms, comp->config.sample_rate);
    comp->release_coeff = time_to_coeff(comp->config.release_ms, comp->config.sample_rate);
    comp->hold_samples = comp->config.hold_ms * comp->config.sample_rate / 1000.0f;

    /* 初始化状态 */
    comp->envelope = 0.0f;
    comp->gain = 1.0f;

    /* 阈值转换 */
    comp->threshold = db_to_linear(comp->config.threshold_db);

    /* 自动补偿增益 */
    if (comp->config.auto_makeup && comp->config.type == VOICE_DRC_COMPRESSOR) {
        float reduction = (comp->config.threshold_db) * (1.0f - 1.0f / comp->config.ratio);
        comp->makeup_gain = db_to_linear(-reduction / 2.0f);
    } else {
        comp->makeup_gain = db_to_linear(comp->config.makeup_gain_db);
    }

    /* 前瞻缓冲区 */
    if (comp->config.lookahead_ms > 0.0f) {
        comp->lookahead_samples = (size_t)(comp->config.lookahead_ms *
                                           comp->config.sample_rate / 1000.0f);
        comp->lookahead_buffer = (float *)calloc(comp->lookahead_samples, sizeof(float));
    }

    return comp;
}

void voice_compressor_destroy(voice_compressor_t *comp) {
    if (comp) {
        free(comp->lookahead_buffer);
        free(comp);
    }
}

/* ============================================
 * Processing
 * ============================================ */

voice_error_t voice_compressor_process(
    voice_compressor_t *comp,
    int16_t *samples,
    size_t num_samples
) {
    if (!comp || !samples) return VOICE_ERROR_INVALID_PARAM;

    /* 分配临时浮点缓冲区 */
    float *float_buf = (float *)voice_aligned_alloc(num_samples * sizeof(float), 64);
    if (!float_buf) {
        /* 回退到标量处理 */
        float sum_sq = 0.0f;
        for (size_t i = 0; i < num_samples; i++) {
            float x = (float)samples[i] / 32768.0f;
            float level = (comp->config.detection == VOICE_DRC_PEAK) ? fabsf(x) : (x * x);
            float coeff = (level > comp->envelope) ? comp->attack_coeff : comp->release_coeff;
            comp->envelope = comp->envelope * (1.0f - coeff) + level * coeff;
            float env_db = (comp->config.detection == VOICE_DRC_RMS) ?
                           linear_to_db(sqrtf(comp->envelope)) : linear_to_db(comp->envelope);
            float gain_db = compute_gain_db(&comp->config, env_db);
            float target_gain = db_to_linear(gain_db);
            coeff = (target_gain < comp->gain) ? comp->attack_coeff : comp->release_coeff;
            comp->gain = comp->gain * (1.0f - coeff) + target_gain * coeff;
            float output = x * comp->gain * comp->makeup_gain;
            sum_sq += x * x;
            comp->is_compressing = (comp->gain < 0.99f);
            if (output > 1.0f) output = 1.0f;
            if (output < -1.0f) output = -1.0f;
            samples[i] = (int16_t)(output * 32767.0f);
        }
        comp->input_level = sqrtf(sum_sq / num_samples);
        comp->gain_reduction = linear_to_db(comp->gain);
        return VOICE_SUCCESS;
    }

    /* SIMD 批量转换: int16 -> float */
    voice_int16_to_float(samples, float_buf, num_samples);

    /* 动态处理 (样本依赖，无法向量化) */
    for (size_t i = 0; i < num_samples; i++) {
        float x = float_buf[i];

        /* 检测 */
        float level = (comp->config.detection == VOICE_DRC_PEAK) ? fabsf(x) : (x * x);

        /* 包络跟踪 */
        float coeff = (level > comp->envelope) ? comp->attack_coeff : comp->release_coeff;
        comp->envelope = comp->envelope * (1.0f - coeff) + level * coeff;

        float env_db = (comp->config.detection == VOICE_DRC_RMS) ?
                       linear_to_db(sqrtf(comp->envelope)) : linear_to_db(comp->envelope);

        /* 计算增益 */
        float gain_db = compute_gain_db(&comp->config, env_db);
        float target_gain = db_to_linear(gain_db);

        /* 平滑增益变化 */
        coeff = (target_gain < comp->gain) ? comp->attack_coeff : comp->release_coeff;
        comp->gain = comp->gain * (1.0f - coeff) + target_gain * coeff;

        /* 应用增益和补偿 */
        float_buf[i] = x * comp->gain * comp->makeup_gain;

        comp->is_compressing = (comp->gain < 0.99f);
    }

    /* SIMD 计算输入能量 */
    float energy = voice_compute_energy_float(float_buf, num_samples);
    comp->input_level = sqrtf(energy / num_samples);
    comp->gain_reduction = linear_to_db(comp->gain);

    /* SIMD 批量转换: float -> int16 (包含限制) */
    voice_float_to_int16(float_buf, samples, num_samples);

    voice_aligned_free(float_buf);
    return VOICE_SUCCESS;
}

voice_error_t voice_compressor_process_float(
    voice_compressor_t *comp,
    float *samples,
    size_t num_samples
) {
    if (!comp || !samples) return VOICE_ERROR_INVALID_PARAM;

    for (size_t i = 0; i < num_samples; i++) {
        float x = samples[i];

        float level = (comp->config.detection == VOICE_DRC_PEAK) ?
                      fabsf(x) : (x * x);

        float coeff = (level > comp->envelope) ? comp->attack_coeff : comp->release_coeff;
        comp->envelope = comp->envelope * (1.0f - coeff) + level * coeff;

        float env_db = (comp->config.detection == VOICE_DRC_RMS) ?
                       linear_to_db(sqrtf(comp->envelope)) : linear_to_db(comp->envelope);

        float gain_db = compute_gain_db(&comp->config, env_db);
        float target_gain = db_to_linear(gain_db);

        coeff = (target_gain < comp->gain) ? comp->attack_coeff : comp->release_coeff;
        comp->gain = comp->gain * (1.0f - coeff) + target_gain * coeff;

        samples[i] = x * comp->gain * comp->makeup_gain;
    }

    return VOICE_SUCCESS;
}

voice_error_t voice_compressor_process_sidechain(
    voice_compressor_t *comp,
    int16_t *samples,
    const int16_t *sidechain,
    size_t num_samples
) {
    if (!comp || !samples || !sidechain) return VOICE_ERROR_INVALID_PARAM;

    for (size_t i = 0; i < num_samples; i++) {
        float x = (float)samples[i] / 32768.0f;
        float sc = (float)sidechain[i] / 32768.0f;

        /* 使用侧链信号进行检测 */
        float level = (comp->config.detection == VOICE_DRC_PEAK) ?
                      fabsf(sc) : (sc * sc);

        float coeff = (level > comp->envelope) ? comp->attack_coeff : comp->release_coeff;
        comp->envelope = comp->envelope * (1.0f - coeff) + level * coeff;

        float env_db = (comp->config.detection == VOICE_DRC_RMS) ?
                       linear_to_db(sqrtf(comp->envelope)) : linear_to_db(comp->envelope);

        float gain_db = compute_gain_db(&comp->config, env_db);
        float target_gain = db_to_linear(gain_db);

        coeff = (target_gain < comp->gain) ? comp->attack_coeff : comp->release_coeff;
        comp->gain = comp->gain * (1.0f - coeff) + target_gain * coeff;

        float output = x * comp->gain * comp->makeup_gain;

        if (output > 1.0f) output = 1.0f;
        if (output < -1.0f) output = -1.0f;

        samples[i] = (int16_t)(output * 32767.0f);
    }

    return VOICE_SUCCESS;
}

/* ============================================
 * 参数控制
 * ============================================ */

voice_error_t voice_compressor_set_threshold(
    voice_compressor_t *comp,
    float threshold_db
) {
    if (!comp) return VOICE_ERROR_INVALID_PARAM;

    comp->config.threshold_db = threshold_db;
    comp->threshold = db_to_linear(threshold_db);
    return VOICE_SUCCESS;
}

voice_error_t voice_compressor_set_ratio(
    voice_compressor_t *comp,
    float ratio
) {
    if (!comp) return VOICE_ERROR_INVALID_PARAM;

    comp->config.ratio = ratio;
    return VOICE_SUCCESS;
}

voice_error_t voice_compressor_set_times(
    voice_compressor_t *comp,
    float attack_ms,
    float release_ms
) {
    if (!comp) return VOICE_ERROR_INVALID_PARAM;

    comp->config.attack_ms = attack_ms;
    comp->config.release_ms = release_ms;

    comp->attack_coeff = time_to_coeff(attack_ms, comp->config.sample_rate);
    comp->release_coeff = time_to_coeff(release_ms, comp->config.sample_rate);

    return VOICE_SUCCESS;
}

voice_error_t voice_compressor_set_makeup_gain(
    voice_compressor_t *comp,
    float gain_db
) {
    if (!comp) return VOICE_ERROR_INVALID_PARAM;

    comp->config.makeup_gain_db = gain_db;
    comp->makeup_gain = db_to_linear(gain_db);
    comp->config.auto_makeup = false;

    return VOICE_SUCCESS;
}

/* ============================================
 * 状态
 * ============================================ */

voice_error_t voice_compressor_get_state(
    voice_compressor_t *comp,
    voice_compressor_state_t *state
) {
    if (!comp || !state) return VOICE_ERROR_INVALID_PARAM;

    state->input_level_db = linear_to_db(comp->input_level);
    state->output_level_db = linear_to_db(comp->output_level);
    state->gain_reduction_db = linear_to_db(comp->gain);
    state->current_ratio = comp->config.ratio;
    state->is_compressing = comp->is_compressing;

    return VOICE_SUCCESS;
}

void voice_compressor_reset(voice_compressor_t *comp) {
    if (!comp) return;

    comp->envelope = 0.0f;
    comp->gain = 1.0f;
    comp->hold_counter = 0.0f;
    comp->is_compressing = false;

    if (comp->lookahead_buffer) {
        memset(comp->lookahead_buffer, 0,
               comp->lookahead_samples * sizeof(float));
        comp->lookahead_pos = 0;
    }
}
