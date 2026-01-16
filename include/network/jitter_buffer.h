/**
 * @file jitter_buffer.h
 * @brief Jitter Buffer and Packet Loss Concealment interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1 Enhancement:
 * - Jitter histogram and statistical delay estimation
 * - WSOLA time stretcher integration for adaptive playout
 * - Improved PLC integration
 */

#ifndef NETWORK_JITTER_BUFFER_H
#define NETWORK_JITTER_BUFFER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include "network/rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Jitter Buffer Constants
 * ============================================ */

#define JITTER_BUFFER_DEFAULT_CAPACITY  100   /**< Default capacity (packets) */
#define JITTER_BUFFER_DEFAULT_MIN_DELAY 20    /**< Default minimum delay (ms) */
#define JITTER_BUFFER_DEFAULT_MAX_DELAY 200   /**< Default maximum delay (ms) */
#define JITTER_HISTOGRAM_BINS           64    /**< Jitter histogram bins */
#define JITTER_HISTORY_SIZE             128   /**< Delay history size */

/* ============================================
 * Jitter Buffer Types
 * ============================================ */

typedef struct jitter_buffer_s jitter_buffer_t;

/** Jitter buffer mode */
typedef enum {
    JITTER_MODE_FIXED,      /**< Fixed delay */
    JITTER_MODE_ADAPTIVE,   /**< Adaptive delay */
} jitter_mode_t;

/** Jitter buffer configuration */
typedef struct {
    uint32_t clock_rate;        /**< Clock rate */
    uint32_t frame_duration_ms; /**< Frame duration (ms) */
    jitter_mode_t mode;         /**< Buffer mode */
    uint32_t min_delay_ms;      /**< Minimum delay (ms) */
    uint32_t max_delay_ms;      /**< Maximum delay (ms) */
    uint32_t initial_delay_ms;  /**< Initial delay (ms) */
    uint32_t capacity;          /**< Capacity (packets) */
    bool enable_plc;            /**< Enable PLC */
    bool enable_time_stretch;   /**< Enable time stretching (catch-up/slow-down) */
    float target_buffer_level;  /**< Target buffer level (frames, default 2.0) */
    float jitter_percentile;    /**< Jitter percentile (0.9-0.99, default 0.95) */
} jitter_buffer_config_t;

/** Packet status */
typedef enum {
    JITTER_PACKET_OK,           /**< Normal */
    JITTER_PACKET_LOST,         /**< Lost */
    JITTER_PACKET_LATE,         /**< Too late */
    JITTER_PACKET_EARLY,        /**< Too early */
    JITTER_PACKET_DUPLICATE,    /**< Duplicate */
    JITTER_PACKET_INTERPOLATED, /**< Interpolated */
} jitter_packet_status_t;

/** Jitter buffer statistics */
typedef struct {
    uint64_t packets_received;
    uint64_t packets_output;
    uint64_t packets_lost;
    uint64_t packets_late;
    uint64_t packets_early;
    uint64_t packets_duplicate;
    uint64_t packets_interpolated;
    uint32_t current_delay_ms;
    uint32_t min_delay_observed_ms;
    uint32_t max_delay_observed_ms;
    float loss_rate;

    /* New: Jitter statistics */
    float    jitter_ms;             /**< Average jitter (ms) */
    float    jitter_max_ms;         /**< Maximum jitter (ms) */
    float    jitter_percentile_ms;  /**< Percentile jitter (ms) */
    uint32_t target_delay_ms;       /**< Calculated target delay */

    /* New: Time stretch statistics */
    uint64_t accelerate_count;      /**< Accelerated playback count */
    uint64_t decelerate_count;      /**< Decelerated playback count */
    float    current_stretch_rate;  /**< Current stretch rate */

    /* New: Buffer health */
    float    buffer_level;          /**< Current buffer level (frames) */
    float    buffer_health;         /**< Buffer health (0-1) */
} jitter_buffer_stats_t;

/* ============================================
 * Jitter Buffer API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void jitter_buffer_config_init(jitter_buffer_config_t *config);

/**
 * @brief Create jitter buffer
 */
VOICE_API jitter_buffer_t *jitter_buffer_create(const jitter_buffer_config_t *config);

/**
 * @brief Destroy jitter buffer
 */
VOICE_API void jitter_buffer_destroy(jitter_buffer_t *jb);

