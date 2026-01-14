/**
 * @file audio_recorder.h
 * @brief Audio recording and playback
 * 
 * 音频录制模块，支持录制到文件或内存缓冲区
 */

#ifndef VOICE_AUDIO_RECORDER_H
#define VOICE_AUDIO_RECORDER_H

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

typedef struct voice_recorder_s voice_recorder_t;
typedef struct voice_player_s voice_player_t;

/** 录制格式 */
typedef enum {
    VOICE_RECORD_WAV,           /**< WAV 文件 */
    VOICE_RECORD_RAW,           /**< 原始 PCM */
    VOICE_RECORD_OGG_OPUS,      /**< Ogg Opus */
    VOICE_RECORD_MEMORY,        /**< 内存缓冲区 */
} voice_record_format_t;

/** 录制源 */
typedef enum {
    VOICE_RECORD_SOURCE_INPUT,  /**< 输入 (麦克风) */
    VOICE_RECORD_SOURCE_OUTPUT, /**< 输出 (扬声器) */
    VOICE_RECORD_SOURCE_BOTH,   /**< 双向 (混合) */
} voice_record_source_t;

/* ============================================
 * 录制器配置
 * ============================================ */

typedef struct {
    voice_record_format_t format;
    voice_record_source_t source;
    
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    
    /* 文件录制 */
    const char *filename;           /**< 输出文件名 (如果录制到文件) */
    bool append;                    /**< 追加到现有文件 */
    
    /* 内存录制 */
    size_t max_memory_bytes;        /**< 最大内存使用 (0=无限制) */
    bool circular_buffer;           /**< 使用循环缓冲区 */
    
    /* 限制 */
    uint64_t max_duration_ms;       /**< 最大录制时长 (0=无限制) */
    uint64_t max_file_size;         /**< 最大文件大小 (0=无限制) */
    
    /* 回调 */
    void (*on_data)(const int16_t *samples, size_t count, void *user_data);
    void (*on_complete)(const char *filename, uint64_t duration_ms, void *user_data);
    void *callback_user_data;
} voice_recorder_config_t;

/* ============================================
 * 录制状态
 * ============================================ */

typedef struct {
    bool is_recording;
    uint64_t duration_ms;           /**< 已录制时长 */
    uint64_t samples_recorded;      /**< 已录制样本数 */
    uint64_t bytes_written;         /**< 已写入字节数 */
    float peak_level_db;            /**< 峰值电平 */
    float avg_level_db;             /**< 平均电平 */
} voice_recorder_status_t;

/* ============================================
 * 播放器配置
 * ============================================ */

typedef struct {
    uint32_t sample_rate;           /**< 目标采样率 (0=使用文件采样率) */
    float playback_speed;           /**< 播放速度 (1.0=正常) */
    float volume;                   /**< 音量 (0.0-1.0) */
    bool loop;                      /**< 循环播放 */
    
    /* 回调 */
    void (*on_complete)(void *user_data);
    void (*on_position)(uint64_t position_ms, uint64_t duration_ms, void *user_data);
    void *callback_user_data;
} voice_player_config_t;

/* ============================================
 * 播放状态
 * ============================================ */

typedef struct {
    bool is_playing;
    bool is_paused;
    uint64_t position_ms;           /**< 当前位置 */
    uint64_t duration_ms;           /**< 总时长 */
    float volume;                   /**< 当前音量 */
    float playback_speed;           /**< 当前速度 */
} voice_player_status_t;

/* ============================================
 * 录制器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_recorder_config_init(voice_recorder_config_t *config);

/**
 * @brief 创建录制器
 */
voice_recorder_t *voice_recorder_create(const voice_recorder_config_t *config);

/**
 * @brief 销毁录制器
 */
void voice_recorder_destroy(voice_recorder_t *recorder);

/**
 * @brief 开始录制
 */
voice_error_t voice_recorder_start(voice_recorder_t *recorder);

/**
 * @brief 停止录制
 */
voice_error_t voice_recorder_stop(voice_recorder_t *recorder);

/**
 * @brief 暂停录制
 */
voice_error_t voice_recorder_pause(voice_recorder_t *recorder);

/**
 * @brief 恢复录制
 */
voice_error_t voice_recorder_resume(voice_recorder_t *recorder);

/**
 * @brief 写入音频数据
 */
voice_error_t voice_recorder_write(
    voice_recorder_t *recorder,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief 获取录制状态
 */
voice_error_t voice_recorder_get_status(
    voice_recorder_t *recorder,
    voice_recorder_status_t *status
);

/**
 * @brief 获取录制的数据 (内存模式)
 */
voice_error_t voice_recorder_get_data(
    voice_recorder_t *recorder,
    int16_t **samples,
    size_t *num_samples
);

/**
 * @brief 保存到文件 (内存模式)
 */
voice_error_t voice_recorder_save_to_file(
    voice_recorder_t *recorder,
    const char *filename,
    voice_record_format_t format
);

/* ============================================
 * 播放器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_player_config_init(voice_player_config_t *config);

/**
 * @brief 从文件创建播放器
 */
voice_player_t *voice_player_create_from_file(
    const char *filename,
    const voice_player_config_t *config
);

/**
 * @brief 从内存创建播放器
 */
voice_player_t *voice_player_create_from_memory(
    const int16_t *samples,
    size_t num_samples,
    uint32_t sample_rate,
    const voice_player_config_t *config
);

/**
 * @brief 销毁播放器
 */
void voice_player_destroy(voice_player_t *player);

/**
 * @brief 开始播放
 */
voice_error_t voice_player_play(voice_player_t *player);

/**
 * @brief 停止播放
 */
voice_error_t voice_player_stop(voice_player_t *player);

/**
 * @brief 暂停播放
 */
voice_error_t voice_player_pause(voice_player_t *player);

/**
 * @brief 恢复播放
 */
voice_error_t voice_player_resume(voice_player_t *player);

/**
 * @brief 跳转到指定位置
 */
voice_error_t voice_player_seek(voice_player_t *player, uint64_t position_ms);

/**
 * @brief 设置音量
 */
voice_error_t voice_player_set_volume(voice_player_t *player, float volume);

/**
 * @brief 设置播放速度
 */
voice_error_t voice_player_set_speed(voice_player_t *player, float speed);

/**
 * @brief 读取音频数据 (供 Pipeline 使用)
 */
size_t voice_player_read(
    voice_player_t *player,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief 获取播放状态
 */
voice_error_t voice_player_get_status(
    voice_player_t *player,
    voice_player_status_t *status
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_RECORDER_H */
