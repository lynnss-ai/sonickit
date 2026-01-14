/**
 * @file config.h
 * @brief Voice library configuration structures
 */

#ifndef VOICE_CONFIG_H
#define VOICE_CONFIG_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 全局配置
 * ============================================ */

typedef struct {
    voice_log_level_t log_level;        /**< 日志级别 */
    bool enable_performance_monitor;     /**< 启用性能监控 */
    bool enable_adaptive_denoise;        /**< 启用自适应去噪切换 */
    uint32_t thread_priority;            /**< 音频线程优先级 */
} voice_global_config_t;

/* ============================================
 * 音频设备配置
 * ============================================ */

typedef struct {
    uint32_t sample_rate;               /**< 采样率 */
    uint8_t channels;                   /**< 通道数 */
    voice_format_t format;              /**< 采样格式 */
    uint32_t frame_size_ms;             /**< 帧时长(毫秒) */
    uint32_t buffer_size_ms;            /**< 缓冲区时长(毫秒) */
    const char *device_id;              /**< 设备ID (NULL=默认) */
} voice_device_config_t;

/* ============================================
 * 编解码器配置
 * ============================================ */

typedef struct {
    voice_codec_type_t type;            /**< 编解码器类型 */
    uint32_t sample_rate;               /**< 采样率 */
    uint8_t channels;                   /**< 通道数 */
    uint32_t bitrate;                   /**< 比特率 (bps) */
    uint32_t frame_size_ms;             /**< 帧时长(毫秒) */
    uint8_t complexity;                 /**< 复杂度 (0-10) */
    bool enable_dtx;                    /**< 启用不连续传输 */
    bool enable_fec;                    /**< 启用前向纠错 */
    bool enable_plc;                    /**< 启用丢包补偿 */
} voice_codec_config_t;

/* ============================================
 * 去噪配置
 * ============================================ */

typedef struct {
    voice_denoise_engine_t engine;      /**< 去噪引擎 */
    int noise_suppress_db;              /**< 噪声抑制量 (负dB) */
    bool enable_agc;                    /**< 启用自动增益 */
    bool enable_vad;                    /**< 启用语音检测 */
    
    /* 自适应切换参数 */
    float cpu_threshold_high;           /**< CPU阈值(切换到SpeexDSP) */
    float cpu_threshold_low;            /**< CPU阈值(切换到RNNoise) */
    int battery_threshold;              /**< 电量阈值 */
} voice_denoise_config_t;

/* ============================================
 * 回声消除配置
 * ============================================ */

typedef struct {
    bool enabled;                       /**< 是否启用 */
    uint32_t frame_size;                /**< 帧大小(样本数) */
    uint32_t filter_length;             /**< 滤波器长度(样本数) */
    int echo_suppress_db;               /**< 回声抑制量 (负dB) */
    int echo_suppress_active_db;        /**< 近端活跃时抑制量 */
} voice_aec_config_t;

/* ============================================
 * 重采样配置
 * ============================================ */

typedef struct {
    uint32_t input_rate;                /**< 输入采样率 */
    uint32_t output_rate;               /**< 输出采样率 */
    uint8_t channels;                   /**< 通道数 */
    int quality;                        /**< 质量 (0-10) */
} voice_resampler_config_t;

/* ============================================
 * 网络传输配置
 * ============================================ */

typedef struct {
    const char *local_ip;               /**< 本地IP */
    uint16_t local_port;                /**< 本地端口 */
    const char *remote_ip;              /**< 远端IP */
    uint16_t remote_port;               /**< 远端端口 */
    bool enable_rtcp;                   /**< 启用RTCP */
    uint32_t rtcp_interval_ms;          /**< RTCP发送间隔 */
} voice_network_config_t;

/* ============================================
 * Jitter Buffer 配置
 * ============================================ */

