/**
 * @file denoiser.h
 * @brief Audio denoiser interface (SpeexDSP + RNNoise)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef DSP_DENOISER_H
#define DSP_DENOISER_H

#include "voice/types.h"
#include "voice/error.h"

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
 * @brief 初始化默认配置
 * @param config 配置结构指针
 */
void voice_denoiser_config_init(voice_denoiser_config_t *config);

/**
 * @brief 创建去噪器
 * @param config 配置
 * @return 去噪器句柄
 */
voice_denoiser_t *voice_denoiser_create(const voice_denoiser_config_t *config);

/**
 * @brief 销毁去噪器
 * @param denoiser 去噪器句柄
 */
void voice_denoiser_destroy(voice_denoiser_t *denoiser);

/**
 * @brief 处理音频帧 (int16)
 * @param denoiser 去噪器句柄
 * @param samples 输入/输出音频样本 (原地处理)
 * @param count 样本数
 * @return 语音概率 (0.0-1.0), 负数表示错误
 */
float voice_denoiser_process_int16(
    voice_denoiser_t *denoiser,
    int16_t *samples,
    size_t count
);

/**
 * @brief 处理音频帧 (float)
 * @param denoiser 去噪器句柄
 * @param samples 输入/输出音频样本 (原地处理)
 * @param count 样本数
 * @return 语音概率 (0.0-1.0), 负数表示错误
 */
float voice_denoiser_process_float(
    voice_denoiser_t *denoiser,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频帧 (通用宏，默认使用 int16)
 * @param denoiser 去噪器句柄
 * @param samples 输入/输出音频样本 (原地处理)
 * @param count 样本数
 * @return 语音概率 (0.0-1.0), 负数表示错误
 */
#define voice_denoiser_process(denoiser, samples, count) \
    voice_denoiser_process_int16((denoiser), (samples), (count))

/**
 * @brief 设置噪声抑制量
 * @param denoiser 去噪器句柄
 * @param db 抑制量 (负dB)
 * @return 错误码
 */
voice_error_t voice_denoiser_set_noise_suppress(voice_denoiser_t *denoiser, int db);

/**
 * @brief 启用/禁用去噪
 * @param denoiser 去噪器句柄
 * @param enabled 是否启用
 * @return 错误码
 */
voice_error_t voice_denoiser_set_enabled(voice_denoiser_t *denoiser, bool enabled);

/**
 * @brief 获取当前引擎类型
 * @param denoiser 去噪器句柄
 * @return 引擎类型
 */
voice_denoise_engine_t voice_denoiser_get_engine(voice_denoiser_t *denoiser);

/**
 * @brief 切换去噪引擎
 * @param denoiser 去噪器句柄
 * @param engine 目标引擎
 * @return 错误码
 */
voice_error_t voice_denoiser_switch_engine(
    voice_denoiser_t *denoiser,
    voice_denoise_engine_t engine
);

/**
 * @brief 重置去噪器状态
 * @param denoiser 去噪器句柄
 */
void voice_denoiser_reset(voice_denoiser_t *denoiser);

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
