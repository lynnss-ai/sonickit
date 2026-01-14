/**
 * @file audio_mixer.h
 * @brief Audio mixer for multi-stream mixing
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 音频混音器，支持多路音频流混合
 */

#ifndef VOICE_AUDIO_MIXER_H
#define VOICE_AUDIO_MIXER_H

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

typedef struct voice_mixer_s voice_mixer_t;
typedef uint32_t mixer_source_id_t;

#define MIXER_INVALID_SOURCE_ID 0

/** 混音算法 */
typedef enum {
    VOICE_MIX_SIMPLE_ADD,       /**< 简单相加 (可能溢出) */
    VOICE_MIX_AVERAGE,          /**< 平均值 */
    VOICE_MIX_NORMALIZED,       /**< 归一化混音 (推荐) */
    VOICE_MIX_LOUDEST_N,        /**< 仅混合最大的N个 */
} voice_mix_algorithm_t;

/* ============================================
 * 混音器配置
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t frame_size;        /**< 每帧样本数 */
    size_t max_sources;         /**< 最大音源数 */
    voice_mix_algorithm_t algorithm;
    
    /* Loudest N 参数 */
    size_t loudest_n;           /**< 混合最大的N个源 */
    
    /* 输出控制 */
    float master_gain;          /**< 主增益 (0.0-2.0) */
    bool enable_limiter;        /**< 启用限制器防止削波 */
    float limiter_threshold_db; /**< 限制器阈值 */
} voice_mixer_config_t;

/* ============================================
 * 音源配置
 * ============================================ */

typedef struct {
    float gain;                 /**< 音源增益 (0.0-2.0) */
    float pan;                  /**< 声像 (-1.0左, 0中, 1.0右) */
    bool muted;                 /**< 是否静音 */
    uint32_t priority;          /**< 优先级 (用于Loudest N) */
    void *user_data;            /**< 用户数据 */
} voice_mixer_source_config_t;

/* ============================================
 * 混音器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_mixer_config_init(voice_mixer_config_t *config);

/**
 * @brief 创建混音器
 */
voice_mixer_t *voice_mixer_create(const voice_mixer_config_t *config);

/**
 * @brief 销毁混音器
 */
void voice_mixer_destroy(voice_mixer_t *mixer);

/**
 * @brief 添加音源
 * @return 音源ID，失败返回 MIXER_INVALID_SOURCE_ID
 */
mixer_source_id_t voice_mixer_add_source(
    voice_mixer_t *mixer,
    const voice_mixer_source_config_t *config
);

/**
 * @brief 移除音源
 */
voice_error_t voice_mixer_remove_source(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id
);

/**
 * @brief 向音源推送音频数据
 */
voice_error_t voice_mixer_push_audio(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief 混合并获取输出
 * @param mixer 混音器
 * @param output 输出缓冲区
 * @param num_samples 请求的样本数
 * @param samples_out 实际输出的样本数
 */
voice_error_t voice_mixer_get_output(
    voice_mixer_t *mixer,
    int16_t *output,
    size_t num_samples,
    size_t *samples_out
);

/**
 * @brief 设置音源增益
 */
voice_error_t voice_mixer_set_source_gain(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    float gain
);

/**
 * @brief 设置音源静音
 */
voice_error_t voice_mixer_set_source_muted(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    bool muted
);

/**
 * @brief 设置主增益
 */
voice_error_t voice_mixer_set_master_gain(
    voice_mixer_t *mixer,
    float gain
);

/**
 * @brief 获取活跃音源数
 */
size_t voice_mixer_get_active_sources(voice_mixer_t *mixer);

/**
 * @brief 混音器统计
 */
typedef struct {
    size_t active_sources;
    size_t total_sources;
    uint64_t frames_mixed;
    float peak_level_db;
    uint32_t clip_count;        /**< 削波次数 */
} voice_mixer_stats_t;

voice_error_t voice_mixer_get_stats(
    voice_mixer_t *mixer,
    voice_mixer_stats_t *stats
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_MIXER_H */
