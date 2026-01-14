/**
 * @file agc.h
 * @brief Automatic Gain Control (AGC)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 自动增益控制模块，自动调整音频电平到目标范围
 * 支持多种 AGC 模式和自适应算法
 */

#ifndef VOICE_AGC_H
#define VOICE_AGC_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 类型定义
 * ============================================ */

typedef struct voice_agc_s voice_agc_t;

/** AGC 模式 */
typedef enum {
    VOICE_AGC_FIXED,            /**< 固定增益 */
    VOICE_AGC_ADAPTIVE,         /**< 自适应增益 */
    VOICE_AGC_ADAPTIVE_DIGITAL, /**< 数字自适应 (WebRTC 风格) */
    VOICE_AGC_LIMITER,          /**< 仅限制器模式 */
} voice_agc_mode_t;

/** AGC 压缩比 */
typedef enum {
    VOICE_AGC_COMPRESSION_NONE = 0,     /**< 无压缩 */
    VOICE_AGC_COMPRESSION_LOW = 1,      /**< 低压缩 (2:1) */
    VOICE_AGC_COMPRESSION_MEDIUM = 2,   /**< 中压缩 (4:1) */
    VOICE_AGC_COMPRESSION_HIGH = 3,     /**< 高压缩 (8:1) */
} voice_agc_compression_t;

/* ============================================
 * 配置
 * ============================================ */

typedef struct {
    voice_agc_mode_t mode;
    uint32_t sample_rate;
    uint32_t frame_size;            /**< 帧大小 (样本数) */
    
    /* 目标电平 */
    float target_level_dbfs;        /**< 目标输出电平 (dBFS), 通常 -3 到 -18 */
    
    /* 增益限制 */
    float min_gain_db;              /**< 最小增益 (dB) */
    float max_gain_db;              /**< 最大增益 (dB) */
    
    /* 动态参数 */
    float attack_time_ms;           /**< 攻击时间 (ms) */
    float release_time_ms;          /**< 释放时间 (ms) */
    float hold_time_ms;             /**< 保持时间 (ms) */
    
    /* 压缩 */
    voice_agc_compression_t compression;
    float compression_threshold_db; /**< 压缩阈值 (dBFS) */
    
    /* 噪声门 */
    bool enable_noise_gate;
    float noise_gate_threshold_db;  /**< 噪声门阈值 (dBFS) */
    
    /* 限制器 */
    bool enable_limiter;
    float limiter_threshold_db;     /**< 限制器阈值 (dBFS), 通常 -1 */
} voice_agc_config_t;

/* ============================================
 * 状态信息
 * ============================================ */

typedef struct {
    float current_gain_db;          /**< 当前增益 (dB) */
    float input_level_db;           /**< 输入电平 (dBFS) */
    float output_level_db;          /**< 输出电平 (dBFS) */
    float compression_ratio;        /**< 当前压缩比 */
    bool gate_active;               /**< 噪声门是否激活 */
    bool limiter_active;            /**< 限制器是否激活 */
    uint32_t saturation_count;      /**< 饱和/削波次数 */
} voice_agc_state_t;

/* ============================================
 * AGC API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_agc_config_init(voice_agc_config_t *config);

/**
 * @brief 创建 AGC 实例
 */
voice_agc_t *voice_agc_create(const voice_agc_config_t *config);

/**
 * @brief 销毁 AGC 实例
 */
void voice_agc_destroy(voice_agc_t *agc);

/**
 * @brief 处理音频帧 (int16)
 * @param agc AGC 实例
 * @param samples 音频样本 (原地修改)
 * @param num_samples 样本数
 * @return 错误码
 */
voice_error_t voice_agc_process(
    voice_agc_t *agc,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief 处理音频帧 (float)
 */
voice_error_t voice_agc_process_float(
    voice_agc_t *agc,
    float *samples,
    size_t num_samples
);

/**
 * @brief 设置目标电平
 */
voice_error_t voice_agc_set_target_level(voice_agc_t *agc, float level_dbfs);

/**
 * @brief 设置增益范围
 */
voice_error_t voice_agc_set_gain_range(
    voice_agc_t *agc,
    float min_gain_db,
    float max_gain_db
);

/**
 * @brief 设置模式
 */
voice_error_t voice_agc_set_mode(voice_agc_t *agc, voice_agc_mode_t mode);

/**
 * @brief 获取当前状态
 */
voice_error_t voice_agc_get_state(voice_agc_t *agc, voice_agc_state_t *state);

/**
 * @brief 重置 AGC 状态
 */
void voice_agc_reset(voice_agc_t *agc);

/**
 * @brief 分析音频电平 (不修改样本)
 */
float voice_agc_analyze_level(
    voice_agc_t *agc,
    const int16_t *samples,
    size_t num_samples
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AGC_H */
