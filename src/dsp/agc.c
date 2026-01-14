/**
 * @file agc.c
 * @brief Automatic Gain Control implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/agc.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_agc_s {
    voice_agc_config_t config;
    
    /* 状态 */
    float current_gain;             /**< 当前增益 (线性) */
    float target_gain;              /**< 目标增益 (线性) */
    float envelope;                 /**< 信号包络 */
    
    /* 时间常数 (每样本) */
    float attack_coeff;
    float release_coeff;
    float hold_samples;
    float hold_counter;
    
    /* 限制器状态 */
    float limiter_gain;
    
    /* 统计 */
    float input_level;
    float output_level;
    uint32_t saturation_count;
    bool gate_active;
    bool limiter_active;
};

/* ============================================
 * 辅助函数
 * ============================================ */

static float db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

static float linear_to_db(float linear) {
    if (linear <= 0.0f) return -96.0f;
    float db = 20.0f * log10f(linear);
    return db < -96.0f ? -96.0f : db;
}

static float time_to_coeff(float time_ms, uint32_t sample_rate) {
    if (time_ms <= 0.0f) return 1.0f;
    float time_sec = time_ms / 1000.0f;
    return 1.0f - expf(-1.0f / (time_sec * sample_rate));
}

static float compute_rms(const int16_t *samples, size_t count) {
    if (count == 0) return 0.0f;
    
    int64_t sum_sq = 0;
    for (size_t i = 0; i < count; i++) {
        sum_sq += (int32_t)samples[i] * (int32_t)samples[i];
    }
    
    return sqrtf((float)sum_sq / (float)count) / 32768.0f;
}

static float compute_rms_float(const float *samples, size_t count) {
    if (count == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum_sq += samples[i] * samples[i];
    }
    
    return sqrtf(sum_sq / (float)count);
}

/* ============================================
 * 配置
 * ============================================ */

void voice_agc_config_init(voice_agc_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_agc_config_t));
    
    config->mode = VOICE_AGC_ADAPTIVE;
    config->sample_rate = 16000;
    config->frame_size = 160;       /* 10ms @ 16kHz */
    
    config->target_level_dbfs = -6.0f;
    
    config->min_gain_db = -12.0f;
    config->max_gain_db = 30.0f;
    
    config->attack_time_ms = 10.0f;
    config->release_time_ms = 100.0f;
    config->hold_time_ms = 50.0f;
    
    config->compression = VOICE_AGC_COMPRESSION_MEDIUM;
    config->compression_threshold_db = -20.0f;
    
    config->enable_noise_gate = true;
    config->noise_gate_threshold_db = -50.0f;
    
    config->enable_limiter = true;
    config->limiter_threshold_db = -1.0f;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_agc_t *voice_agc_create(const voice_agc_config_t *config) {
    voice_agc_t *agc = (voice_agc_t *)calloc(1, sizeof(voice_agc_t));
    if (!agc) return NULL;
    
    if (config) {
        agc->config = *config;
    } else {
        voice_agc_config_init(&agc->config);
    }
    
    /* 初始化增益 */
    agc->current_gain = 1.0f;
    agc->target_gain = 1.0f;
    agc->limiter_gain = 1.0f;
    
    /* 计算时间常数 */
    agc->attack_coeff = time_to_coeff(agc->config.attack_time_ms, agc->config.sample_rate);
    agc->release_coeff = time_to_coeff(agc->config.release_time_ms, agc->config.sample_rate);
    agc->hold_samples = agc->config.hold_time_ms * agc->config.sample_rate / 1000.0f;
    
    return agc;
}

void voice_agc_destroy(voice_agc_t *agc) {
    if (agc) {
        free(agc);
    }
}

/* ============================================
 * 处理
 * ============================================ */

