/**
 * @file audio_level.h
 * @brief Audio level metering and analysis
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 音频电平测量和分析工具
 */

#ifndef VOICE_AUDIO_LEVEL_H
#define VOICE_AUDIO_LEVEL_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 电平计类型
 * ============================================ */

typedef struct voice_level_meter_s voice_level_meter_t;

/** 测量类型 */
typedef enum {
    VOICE_LEVEL_PEAK,           /**< 峰值电平 */
    VOICE_LEVEL_RMS,            /**< RMS 电平 */
    VOICE_LEVEL_LUFS,           /**< LUFS 响度 (ITU-R BS.1770) */
} voice_level_type_t;

/* ============================================
 * 配置
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t window_size_ms;    /**< 测量窗口大小 */
    float attack_ms;            /**< 攻击时间 (ms) */
    float release_ms;           /**< 释放时间 (ms) */
    bool enable_true_peak;      /**< 启用真峰值测量 (过采样) */
} voice_level_meter_config_t;

/* ============================================
 * 测量结果
 * ============================================ */

typedef struct {
    float peak_db;              /**< 峰值电平 (dBFS) */
    float rms_db;               /**< RMS 电平 (dBFS) */
    float peak_sample;          /**< 峰值样本值 (线性) */
    float rms_linear;           /**< RMS 值 (线性) */
    bool clipping;              /**< 是否削波 */
    uint32_t clip_count;        /**< 削波样本数 */
} voice_level_result_t;

/* ============================================
 * 电平计 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_level_meter_config_init(voice_level_meter_config_t *config);

/**
 * @brief 创建电平计
 */
voice_level_meter_t *voice_level_meter_create(const voice_level_meter_config_t *config);

/**
 * @brief 销毁电平计
 */
void voice_level_meter_destroy(voice_level_meter_t *meter);

/**
 * @brief 处理音频样本
 */
voice_error_t voice_level_meter_process(
    voice_level_meter_t *meter,
    const int16_t *samples,
    size_t num_samples,
    voice_level_result_t *result
);

/**
 * @brief 处理浮点音频样本
 */
voice_error_t voice_level_meter_process_float(
    voice_level_meter_t *meter,
    const float *samples,
    size_t num_samples,
    voice_level_result_t *result
);

/**
 * @brief 获取当前电平
 */
float voice_level_meter_get_level_db(voice_level_meter_t *meter);

/**
 * @brief 重置电平计
 */
void voice_level_meter_reset(voice_level_meter_t *meter);

/* ============================================
 * 快捷函数 (无状态)
 * ============================================ */

/**
 * @brief 计算音频块的峰值电平 (dB)
 */
float voice_audio_peak_db(const int16_t *samples, size_t num_samples);

/**
 * @brief 计算音频块的 RMS 电平 (dB)
 */
float voice_audio_rms_db(const int16_t *samples, size_t num_samples);

/**
 * @brief 线性值转 dB
 */
static inline float voice_linear_to_db(float linear) {
    if (linear <= 0.0f) return -96.0f;
    float db = 20.0f * log10f(linear);
    return db < -96.0f ? -96.0f : db;
}

/**
 * @brief dB 转线性值
 */
static inline float voice_db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

/* ============================================
 * RTP 音频电平扩展 (RFC 6464)
 * ============================================ */

/**
 * @brief 计算 RTP 扩展头中的音频电平 (RFC 6464)
 * @param samples 音频样本
 * @param num_samples 样本数
 * @return 音频电平 (0-127, 0 = 0dBov, 127 = -127dBov)
 */
uint8_t voice_audio_level_rfc6464(const int16_t *samples, size_t num_samples);

/**
 * @brief 解析 RFC 6464 音频电平
 * @param level RFC 6464 电平值
 * @return dBov 值 (0 到 -127)
 */
static inline float voice_audio_level_rfc6464_to_db(uint8_t level) {
    return -(float)(level & 0x7F);
}

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_LEVEL_H */