/**
 * @brief Add packet to buffer
 *
 * @param jb Jitter buffer handle
 * @param data Packet data
 * @param size Packet size
 * @param timestamp RTP timestamp
 * @param sequence Sequence number
 * @param marker Marker bit
 * @return Error code
 */
VOICE_API voice_error_t jitter_buffer_put(
    jitter_buffer_t *jb,
    const uint8_t *data,
    size_t size,
    uint32_t timestamp,
    uint16_t sequence,
    bool marker
);

/**
 * @brief Get packet from buffer
 *
 * This function should be called periodically, with intervals equal to frame_duration_ms
 *
 * @param jb Jitter buffer handle
 * @param output Output buffer
 * @param max_size Buffer size
 * @param out_size Actual output size
 * @param status Packet status
 * @return Error code
 */
VOICE_API voice_error_t jitter_buffer_get(
    jitter_buffer_t *jb,
    uint8_t *output,
    size_t max_size,
    size_t *out_size,
    jitter_packet_status_t *status
);

/**
 * @brief Set target delay
 */
VOICE_API voice_error_t jitter_buffer_set_delay(
    jitter_buffer_t *jb,
    uint32_t delay_ms
);

/**
 * @brief Get current delay
 */
VOICE_API uint32_t jitter_buffer_get_delay(jitter_buffer_t *jb);

/**
 * @brief Get statistics
 */
VOICE_API voice_error_t jitter_buffer_get_stats(
    jitter_buffer_t *jb,
    jitter_buffer_stats_t *stats
);

/**
 * @brief Reset statistics
 */
VOICE_API void jitter_buffer_reset_stats(jitter_buffer_t *jb);

/**
 * @brief Reset buffer
 */
VOICE_API void jitter_buffer_reset(jitter_buffer_t *jb);

/**
 * @brief Get packet count in buffer
 */
VOICE_API size_t jitter_buffer_get_count(jitter_buffer_t *jb);

/**
 * @brief Get jitter histogram
 *
 * @param jb Jitter buffer handle
 * @param histogram Output histogram array (JITTER_HISTOGRAM_BINS bins)
 * @param bin_width_ms Width of each bin (ms)
 * @return Error code
 */
VOICE_API voice_error_t jitter_buffer_get_histogram(
    jitter_buffer_t *jb,
    uint32_t *histogram,
    float *bin_width_ms
);

/**
 * @brief Enable/disable time stretching
 *
 * @param jb Jitter buffer handle
 * @param enable Whether to enable
 * @return Error code
 */
VOICE_API voice_error_t jitter_buffer_enable_time_stretch(
    jitter_buffer_t *jb,
    bool enable
);

/**
 * @brief Get current recommended playout rate
 *
 * For external time stretcher control
 *
 * @param jb Jitter buffer handle
 * @return Playout rate (1.0=normal, >1.0=accelerate, <1.0=decelerate)
 */
VOICE_API float jitter_buffer_get_playout_rate(jitter_buffer_t *jb);

/* ============================================
 * PLC (Packet Loss Concealment) API
 * ============================================ */

typedef struct voice_plc_s voice_plc_t;

/** PLC algorithm type */
typedef enum {
    PLC_ALG_ZERO,           /**< Silence fill */
    PLC_ALG_REPEAT,         /**< Repeat last frame */
    PLC_ALG_FADE,           /**< Fade out */
    PLC_ALG_INTERPOLATE,    /**< Waveform interpolation */
} plc_algorithm_t;

/** PLC configuration */
typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    plc_algorithm_t algorithm;
    uint32_t max_consecutive_loss;  /**< Maximum consecutive loss count */
} voice_plc_config_t;

/**
 * @brief Initialize PLC default configuration
 */
VOICE_API void voice_plc_config_init(voice_plc_config_t *config);

/**
 * @brief Create PLC processor
 */
VOICE_API voice_plc_t *voice_plc_create(const voice_plc_config_t *config);

/**
 * @brief Destroy PLC processor
 */
VOICE_API void voice_plc_destroy(voice_plc_t *plc);

/**
 * @brief Update good frame (for PLC reference)
 */
VOICE_API void voice_plc_update_good_frame(
    voice_plc_t *plc,
    const int16_t *pcm,
    size_t samples
);

/**
 * @brief Generate loss concealment frame
 */
VOICE_API voice_error_t voice_plc_generate(
    voice_plc_t *plc,
    int16_t *output,
    size_t samples
);

/**
 * @brief Reset PLC state
 */
VOICE_API void voice_plc_reset(voice_plc_t *plc);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_JITTER_BUFFER_H */