voice_error_t voice_agc_process(
    voice_agc_t *agc,
    int16_t *samples,
    size_t num_samples
) {
    if (!agc || !samples) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    /* 计算输入电平 */
    float input_rms = compute_rms(samples, num_samples);
    agc->input_level = input_rms;
    
    float input_db = linear_to_db(input_rms);
    
    /* 噪声门 */
    if (agc->config.enable_noise_gate) {
        if (input_db < agc->config.noise_gate_threshold_db) {
            agc->gate_active = true;
            /* 平滑降低增益 */
            agc->target_gain *= 0.9f;
            if (agc->target_gain < 0.01f) {
                agc->target_gain = 0.01f;
            }
        } else {
            agc->gate_active = false;
        }
    }
    
    /* 计算目标增益 */
    if (!agc->gate_active && input_rms > 0.0001f) {
        float target_linear = db_to_linear(agc->config.target_level_dbfs);
        float desired_gain_db = agc->config.target_level_dbfs - input_db;
        
        /* 应用压缩 */
        if (agc->config.compression != VOICE_AGC_COMPRESSION_NONE) {
            float threshold_db = agc->config.compression_threshold_db;
            if (input_db > threshold_db) {
                float ratio = 1.0f;
                switch (agc->config.compression) {
                    case VOICE_AGC_COMPRESSION_LOW:    ratio = 2.0f; break;
                    case VOICE_AGC_COMPRESSION_MEDIUM: ratio = 4.0f; break;
                    case VOICE_AGC_COMPRESSION_HIGH:   ratio = 8.0f; break;
                    default: break;
                }
                float excess = input_db - threshold_db;
                float compressed_excess = excess / ratio;
                desired_gain_db = threshold_db + compressed_excess - input_db + 
                                  (agc->config.target_level_dbfs - threshold_db);
            }
        }
        
        /* 限制增益范围 */
        if (desired_gain_db < agc->config.min_gain_db) {
            desired_gain_db = agc->config.min_gain_db;
        }
        if (desired_gain_db > agc->config.max_gain_db) {
            desired_gain_db = agc->config.max_gain_db;
        }
        
        agc->target_gain = db_to_linear(desired_gain_db);
    }
    
    /* 平滑增益变化 */
    float coeff;
    if (agc->target_gain < agc->current_gain) {
        coeff = agc->attack_coeff;
    } else {
        coeff = agc->release_coeff;
    }
    
    /* 应用增益 */
    for (size_t i = 0; i < num_samples; i++) {
        /* 更新当前增益 */
        agc->current_gain = agc->current_gain * (1.0f - coeff) + agc->target_gain * coeff;
        
        /* 应用增益 */
        float sample = (float)samples[i] * agc->current_gain;
        
        /* 限制器 */
        if (agc->config.enable_limiter) {
            float threshold = db_to_linear(agc->config.limiter_threshold_db) * 32768.0f;
            float abs_sample = sample < 0 ? -sample : sample;
            
            if (abs_sample > threshold) {
                float limit_gain = threshold / abs_sample;
                if (limit_gain < agc->limiter_gain) {
                    agc->limiter_gain = limit_gain;
                    agc->limiter_active = true;
                }
            } else {
                /* 限制器释放 */
                agc->limiter_gain += 0.001f;
                if (agc->limiter_gain > 1.0f) {
                    agc->limiter_gain = 1.0f;
                    agc->limiter_active = false;
                }
            }
            
            sample *= agc->limiter_gain;
        }
        
        /* 饱和 */
        if (sample > 32767.0f) {
            sample = 32767.0f;
            agc->saturation_count++;
        } else if (sample < -32768.0f) {
            sample = -32768.0f;
            agc->saturation_count++;
        }
        
        samples[i] = (int16_t)sample;
    }
    
    /* 更新输出电平 */
    agc->output_level = compute_rms(samples, num_samples);
    
    return VOICE_SUCCESS;
}

