/**
 * @file audio_level.c
 * @brief Audio level metering implementation
 */

#include "audio/audio_level.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_level_meter_s {
    voice_level_meter_config_t config;
    
    /* 平滑状态 */
    float peak_smooth;
    float rms_smooth;
    
    /* 平滑系数 */
    float attack_coeff;
    float release_coeff;
    
    /* 统计 */
    uint32_t clip_count;
    float max_peak;
};

/* ============================================
 * 辅助函数
 * ============================================ */

static float time_constant_to_coeff(float time_ms, uint32_t sample_rate, uint32_t block_size) {
    if (time_ms <= 0.0f) return 1.0f;
    
    float blocks_per_second = (float)sample_rate / (float)block_size;
    float time_sec = time_ms / 1000.0f;
    float num_blocks = time_sec * blocks_per_second;
    
    if (num_blocks < 1.0f) return 1.0f;
    return 1.0f - expf(-1.0f / num_blocks);
}

/* ============================================
 * 配置
 * ============================================ */

void voice_level_meter_config_init(voice_level_meter_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_level_meter_config_t));
    
    config->sample_rate = 48000;
    config->channels = 1;
    config->window_size_ms = 50;
    config->attack_ms = 5.0f;
    config->release_ms = 100.0f;
    config->enable_true_peak = false;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_level_meter_t *voice_level_meter_create(const voice_level_meter_config_t *config) {
    voice_level_meter_t *meter = (voice_level_meter_t *)calloc(1, sizeof(voice_level_meter_t));
    if (!meter) return NULL;
    
    if (config) {
        meter->config = *config;
    } else {
        voice_level_meter_config_init(&meter->config);
    }
    
    /* 计算平滑系数 */
    uint32_t block_size = meter->config.sample_rate * meter->config.window_size_ms / 1000;
    if (block_size == 0) block_size = 1;
    
    meter->attack_coeff = time_constant_to_coeff(
        meter->config.attack_ms, meter->config.sample_rate, block_size
    );
    meter->release_coeff = time_constant_to_coeff(
        meter->config.release_ms, meter->config.sample_rate, block_size
    );
    
    meter->peak_smooth = 0.0f;
    meter->rms_smooth = 0.0f;
    meter->max_peak = 0.0f;
    
    return meter;
}

void voice_level_meter_destroy(voice_level_meter_t *meter) {
    if (meter) {
        free(meter);
    }
}

/* ============================================
 * 处理
 * ============================================ */

