/**
 * @file pipeline.h
 * @brief Audio processing pipeline interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Complete audio processing pipeline:
 * Capture -> Resampling -> AEC -> Denoising -> AGC -> Encoding -> RTP -> Network
 * Network -> RTP -> Jitter Buffer -> Decoding -> Resampling -> Playback
 */

#ifndef VOICE_PIPELINE_H
#define VOICE_PIPELINE_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/config.h"
#include "voice/export.h"
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
 * Pipeline Types
 * ============================================ */

typedef struct voice_pipeline_s voice_pipeline_t;

/** Pipeline mode */
typedef enum {
    PIPELINE_MODE_CAPTURE,      /**< Capture only */
    PIPELINE_MODE_PLAYBACK,     /**< Playback only */
    PIPELINE_MODE_DUPLEX,       /**< Full duplex */
    PIPELINE_MODE_LOOPBACK,     /**< Loopback test */
} pipeline_mode_t;

/** Pipeline state */
typedef enum {
    PIPELINE_STATE_STOPPED,
    PIPELINE_STATE_STARTING,
    PIPELINE_STATE_RUNNING,
    PIPELINE_STATE_STOPPING,
    PIPELINE_STATE_ERROR,
} pipeline_state_t;

/* ============================================
 * Pipeline Configuration (Extended)
 * ============================================ */

typedef struct {
    pipeline_mode_t mode;

    /* Audio device configuration */
    uint32_t sample_rate;           /**< Internal sample rate */
    uint8_t channels;               /**< Number of channels */
    uint32_t frame_duration_ms;     /**< Frame duration (ms) */
    const char *capture_device;     /**< Capture device ID (NULL=default) */
    const char *playback_device;    /**< Playback device ID (NULL=default) */

    /* DSP configuration */
    bool enable_aec;                /**< Enable AEC */
    bool enable_denoise;            /**< Enable denoising */
    bool enable_agc;                /**< Enable AGC */
    voice_denoise_engine_t denoise_engine; /**< Denoising engine */
    int denoise_level;              /**< Denoising level (0-100) */

    /* Codec configuration */
    voice_codec_id_t codec;         /**< Codec */
    uint32_t bitrate;               /**< Bit rate */
    bool enable_fec;                /**< Enable FEC */

    /* Network configuration */
    bool enable_srtp;               /**< Enable SRTP */
    srtp_profile_t srtp_profile;    /**< SRTP profile */

    /* Jitter Buffer configuration */
    uint32_t jitter_min_delay_ms;
    uint32_t jitter_max_delay_ms;
} voice_pipeline_ext_config_t;
/* ============================================
 * Callback Functions
 * ============================================ */

/** Encoded data callback (for transmission) */
typedef void (*pipeline_encoded_callback_t)(
    voice_pipeline_t *pipeline,
    const uint8_t *data,
    size_t size,
    uint32_t timestamp,
    void *user_data
);

/** Decoded data callback */
typedef void (*pipeline_decoded_callback_t)(
    voice_pipeline_t *pipeline,
    const int16_t *pcm,
    size_t samples,
    void *user_data
);

/** State change callback */
typedef void (*pipeline_state_callback_t)(
    voice_pipeline_t *pipeline,
    pipeline_state_t state,
    void *user_data
);

/** Error callback */
typedef void (*pipeline_error_callback_t)(
    voice_pipeline_t *pipeline,
    voice_error_t error,
    const char *message,
    void *user_data
);

/* ============================================
 * Pipeline Statistics
 * ============================================ */

typedef struct {
    /* Capture statistics */
    uint64_t frames_captured;
    uint64_t frames_dropped_capture;

    /* Playback statistics */
    uint64_t frames_played;
    uint64_t frames_dropped_playback;

    /* Codec statistics */
    uint64_t frames_encoded;
    uint64_t frames_decoded;

    /* Network statistics */
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_lost;
    float packet_loss_rate;
    uint32_t jitter_ms;
    uint32_t rtt_ms;

    /* Audio quality */
    float capture_level_db;
    float playback_level_db;
    bool vad_active;
} voice_pipeline_stats_t;

/* ============================================
 * Pipeline API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_pipeline_ext_config_init(voice_pipeline_ext_config_t *config);

/**
 * @brief Create pipeline
 */
