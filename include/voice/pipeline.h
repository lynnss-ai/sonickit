/**
 * @file pipeline.h
 * @brief Audio processing pipeline interface
 * 
 * 完整的音频处理流水线:
 * 采集 -> 重采样 -> AEC -> 降噪 -> AGC -> 编码 -> RTP -> 网络
 * 网络 -> RTP -> 抖动缓冲 -> 解码 -> 重采样 -> 播放
 */

#ifndef VOICE_PIPELINE_H
#define VOICE_PIPELINE_H

#include "voice/types.h"
#include "voice/error.h"
#include "audio/device.h"
#include "audio/audio_buffer.h"
#include "dsp/resampler.h"
#include "dsp/denoiser.h"
#include "dsp/echo_canceller.h"
#include "codec/codec.h"
#include "network/rtp.h"
#include "network/srtp.h"
#include "network/jitter_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 管线类型
 * ============================================ */

typedef struct voice_pipeline_s voice_pipeline_t;

/** 管线模式 */
typedef enum {
    PIPELINE_MODE_CAPTURE,      /**< 仅采集 */
    PIPELINE_MODE_PLAYBACK,     /**< 仅播放 */
    PIPELINE_MODE_DUPLEX,       /**< 双工 */
    PIPELINE_MODE_LOOPBACK,     /**< 回环测试 */
} pipeline_mode_t;

/** 管线状态 */
typedef enum {
    PIPELINE_STATE_STOPPED,
    PIPELINE_STATE_STARTING,
    PIPELINE_STATE_RUNNING,
    PIPELINE_STATE_STOPPING,
    PIPELINE_STATE_ERROR,
} pipeline_state_t;

/* ============================================
 * 管线配置
 * ============================================ */

typedef struct {
    pipeline_mode_t mode;
    
    /* 音频设备配置 */
    uint32_t sample_rate;           /**< 内部采样率 */
    uint8_t channels;               /**< 通道数 */
    uint32_t frame_duration_ms;     /**< 帧时长 (ms) */
    const char *capture_device;     /**< 采集设备ID (NULL=默认) */
    const char *playback_device;    /**< 播放设备ID (NULL=默认) */
    
    /* DSP 配置 */
    bool enable_aec;                /**< 启用AEC */
    bool enable_denoise;            /**< 启用降噪 */
    bool enable_agc;                /**< 启用AGC */
    voice_denoise_engine_t denoise_engine; /**< 降噪引擎 */
    int denoise_level;              /**< 降噪级别 (0-100) */
    
    /* 编解码器配置 */
    voice_codec_id_t codec;         /**< 编解码器 */
    uint32_t bitrate;               /**< 比特率 */
    bool enable_fec;                /**< 启用FEC */
    
    /* 网络配置 */
    bool enable_srtp;               /**< 启用SRTP */
    srtp_profile_t srtp_profile;    /**< SRTP配置 */
    
    /* Jitter Buffer 配置 */
    uint32_t jitter_min_delay_ms;
    uint32_t jitter_max_delay_ms;
} voice_pipeline_config_t;

/* ============================================
 * 回调函数
 * ============================================ */

/** 编码后数据回调 (用于发送) */
typedef void (*pipeline_encoded_callback_t)(
    voice_pipeline_t *pipeline,
    const uint8_t *data,
    size_t size,
    uint32_t timestamp,
    void *user_data
);

/** 解码后数据回调 */
typedef void (*pipeline_decoded_callback_t)(
    voice_pipeline_t *pipeline,
    const int16_t *pcm,
    size_t samples,
    void *user_data
);

/** 状态变化回调 */
typedef void (*pipeline_state_callback_t)(
    voice_pipeline_t *pipeline,
    pipeline_state_t state,
    void *user_data
);

/** 错误回调 */
typedef void (*pipeline_error_callback_t)(
    voice_pipeline_t *pipeline,
    voice_error_t error,
    const char *message,
    void *user_data
);

/* ============================================
 * 管线统计
 * ============================================ */

typedef struct {
    /* 采集统计 */
    uint64_t frames_captured;
    uint64_t frames_dropped_capture;
    
    /* 播放统计 */
    uint64_t frames_played;
    uint64_t frames_dropped_playback;
    
    /* 编解码统计 */
    uint64_t frames_encoded;
    uint64_t frames_decoded;
    
    /* 网络统计 */
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_lost;
    float packet_loss_rate;
    uint32_t jitter_ms;
    uint32_t rtt_ms;
    
    /* 音频质量 */
    float capture_level_db;
    float playback_level_db;
    bool vad_active;
} voice_pipeline_stats_t;