voice_error_t voice_level_meter_process(
    voice_level_meter_t *meter,
    const int16_t *samples,
    size_t num_samples,
    voice_level_result_t *result
) {
    if (!meter || !samples || !result) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(voice_level_result_t));
    
    if (num_samples == 0) {
        result->peak_db = -96.0f;
        result->rms_db = -96.0f;
        return VOICE_SUCCESS;
    }
    
    /* 计算峰值和 RMS */
    int16_t peak_sample = 0;
    int64_t sum_sq = 0;
    uint32_t clips = 0;
    
    for (size_t i = 0; i < num_samples; i++) {
        int16_t s = samples[i];
        int16_t abs_s = s < 0 ? -s : s;
        
        if (abs_s > peak_sample) {
            peak_sample = abs_s;
        }
        
        sum_sq += (int32_t)s * (int32_t)s;
        
        /* 检测削波 */
        if (abs_s >= 32760) {
            clips++;
        }
    }
    
    float peak = (float)peak_sample / 32768.0f;
    float rms = sqrtf((float)sum_sq / (float)num_samples) / 32768.0f;
    
    /* 平滑处理 */
    float coeff;
    
    /* 峰值平滑 */
    if (peak > meter->peak_smooth) {
        coeff = meter->attack_coeff;
    } else {
        coeff = meter->release_coeff;
    }
    meter->peak_smooth = meter->peak_smooth * (1.0f - coeff) + peak * coeff;
    
    /* RMS 平滑 */
    if (rms > meter->rms_smooth) {
        coeff = meter->attack_coeff;
    } else {
        coeff = meter->release_coeff;
    }
    meter->rms_smooth = meter->rms_smooth * (1.0f - coeff) + rms * coeff;
    
    /* 更新最大峰值 */
    if (peak > meter->max_peak) {
        meter->max_peak = peak;
    }
    
    /* 填充结果 */
    result->peak_sample = meter->peak_smooth;
    result->rms_linear = meter->rms_smooth;
    result->peak_db = voice_linear_to_db(meter->peak_smooth);
    result->rms_db = voice_linear_to_db(meter->rms_smooth);
    result->clipping = clips > 0;
    result->clip_count = clips;
    
    meter->clip_count += clips;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_level_meter_process_float(
    voice_level_meter_t *meter,
    const float *samples,
    size_t num_samples,
    voice_level_result_t *result
) {
    if (!meter || !samples || !result) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(voice_level_result_t));
    
    if (num_samples == 0) {
        result->peak_db = -96.0f;
        result->rms_db = -96.0f;
        return VOICE_SUCCESS;
    }
    
    float peak = 0.0f;
    float sum_sq = 0.0f;
    uint32_t clips = 0;
    
    for (size_t i = 0; i < num_samples; i++) {
        float s = samples[i];
        float abs_s = s < 0.0f ? -s : s;
        
        if (abs_s > peak) {
            peak = abs_s;
        }
        
        sum_sq += s * s;
        
        if (abs_s >= 0.99f) {
            clips++;
        }
    }
    
    float rms = sqrtf(sum_sq / (float)num_samples);
    
    /* 平滑处理 */
    float coeff;
    
    if (peak > meter->peak_smooth) {
        coeff = meter->attack_coeff;
    } else {
        coeff = meter->release_coeff;
    }
    meter->peak_smooth = meter->peak_smooth * (1.0f - coeff) + peak * coeff;
    
    if (rms > meter->rms_smooth) {
        coeff = meter->attack_coeff;
    } else {
        coeff = meter->release_coeff;
    }
    meter->rms_smooth = meter->rms_smooth * (1.0f - coeff) + rms * coeff;
    
    if (peak > meter->max_peak) {
        meter->max_peak = peak;
    }
    
    result->peak_sample = meter->peak_smooth;
    result->rms_linear = meter->rms_smooth;
    result->peak_db = voice_linear_to_db(meter->peak_smooth);
    result->rms_db = voice_linear_to_db(meter->rms_smooth);
    result->clipping = clips > 0;
    result->clip_count = clips;
    
    meter->clip_count += clips;
    
    return VOICE_SUCCESS;
}

float voice_level_meter_get_level_db(voice_level_meter_t *meter) {
    if (!meter) return -96.0f;
    return voice_linear_to_db(meter->rms_smooth);
}

void voice_level_meter_reset(voice_level_meter_t *meter) {
    if (!meter) return;
    
    meter->peak_smooth = 0.0f;
    meter->rms_smooth = 0.0f;
    meter->max_peak = 0.0f;
    meter->clip_count = 0;
}

/* ============================================
 * 快捷函数
 * ============================================ */

float voice_audio_peak_db(const int16_t *samples, size_t num_samples) {
    if (!samples || num_samples == 0) return -96.0f;
    
    int16_t peak = 0;
    for (size_t i = 0; i < num_samples; i++) {
        int16_t abs_s = samples[i] < 0 ? -samples[i] : samples[i];
        if (abs_s > peak) peak = abs_s;
    }
    
    return voice_linear_to_db((float)peak / 32768.0f);
}

float voice_audio_rms_db(const int16_t *samples, size_t num_samples) {
    if (!samples || num_samples == 0) return -96.0f;
    
    int64_t sum_sq = 0;
    for (size_t i = 0; i < num_samples; i++) {
        sum_sq += (int32_t)samples[i] * (int32_t)samples[i];
    }
    
    float rms = sqrtf((float)sum_sq / (float)num_samples) / 32768.0f;
    return voice_linear_to_db(rms);
}

/* ============================================
 * RFC 6464 音频电平
 * ============================================ */

uint8_t voice_audio_level_rfc6464(const int16_t *samples, size_t num_samples) {
    if (!samples || num_samples == 0) return 127;
    
    /* 计算 RMS */
    int64_t sum_sq = 0;
    for (size_t i = 0; i < num_samples; i++) {
        sum_sq += (int32_t)samples[i] * (int32_t)samples[i];
    }
    
    float rms = sqrtf((float)sum_sq / (float)num_samples);
    
    /* 转换到 dBov (相对于满量程) */
    float dbov = 0.0f;
    if (rms > 0.0f) {
        dbov = 20.0f * log10f(rms / 32768.0f);
    } else {
        dbov = -127.0f;
    }
    
    /* RFC 6464: level = -dBov, clamped to 0-127 */
    int level = (int)(-dbov + 0.5f);
    if (level < 0) level = 0;
    if (level > 127) level = 127;
    
    return (uint8_t)level;
}