typedef struct {
    uint32_t min_delay_ms;              /**< 最小延迟 */
    uint32_t max_delay_ms;              /**< 最大延迟 */
    uint32_t target_delay_ms;           /**< 目标延迟 */
    bool adaptive;                      /**< 自适应模式 */
    uint32_t max_packets;               /**< 最大缓存包数 */
} voice_jitter_buffer_config_t;

/* ============================================
 * SRTP 加密配置
 * ============================================ */

typedef struct {
    bool enabled;                       /**< 是否启用 */
    voice_crypto_suite_t suite;         /**< 加密套件 */
    voice_key_exchange_t key_exchange;  /**< 密钥交换方式 */
    
    /* 预共享密钥模式 */
    const uint8_t *master_key;          /**< 主密钥 */
    size_t master_key_len;              /**< 密钥长度 */
    const uint8_t *master_salt;         /**< 主盐 */
    size_t master_salt_len;             /**< 盐长度 */
    
    /* DTLS 模式 */
    const char *cert_file;              /**< 证书文件 */
    const char *key_file;               /**< 私钥文件 */
    const char *ca_file;                /**< CA证书文件 */
    bool verify_peer;                   /**< 验证对端 */
} voice_srtp_config_t;

/* ============================================
 * 文件 I/O 配置
 * ============================================ */

typedef struct {
    const char *path;                   /**< 文件路径 */
    voice_format_t format;              /**< 输出格式 */
    uint32_t sample_rate;               /**< 输出采样率 */
    uint8_t channels;                   /**< 输出通道数 */
} voice_file_config_t;

/* ============================================
 * 音频处理管线配置
 * ============================================ */

typedef struct {
    /* 设备配置 */
    voice_device_config_t capture;      /**< 采集设备配置 */
    voice_device_config_t playback;     /**< 播放设备配置 */
    
    /* DSP 配置 */
    voice_denoise_config_t denoise;     /**< 去噪配置 */
    voice_aec_config_t aec;             /**< 回声消除配置 */
    voice_resampler_config_t resampler; /**< 重采样配置 */
    
    /* 编解码配置 */
    voice_codec_config_t codec;         /**< 编解码器配置 */
    
    /* 网络配置 */
    voice_network_config_t network;     /**< 网络配置 */
    voice_jitter_buffer_config_t jitter_buffer; /**< Jitter Buffer配置 */
    voice_srtp_config_t srtp;           /**< SRTP配置 */
    
    /* 回调 */
    voice_audio_callback_t on_capture;  /**< 采集回调 */
    voice_audio_callback_t on_playback; /**< 播放回调 */
    voice_event_callback_t on_event;    /**< 事件回调 */
    void *user_data;                    /**< 用户数据 */
} voice_pipeline_config_t;

/* ============================================
 * 默认配置初始化
 * ============================================ */

/**
 * @brief 初始化默认全局配置
 * @param config 配置结构指针
 */
void voice_config_init_global(voice_global_config_t *config);

/**
 * @brief 初始化默认设备配置
 * @param config 配置结构指针
 */
void voice_config_init_device(voice_device_config_t *config);

/**
 * @brief 初始化默认编解码器配置
 * @param config 配置结构指针
 * @param type 编解码器类型
 */
void voice_config_init_codec(voice_codec_config_t *config, voice_codec_type_t type);

/**
 * @brief 初始化默认去噪配置
 * @param config 配置结构指针
 */
void voice_config_init_denoise(voice_denoise_config_t *config);

/**
 * @brief 初始化默认回声消除配置
 * @param config 配置结构指针
 */
void voice_config_init_aec(voice_aec_config_t *config);

/**
 * @brief 初始化默认Jitter Buffer配置
 * @param config 配置结构指针
 */
void voice_config_init_jitter_buffer(voice_jitter_buffer_config_t *config);

/**
 * @brief 初始化默认SRTP配置
 * @param config 配置结构指针
 */
void voice_config_init_srtp(voice_srtp_config_t *config);

/**
 * @brief 初始化默认管线配置
 * @param config 配置结构指针
 */
void voice_config_init_pipeline(voice_pipeline_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_CONFIG_H */
