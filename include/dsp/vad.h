/**
 * @file vad.h
 * @brief Voice Activity Detection (VAD) interface
 * 
 * 语音活动检测模块，支持多种VAD算法
 */

#ifndef VOICE_VAD_H
#define VOICE_VAD_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * VAD 类型定义
 * ============================================ */

typedef struct voice_vad_s voice_vad_t;

/** VAD 算法类型 */
typedef enum {
    VOICE_VAD_ENERGY,      /**< 基于能量阈值 */
    VOICE_VAD_SPEEX,       /**< SpeexDSP VAD */
    VOICE_VAD_WEBRTC,      /**< WebRTC VAD (如果可用) */
    VOICE_VAD_RNNOISE,     /**< RNNoise VAD (基于语音概率) */
} voice_vad_mode_t;

/** VAD 激进程度 */
typedef enum {
    VOICE_VAD_QUALITY = 0,    /**< 高质量 - 较少误判 */
    VOICE_VAD_LOW_BITRATE,    /**< 低比特率 - 平衡 */
    VOICE_VAD_AGGRESSIVE,     /**< 激进 - 快速响应 */
    VOICE_VAD_VERY_AGGRESSIVE /**< 非常激进 - 可能丢失语音开头 */
} voice_vad_aggressiveness_t;

/* ============================================
 * VAD 配置
 * ============================================ */

typedef struct {
    voice_vad_mode_t mode;
    voice_vad_aggressiveness_t aggressiveness;
    uint32_t sample_rate;
    uint32_t frame_size_ms;     /**< 帧时长 (10/20/30 ms) */
    
    /* 能量VAD参数 */
    float energy_threshold_db;   /**< 能量阈值 (dB) */
    float speech_hangover_ms;    /**< 语音结束后的延迟 (ms) */
    
    /* 自适应参数 */
    bool adaptive_threshold;     /**< 自适应阈值 */
    float adaptation_rate;       /**< 自适应速率 */
} voice_vad_config_t;

/* ============================================
 * VAD 结果
 * ============================================ */

typedef struct {
    bool is_speech;             /**< 是否包含语音 */
    float speech_probability;   /**< 语音概率 [0, 1] */
    float energy_db;            /**< 帧能量 (dB) */
    uint32_t speech_frames;     /**< 连续语音帧数 */
    uint32_t silence_frames;    /**< 连续静音帧数 */
} voice_vad_result_t;

/* ============================================
 * VAD API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_vad_config_init(voice_vad_config_t *config);

/**
 * @brief 创建VAD实例
 */
voice_vad_t *voice_vad_create(const voice_vad_config_t *config);

/**
 * @brief 销毁VAD实例
 */
void voice_vad_destroy(voice_vad_t *vad);

/**
 * @brief 处理音频帧
 * @param vad VAD实例
 * @param samples 音频样本 (16-bit PCM)
 * @param num_samples 样本数
 * @param result 输出结果
 * @return 错误码
 */
voice_error_t voice_vad_process(
    voice_vad_t *vad,
    const int16_t *samples,
    size_t num_samples,
    voice_vad_result_t *result
);

/**
 * @brief 重置VAD状态
 */
void voice_vad_reset(voice_vad_t *vad);

/**
 * @brief 设置激进程度
 */
voice_error_t voice_vad_set_aggressiveness(
    voice_vad_t *vad,
    voice_vad_aggressiveness_t aggressiveness
);

/**
 * @brief 获取VAD统计信息
 */
typedef struct {
    uint64_t total_frames;
    uint64_t speech_frames;
    uint64_t silence_frames;
    float average_speech_probability;
    float speech_ratio;         /**< 语音占比 */
} voice_vad_stats_t;

voice_error_t voice_vad_get_stats(
    voice_vad_t *vad,
    voice_vad_stats_t *stats
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_VAD_H */