voice_error_t voice_agc_process_float(
    voice_agc_t *agc,
    float *samples,
    size_t num_samples
) {
    if (!agc || !samples) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    float input_rms = compute_rms_float(samples, num_samples);
    agc->input_level = input_rms;
    
    float input_db = linear_to_db(input_rms);
    
    /* 噪声门 */
    if (agc->config.enable_noise_gate) {
        agc->gate_active = (input_db < agc->config.noise_gate_threshold_db);
        if (agc->gate_active) {
            agc->target_gain *= 0.9f;
            if (agc->target_gain < 0.01f) agc->target_gain = 0.01f;
        }
    }
    
    /* 计算目标增益 */
    if (!agc->gate_active && input_rms > 0.0001f) {
        float desired_gain_db = agc->config.target_level_dbfs - input_db;
        
        if (desired_gain_db < agc->config.min_gain_db) {
            desired_gain_db = agc->config.min_gain_db;
        }
        if (desired_gain_db > agc->config.max_gain_db) {
            desired_gain_db = agc->config.max_gain_db;
        }
        
        agc->target_gain = db_to_linear(desired_gain_db);
    }
    
    float coeff = (agc->target_gain < agc->current_gain) ? 
                   agc->attack_coeff : agc->release_coeff;
    
    float limiter_threshold = db_to_linear(agc->config.limiter_threshold_db);
    
    for (size_t i = 0; i < num_samples; i++) {
        agc->current_gain = agc->current_gain * (1.0f - coeff) + agc->target_gain * coeff;
        
        float sample = samples[i] * agc->current_gain;
        
        /* 限制器 */
        if (agc->config.enable_limiter) {
            float abs_sample = sample < 0 ? -sample : sample;
            
            if (abs_sample > limiter_threshold) {
                float limit_gain = limiter_threshold / abs_sample;
                if (limit_gain < agc->limiter_gain) {
                    agc->limiter_gain = limit_gain;
                    agc->limiter_active = true;
                }
            } else {
                agc->limiter_gain += 0.001f;
                if (agc->limiter_gain > 1.0f) {
                    agc->limiter_gain = 1.0f;
                    agc->limiter_active = false;
                }
            }
            
            sample *= agc->limiter_gain;
        }
        
        /* 软削波 */
        if (sample > 1.0f) {
            sample = 1.0f;
            agc->saturation_count++;
        } else if (sample < -1.0f) {
            sample = -1.0f;
            agc->saturation_count++;
        }
        
        samples[i] = sample;
    }
    
    agc->output_level = compute_rms_float(samples, num_samples);
    
    return VOICE_SUCCESS;
}

/* ============================================
 * 控制接口
 * ============================================ */

voice_error_t voice_agc_set_target_level(voice_agc_t *agc, float level_dbfs) {
    if (!agc) return VOICE_ERROR_INVALID_PARAM;
    
    agc->config.target_level_dbfs = level_dbfs;
    return VOICE_SUCCESS;
}

voice_error_t voice_agc_set_gain_range(
    voice_agc_t *agc,
    float min_gain_db,
    float max_gain_db
) {
    if (!agc) return VOICE_ERROR_INVALID_PARAM;
    
    agc->config.min_gain_db = min_gain_db;
    agc->config.max_gain_db = max_gain_db;
    return VOICE_SUCCESS;
}

voice_error_t voice_agc_set_mode(voice_agc_t *agc, voice_agc_mode_t mode) {
    if (!agc) return VOICE_ERROR_INVALID_PARAM;
    
    agc->config.mode = mode;
    return VOICE_SUCCESS;
}

voice_error_t voice_agc_get_state(voice_agc_t *agc, voice_agc_state_t *state) {
    if (!agc || !state) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    state->current_gain_db = linear_to_db(agc->current_gain);
    state->input_level_db = linear_to_db(agc->input_level);
    state->output_level_db = linear_to_db(agc->output_level);
    state->compression_ratio = 1.0f;  /* TODO: 计算实际压缩比 */
    state->gate_active = agc->gate_active;
    state->limiter_active = agc->limiter_active;
    state->saturation_count = agc->saturation_count;
    
    return VOICE_SUCCESS;
}

void voice_agc_reset(voice_agc_t *agc) {
    if (!agc) return;
    
    agc->current_gain = 1.0f;
    agc->target_gain = 1.0f;
    agc->limiter_gain = 1.0f;
    agc->envelope = 0.0f;
    agc->hold_counter = 0.0f;
    agc->saturation_count = 0;
    agc->gate_active = false;
    agc->limiter_active = false;
}

float voice_agc_analyze_level(
    voice_agc_t *agc,
    const int16_t *samples,
    size_t num_samples
) {
    (void)agc;
    return linear_to_db(compute_rms(samples, num_samples));
}