VOICE_API voice_pipeline_t *voice_pipeline_create(const voice_pipeline_ext_config_t *config);

/**
 * @brief Destroy pipeline
 */
VOICE_API void voice_pipeline_destroy(voice_pipeline_t *pipeline);

/**
 * @brief Start pipeline
 */
VOICE_API voice_error_t voice_pipeline_start(voice_pipeline_t *pipeline);

/**
 * @brief Stop pipeline
 */
VOICE_API voice_error_t voice_pipeline_stop(voice_pipeline_t *pipeline);

/**
 * @brief Get state
 */
VOICE_API pipeline_state_t voice_pipeline_get_state(voice_pipeline_t *pipeline);

/**
 * @brief Set encoded data callback
 */
VOICE_API void voice_pipeline_set_encoded_callback(
    voice_pipeline_t *pipeline,
    pipeline_encoded_callback_t callback,
    void *user_data
);

/**
 * @brief Set decoded data callback
 */
VOICE_API void voice_pipeline_set_decoded_callback(
    voice_pipeline_t *pipeline,
    pipeline_decoded_callback_t callback,
    void *user_data
);

/**
 * @brief Set state callback
 */
VOICE_API void voice_pipeline_set_state_callback(
    voice_pipeline_t *pipeline,
    pipeline_state_callback_t callback,
    void *user_data
);

/**
 * @brief Set error callback
 */
VOICE_API void voice_pipeline_set_error_callback(
    voice_pipeline_t *pipeline,
    pipeline_error_callback_t callback,
    void *user_data
);

/**
 * @brief Input received network data
 */
VOICE_API voice_error_t voice_pipeline_receive_packet(
    voice_pipeline_t *pipeline,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Input local PCM data (when not using device capture)
 */
VOICE_API voice_error_t voice_pipeline_push_capture(
    voice_pipeline_t *pipeline,
    const int16_t *pcm,
    size_t samples
);

/**
 * @brief Get playback PCM data (when not using device playback)
 */
VOICE_API voice_error_t voice_pipeline_pull_playback(
    voice_pipeline_t *pipeline,
    int16_t *pcm,
    size_t samples,
    size_t *samples_out
);

/**
 * @brief Get statistics
 */
VOICE_API voice_error_t voice_pipeline_get_stats(
    voice_pipeline_t *pipeline,
    voice_pipeline_stats_t *stats
);

/**
 * @brief Reset statistics
 */
VOICE_API void voice_pipeline_reset_stats(voice_pipeline_t *pipeline);

/* ============================================
 * Pipeline Control
 * ============================================ */

/**
 * @brief Enable/disable AEC
 */
VOICE_API voice_error_t voice_pipeline_set_aec_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief Enable/disable denoising
 */
VOICE_API voice_error_t voice_pipeline_set_denoise_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief Set denoising level
 */
VOICE_API voice_error_t voice_pipeline_set_denoise_level(
    voice_pipeline_t *pipeline,
    int level
);

/**
 * @brief Enable/disable AGC
 */
VOICE_API voice_error_t voice_pipeline_set_agc_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief Set encoding bit rate
 */
VOICE_API voice_error_t voice_pipeline_set_bitrate(
    voice_pipeline_t *pipeline,
    uint32_t bitrate
);

/**
 * @brief Mute capture
 */
VOICE_API voice_error_t voice_pipeline_set_capture_muted(
    voice_pipeline_t *pipeline,
    bool muted
);

/**
 * @brief Mute playback
 */
VOICE_API voice_error_t voice_pipeline_set_playback_muted(
    voice_pipeline_t *pipeline,
    bool muted
);

/**
 * @brief Set playback volume
 */
VOICE_API voice_error_t voice_pipeline_set_playback_volume(
    voice_pipeline_t *pipeline,
    float volume  /**< 0.0 - 1.0 */
);

/* ============================================
 * SRTP Key Setup
 * ============================================ */

/**
 * @brief Set SRTP send key
 */
VOICE_API voice_error_t voice_pipeline_set_srtp_send_key(
    voice_pipeline_t *pipeline,
    const uint8_t *key,
    size_t key_len,
    const uint8_t *salt,
    size_t salt_len
);

/**
 * @brief Set SRTP receive key
 */
VOICE_API voice_error_t voice_pipeline_set_srtp_recv_key(
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
