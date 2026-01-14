/**
 * @file resampler.h
 * @brief Audio resampler interface (SpeexDSP)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef DSP_RESAMPLER_H
#define DSP_RESAMPLER_H

#include "voice/types.h"
#include "voice/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 重采样器
 * ============================================ */

/** 重采样器句柄 */
typedef struct voice_resampler_s voice_resampler_t;

/** 重采样质量 */
typedef enum {
    VOICE_RESAMPLE_QUALITY_FAST = 0,        /**< 最快 (质量0) */
    VOICE_RESAMPLE_QUALITY_LOW = 2,         /**< 低质量 */
    VOICE_RESAMPLE_QUALITY_MEDIUM = 4,      /**< 中等质量 */
    VOICE_RESAMPLE_QUALITY_DEFAULT = 5,     /**< 默认 (推荐) */
    VOICE_RESAMPLE_QUALITY_HIGH = 7,        /**< 高质量 */
    VOICE_RESAMPLE_QUALITY_BEST = 10        /**< 最佳质量 */
} voice_resample_quality_t;

/**
 * @brief 创建重采样器
 * @param channels 通道数
 * @param in_rate 输入采样率
 * @param out_rate 输出采样率
 * @param quality 质量等级 (0-10)
 * @return 重采样器句柄
 */
voice_resampler_t *voice_resampler_create(
    uint8_t channels,
    uint32_t in_rate,
    uint32_t out_rate,
    int quality
);

/**
 * @brief 销毁重采样器
 * @param rs 重采样器句柄
 */
void voice_resampler_destroy(voice_resampler_t *rs);

/**
 * @brief 处理int16格式音频
 * @param rs 重采样器句柄
 * @param in 输入数据
 * @param in_frames 输入帧数 (每通道样本数)
 * @param out 输出缓冲区
 * @param out_frames 输出缓冲区可容纳的帧数
 * @return 实际输出的帧数，负数表示错误
 */
int voice_resampler_process_int16(
    voice_resampler_t *rs,
    const int16_t *in,
    size_t in_frames,
    int16_t *out,
    size_t out_frames
);

/**
 * @brief 处理float格式音频
 * @param rs 重采样器句柄
 * @param in 输入数据
 * @param in_frames 输入帧数 (每通道样本数)
 * @param out 输出缓冲区
 * @param out_frames 输出缓冲区可容纳的帧数
 * @return 实际输出的帧数，负数表示错误
 */
int voice_resampler_process_float(
    voice_resampler_t *rs,
    const float *in,
    size_t in_frames,
    float *out,
    size_t out_frames
);

/**
 * @brief 设置采样率
 * @param rs 重采样器句柄
 * @param in_rate 输入采样率
 * @param out_rate 输出采样率
 * @return 错误码
 */
voice_error_t voice_resampler_set_rate(
    voice_resampler_t *rs,
    uint32_t in_rate,
    uint32_t out_rate
);

/**
 * @brief 设置质量
 * @param rs 重采样器句柄
 * @param quality 质量等级 (0-10)
 * @return 错误码
 */
voice_error_t voice_resampler_set_quality(voice_resampler_t *rs, int quality);

/**
 * @brief 获取输入延迟
 * @param rs 重采样器句柄
 * @return 输入延迟(样本数)
 */
int voice_resampler_get_input_latency(voice_resampler_t *rs);

/**
 * @brief 获取输出延迟
 * @param rs 重采样器句柄
 * @return 输出延迟(样本数)
 */
int voice_resampler_get_output_latency(voice_resampler_t *rs);

/**
 * @brief 重置重采样器状态
 * @param rs 重采样器句柄
 */
void voice_resampler_reset(voice_resampler_t *rs);

/**
 * @brief 计算输出帧数
 * @param rs 重采样器句柄
 * @param in_frames 输入帧数
 * @return 预期输出帧数
 */
size_t voice_resampler_get_output_frames(voice_resampler_t *rs, size_t in_frames);

/**
 * @brief 计算输入帧数
 * @param rs 重采样器句柄
 * @param out_frames 需要的输出帧数
 * @return 需要的输入帧数
 */
size_t voice_resampler_get_input_frames(voice_resampler_t *rs, size_t out_frames);

#ifdef __cplusplus
}
#endif

#endif /* DSP_RESAMPLER_H */
