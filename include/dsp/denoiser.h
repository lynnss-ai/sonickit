/**
 * @file denoiser.h
 * @brief Audio denoiser interface (SpeexDSP + RNNoise)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef DSP_DENOISER_H
#define DSP_DENOISER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 去噪器接口
 * ============================================ */

/** 去噪器句柄 */
typedef struct voice_denoiser_s voice_denoiser_t;

/** 去噪器配置 */
typedef struct {
    voice_denoise_engine_t engine;      /**< 去噪引擎 */
    uint32_t sample_rate;               /**< 采样率 */
    uint32_t frame_size;                /**< 帧大小(样本数) */
    int noise_suppress_db;              /**< 噪声抑制量 (负dB, 如-25) */
    bool enable_agc;                    /**< 启用AGC */
    bool enable_vad;                    /**< 启用VAD */
    float agc_level;                    /**< AGC目标电平 */

    /* 兼容字段别名 */
    #define denoise_enabled enable_vad  /* 兼容旧代码 */
    #define agc_enabled enable_agc      /* 兼容旧代码 */
} voice_denoiser_config_t;

/**
 * @brief Initialize default configuration
 * @param config Configuration structure pointer
 */
VOICE_API void voice_denoiser_config_init(voice_denoiser_config_t *config);

/**
 * @brief Create denoiser instance
 * @param config Configuration
 * @return Denoiser handle, or NULL on failure
 */
VOICE_API voice_denoiser_t *voice_denoiser_create(const voice_denoiser_config_t *config);

/**
 * @brief Destroy denoiser instance
 * @param denoiser Denoiser handle
 */
VOICE_API void voice_denoiser_destroy(voice_denoiser_t *denoiser);

/**
 * @brief Process audio frame (int16)
 * @param denoiser Denoiser handle
 * @param samples Input/output audio samples (in-place processing)
 * @param count Sample count
 * @return Voice probability (0.0-1.0), negative on error
 */
VOICE_API float voice_denoiser_process_int16(
    voice_denoiser_t *denoiser,
    int16_t *samples,
    size_t count
);

/**
 * @brief Process audio frame (float)
 * @param denoiser Denoiser handle
 * @param samples Input/output audio samples (in-place processing)
 * @param count Sample count
 * @return Voice probability (0.0-1.0), negative on error
 */
VOICE_API float voice_denoiser_process_float(
    voice_denoiser_t *denoiser,
    float *samples,
    size_t count
);

/**
 * @brief Process audio frame (generic macro, defaults to int16)
 * @param denoiser Denoiser handle
 * @param samples Input/output audio samples (in-place processing)
 * @param count Sample count
 * @return Voice probability (0.0-1.0), negative on error
 */
#define voice_denoiser_process(denoiser, samples, count) \
    voice_denoiser_process_int16((denoiser), (samples), (count))

/**
 * @brief Set noise suppression level
 * @param denoiser Denoiser handle
 * @param db Suppression level (negative dB)
 * @return Error code
 */
VOICE_API voice_error_t voice_denoiser_set_noise_suppress(voice_denoiser_t *denoiser, int db);

/**
 * @brief Enable/disable denoising
 * @param denoiser Denoiser handle
 * @param enabled Enable flag
 * @return Error code
 */
VOICE_API voice_error_t voice_denoiser_set_enabled(voice_denoiser_t *denoiser, bool enabled);

/**
 * @brief Get current engine type
 * @param denoiser Denoiser handle
 * @return Engine type
 */
VOICE_API voice_denoise_engine_t voice_denoiser_get_engine(voice_denoiser_t *denoiser);

/**
 * @brief Switch denoising engine
 * @param denoiser Denoiser handle
 * @param engine Target engine
 * @return Error code
 */
VOICE_API voice_error_t voice_denoiser_switch_engine(
    voice_denoiser_t *denoiser,
    voice_denoise_engine_t engine
);

/**
 * @brief Reset denoiser state
 * @param denoiser Denoiser handle
 */
VOICE_API void voice_denoiser_reset(voice_denoiser_t *denoiser);

/* ============================================
 * 自适应去噪器
 * ============================================ */

/** 自适应去噪器句柄 */
typedef struct voice_adaptive_denoiser_s voice_adaptive_denoiser_t;

/** 自适应配置 */
typedef struct {
    voice_denoiser_config_t base;       /**< 基础配置 */
    float cpu_threshold_high;           /**< CPU阈值(降级) */
    float cpu_threshold_low;            /**< CPU阈值(升级) */
    int battery_threshold;              /**< 电量阈值 */
    uint32_t switch_interval_ms;        /**< 切换检测间隔 */
} voice_adaptive_denoiser_config_t;

/**
 * @brief 初始化自适应配置
 * @param config 配置结构指针
 */
void voice_adaptive_denoiser_config_init(voice_adaptive_denoiser_config_t *config);

/**
 * @brief 创建自适应去噪器
 * @param config 配置
 * @return 去噪器句柄
 */
voice_adaptive_denoiser_t *voice_adaptive_denoiser_create(
    const voice_adaptive_denoiser_config_t *config
);

/**
 * @brief 销毁自适应去噪器
 * @param denoiser 去噪器句柄
 */
void voice_adaptive_denoiser_destroy(voice_adaptive_denoiser_t *denoiser);

/**
 * @brief 处理音频帧
 * @param denoiser 去噪器句柄
 * @param samples 输入/输出音频样本
 * @param count 样本数
 * @return 语音概率
 */
float voice_adaptive_denoiser_process(
    voice_adaptive_denoiser_t *denoiser,
    int16_t *samples,
    size_t count
);

/**
 * @brief 获取当前使用的引擎
 * @param denoiser 去噪器句柄
 * @return 引擎类型
 */
voice_denoise_engine_t voice_adaptive_denoiser_get_engine(
    voice_adaptive_denoiser_t *denoiser
);

#ifdef __cplusplus
}
#endif

#endif /* DSP_DENOISER_H */
