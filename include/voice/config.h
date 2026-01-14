/**
 * @file config.h
 * @brief Voice library configuration structures
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef VOICE_CONFIG_H
#define VOICE_CONFIG_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Global Configuration
 * ============================================ */

typedef struct {
    voice_log_level_t log_level;        /**< Log level */
    bool enable_performance_monitor;     /**< Enable performance monitoring */
    bool enable_adaptive_denoise;        /**< Enable adaptive denoising switching */
    uint32_t thread_priority;            /**< Audio thread priority */
} voice_global_config_t;

/* ============================================
 * Audio Device Configuration
 * ============================================ */

typedef struct {
    uint32_t sample_rate;               /**< Sample rate */
    uint8_t channels;                   /**< Number of channels */
    voice_format_t format;              /**< Sample format */
    uint32_t frame_size_ms;             /**< Frame duration (milliseconds) */
    uint32_t buffer_size_ms;            /**< Buffer duration (milliseconds) */
    const char *device_id;              /**< Device ID (NULL=default) */
} voice_device_config_t;

/* ============================================
 * Codec Configuration
 * ============================================ */

typedef struct {
    voice_codec_type_t type;            /**< Codec type */
    uint32_t sample_rate;               /**< Sample rate */
    uint8_t channels;                   /**< Number of channels */
    uint32_t bitrate;                   /**< Bit rate (bps) */
    uint32_t frame_size_ms;             /**< Frame duration (milliseconds) */
    uint8_t complexity;                 /**< Complexity (0-10) */
    bool enable_dtx;                    /**< Enable discontinuous transmission */
    bool enable_fec;                    /**< Enable forward error correction */
    bool enable_plc;                    /**< Enable packet loss concealment */
} voice_codec_config_t;

/* ============================================
 * Denoising Configuration
 * ============================================ */

typedef struct {
    voice_denoise_engine_t engine;      /**< Denoising engine */
    int noise_suppress_db;              /**< Noise suppression amount (negative dB) */
    bool enable_agc;                    /**< Enable automatic gain */
    bool enable_vad;                    /**< Enable voice detection */
    
    /* Adaptive switching parameters */
    float cpu_threshold_high;           /**< CPU threshold (switch to SpeexDSP) */
    float cpu_threshold_low;            /**< CPU threshold (switch to RNNoise) */
    int battery_threshold;              /**< Battery threshold */
} voice_denoise_config_t;

/* ============================================
 * Echo Cancellation Configuration
 * ============================================ */

typedef struct {
    bool enabled;                       /**< Enable or not */
    uint32_t frame_size;                /**< Frame size (samples) */
    uint32_t filter_length;             /**< Filter length (samples) */
    int echo_suppress_db;               /**< Echo suppression amount (negative dB) */
    int echo_suppress_active_db;        /**< Suppression when near-end active */
} voice_aec_config_t;

/* ============================================
 * Resampler Configuration
 * ============================================ */

typedef struct {
    uint32_t input_rate;                /**< Input sample rate */
    uint32_t output_rate;               /**< Output sample rate */
    uint8_t channels;                   /**< Number of channels */
    int quality;                        /**< Quality (0-10) */
} voice_resampler_config_t;

/* ============================================
 * Network Transport Configuration
 * ============================================ */

typedef struct {
    const char *local_ip;               /**< Local IP */
    uint16_t local_port;                /**< Local port */
    const char *remote_ip;              /**< Remote IP */
    uint16_t remote_port;               /**< Remote port */
    bool enable_rtcp;                   /**< Enable RTCP */
    uint32_t rtcp_interval_ms;          /**< RTCP send interval */
} voice_network_config_t;

/* ============================================
 * Jitter Buffer Configuration
 * ============================================ */

typedef struct {
    uint32_t min_delay_ms;              /**< Minimum delay */
    uint32_t max_delay_ms;              /**< Maximum delay */
    uint32_t target_delay_ms;           /**< Target delay */
    bool adaptive;                      /**< Adaptive mode */
    uint32_t max_packets;               /**< Maximum buffered packets */
} voice_jitter_buffer_config_t;

/* ============================================
 * SRTP Encryption Configuration
 * ============================================ */

typedef struct {
    bool enabled;                       /**< Enable or not */
    voice_crypto_suite_t suite;         /**< Crypto suite */
    voice_key_exchange_t key_exchange;  /**< Key exchange method */
    
    /* Pre-shared key mode */
    const uint8_t *master_key;          /**< Master key */
    size_t master_key_len;              /**< Key length */
    const uint8_t *master_salt;         /**< Master salt */
    size_t master_salt_len;             /**< Salt length */
    
    /* DTLS mode */
    const char *cert_file;              /**< Certificate file */
    const char *key_file;               /**< Private key file */
    const char *ca_file;                /**< CA certificate file */
    bool verify_peer;                   /**< Verify peer */
} voice_srtp_config_t;

/* ============================================
 * File I/O Configuration
 * ============================================ */

typedef struct {
    const char *path;                   /**< File path */
    voice_format_t format;              /**< Output format */
    uint32_t sample_rate;               /**< Output sample rate */
    uint8_t channels;                   /**< Output channels */
} voice_file_config_t;

/* ============================================
 * Audio Processing Pipeline Configuration
 * ============================================ */

typedef struct {
    /* Device configuration */
    voice_device_config_t capture;      /**< Capture device configuration */
    voice_device_config_t playback;     /**< Playback device configuration */
    
    /* DSP configuration */
    voice_denoise_config_t denoise;     /**< Denoising configuration */
    voice_aec_config_t aec;             /**< Echo cancellation configuration */
    voice_resampler_config_t resampler; /**< Resampler configuration */
    
    /* Codec configuration */
    voice_codec_config_t codec;         /**< Codec configuration */
    
    /* Network configuration */
    voice_network_config_t network;     /**< Network configuration */
    voice_jitter_buffer_config_t jitter_buffer; /**< Jitter buffer configuration */
    voice_srtp_config_t srtp;           /**< SRTP configuration */
    
    /* Callbacks */
    voice_audio_callback_t on_capture;  /**< Capture callback */
    voice_audio_callback_t on_playback; /**< Playback callback */
    voice_event_callback_t on_event;    /**< Event callback */
    void *user_data;                    /**< User data */
} voice_pipeline_config_t;

/* ============================================
 * Default Configuration Initialization
 * ============================================ */

/**
 * @brief Initialize default global configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_global(voice_global_config_t *config);

/**
 * @brief Initialize default device configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_device(voice_device_config_t *config);

/**
 * @brief Initialize default codec configuration
 * @param config Configuration structure pointer
 * @param type Codec type
 */
void voice_config_init_codec(voice_codec_config_t *config, voice_codec_type_t type);

/**
 * @brief Initialize default denoising configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_denoise(voice_denoise_config_t *config);

/**
 * @brief Initialize default echo cancellation configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_aec(voice_aec_config_t *config);

/**
 * @brief Initialize default jitter buffer configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_jitter_buffer(voice_jitter_buffer_config_t *config);

/**
 * @brief Initialize default SRTP configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_srtp(voice_srtp_config_t *config);

/**
 * @brief Initialize default pipeline configuration
 * @param config Configuration structure pointer
 */
void voice_config_init_pipeline(voice_pipeline_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_CONFIG_H */
