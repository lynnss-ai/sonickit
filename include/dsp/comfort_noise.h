/**
 * @file comfort_noise.h
 * @brief Comfort Noise Generation (CNG)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 舒适噪声生成模块，用于在静音期间生成背景噪声
 * 避免通话中出现"死寂"的感觉
 */

#ifndef VOICE_COMFORT_NOISE_H
#define VOICE_COMFORT_NOISE_H

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

typedef struct voice_cng_s voice_cng_t;

/** CNG 噪声类型 */
typedef enum {
    VOICE_CNG_WHITE,            /**< 白噪声 */
    VOICE_CNG_PINK,             /**< 粉红噪声 */
    VOICE_CNG_BROWN,            /**< 布朗噪声 */
    VOICE_CNG_SPECTRAL,         /**< 基于频谱匹配 */
} voice_cng_type_t;

/* ============================================
 * SID 帧 (Silence Insertion Descriptor)
 * RFC 3389
 * ============================================ */

typedef struct {
    uint8_t noise_level;        /**< 噪声电平 (0-127) */
    uint8_t spectral_params[12];/**< 频谱参数 (可选) */
    size_t param_count;         /**< 参数数量 */
} voice_sid_frame_t;

/* ============================================
 * 配置
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    voice_cng_type_t noise_type;
    
    /* 电平控制 */
    float noise_level_db;       /**< 舒适噪声电平 (dBFS), 通常 -45 到 -60 */
    bool auto_level;            /**< 自动调整电平 (基于输入噪声) */
    
    /* 平滑 */
    float transition_time_ms;   /**< 静音/语音切换过渡时间 */
    
    /* 频谱匹配 */
    bool enable_spectral_match; /**< 启用频谱匹配 */
    size_t spectral_bands;      /**< 频谱分析频带数 */
} voice_cng_config_t;

/* ============================================
 * CNG API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_cng_config_init(voice_cng_config_t *config);

/**
 * @brief 创建 CNG 实例
 */
voice_cng_t *voice_cng_create(const voice_cng_config_t *config);

/**
 * @brief 销毁 CNG 实例
 */
void voice_cng_destroy(voice_cng_t *cng);

/**
 * @brief 分析输入噪声特性
 * 在检测到静音时调用，用于学习背景噪声特性
 */
voice_error_t voice_cng_analyze(
    voice_cng_t *cng,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief 生成舒适噪声
 * @param cng CNG 实例
 * @param output 输出缓冲区
 * @param num_samples 生成的样本数
 */
voice_error_t voice_cng_generate(
    voice_cng_t *cng,
    int16_t *output,
    size_t num_samples
);

/**
 * @brief 生成舒适噪声 (float)
 */
voice_error_t voice_cng_generate_float(
    voice_cng_t *cng,
    float *output,
    size_t num_samples
);

/**
 * @brief 设置噪声电平
 */
voice_error_t voice_cng_set_level(voice_cng_t *cng, float level_db);

/**
 * @brief 获取当前噪声电平
 */
float voice_cng_get_level(voice_cng_t *cng);

/**
 * @brief 编码 SID 帧 (RFC 3389)
 */
voice_error_t voice_cng_encode_sid(
    voice_cng_t *cng,
    voice_sid_frame_t *sid
);

/**
 * @brief 解码 SID 帧并更新 CNG 参数
 */
voice_error_t voice_cng_decode_sid(
    voice_cng_t *cng,
    const voice_sid_frame_t *sid
);

/**
 * @brief 重置 CNG
 */
void voice_cng_reset(voice_cng_t *cng);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_COMFORT_NOISE_H */