/* ============================================
 * 管线 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_pipeline_config_init(voice_pipeline_config_t *config);

/**
 * @brief 创建管线
 */
voice_pipeline_t *voice_pipeline_create(const voice_pipeline_config_t *config);

/**
 * @brief 销毁管线
 */
void voice_pipeline_destroy(voice_pipeline_t *pipeline);

/**
 * @brief 启动管线
 */
voice_error_t voice_pipeline_start(voice_pipeline_t *pipeline);

/**
 * @brief 停止管线
 */
voice_error_t voice_pipeline_stop(voice_pipeline_t *pipeline);

/**
 * @brief 获取状态
 */
pipeline_state_t voice_pipeline_get_state(voice_pipeline_t *pipeline);

/**
 * @brief 设置编码数据回调
 */
void voice_pipeline_set_encoded_callback(
    voice_pipeline_t *pipeline,
    pipeline_encoded_callback_t callback,
    void *user_data
);

/**
 * @brief 设置解码数据回调
 */
void voice_pipeline_set_decoded_callback(
    voice_pipeline_t *pipeline,
    pipeline_decoded_callback_t callback,
    void *user_data
);

/**
 * @brief 设置状态回调
 */
void voice_pipeline_set_state_callback(
    voice_pipeline_t *pipeline,
    pipeline_state_callback_t callback,
    void *user_data
);

/**
 * @brief 设置错误回调
 */
void voice_pipeline_set_error_callback(
    voice_pipeline_t *pipeline,
    pipeline_error_callback_t callback,
    void *user_data
);

/**
 * @brief 输入接收的网络数据
 */
voice_error_t voice_pipeline_receive_packet(
    voice_pipeline_t *pipeline,
    const uint8_t *data,
    size_t size
);

/**
 * @brief 输入本地PCM数据 (不使用设备采集时)
 */
voice_error_t voice_pipeline_push_capture(
    voice_pipeline_t *pipeline,
    const int16_t *pcm,
    size_t samples
);

/**
 * @brief 获取播放PCM数据 (不使用设备播放时)
 */
voice_error_t voice_pipeline_pull_playback(
    voice_pipeline_t *pipeline,
    int16_t *pcm,
    size_t samples,
    size_t *samples_out
);

/**
 * @brief 获取统计信息
 */
voice_error_t voice_pipeline_get_stats(
    voice_pipeline_t *pipeline,
    voice_pipeline_stats_t *stats
);

/**
 * @brief 重置统计
 */
void voice_pipeline_reset_stats(voice_pipeline_t *pipeline);

/* ============================================
 * 管线控制
 * ============================================ */

/**
 * @brief 启用/禁用AEC
 */
voice_error_t voice_pipeline_set_aec_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief 启用/禁用降噪
 */
voice_error_t voice_pipeline_set_denoise_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief 设置降噪级别
 */
voice_error_t voice_pipeline_set_denoise_level(
    voice_pipeline_t *pipeline,
    int level
);

/**
 * @brief 启用/禁用AGC
 */
voice_error_t voice_pipeline_set_agc_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief 设置编码比特率
 */
voice_error_t voice_pipeline_set_bitrate(
    voice_pipeline_t *pipeline,
    uint32_t bitrate
);

/**
 * @brief 静音采集
 */
voice_error_t voice_pipeline_set_capture_muted(
    voice_pipeline_t *pipeline,
    bool muted
);

/**
 * @brief 静音播放
 */
voice_error_t voice_pipeline_set_playback_muted(
    voice_pipeline_t *pipeline,
    bool muted
);

/**
 * @brief 设置播放音量
 */
voice_error_t voice_pipeline_set_playback_volume(
    voice_pipeline_t *pipeline,
    float volume  /**< 0.0 - 1.0 */
);

/* ============================================
 * SRTP 密钥设置
 * ============================================ */

/**
 * @brief 设置SRTP发送密钥
 */
voice_error_t voice_pipeline_set_srtp_send_key(
    voice_pipeline_t *pipeline,
    const uint8_t *key,
    size_t key_len,
    const uint8_t *salt,
    size_t salt_len
);

/**
 * @brief 设置SRTP接收密钥
 */
voice_error_t voice_pipeline_set_srtp_recv_key(
    voice_pipeline_t *pipeline,
    const uint8_t *key,
    size_t key_len,
    const uint8_t *salt,
    size_t salt_len
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_PIPELINE_H */
