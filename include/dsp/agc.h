/**
 * @file agc.h
 * @brief Automatic Gain Control (AGC)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Automatic gain control module that adjusts audio levels to target range.
 * Supports multiple AGC modes and adaptive algorithms.
 */

#ifndef VOICE_AGC_H
#define VOICE_AGC_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Type Definitions
 * ============================================ */

typedef struct voice_agc_s voice_agc_t;

/** AGC mode */
typedef enum {
    VOICE_AGC_FIXED,            /**< Fixed gain */
    VOICE_AGC_ADAPTIVE,         /**< Adaptive gain */
    VOICE_AGC_ADAPTIVE_DIGITAL, /**< Digital adaptive (WebRTC style) */
    VOICE_AGC_LIMITER,          /**< Limiter-only mode */
} voice_agc_mode_t;

/** AGC compression ratio */
typedef enum {
    VOICE_AGC_COMPRESSION_NONE = 0,     /**< No compression */
    VOICE_AGC_COMPRESSION_LOW = 1,      /**< Low compression (2:1) */
    VOICE_AGC_COMPRESSION_MEDIUM = 2,   /**< Medium compression (4:1) */
    VOICE_AGC_COMPRESSION_HIGH = 3,     /**< High compression (8:1) */
} voice_agc_compression_t;

/* ============================================
 * Configuration
 * ============================================ */

typedef struct {
    voice_agc_mode_t mode;
    uint32_t sample_rate;
    uint32_t frame_size;            /**< Frame size (samples) */

    /* Target level */
    float target_level_dbfs;        /**< Target output level (dBFS), typically -3 to -18 */

    /* Gain limits */
    float min_gain_db;              /**< Minimum gain (dB) */
    float max_gain_db;              /**< Maximum gain (dB) */

    /* Dynamic parameters */
    float attack_time_ms;           /**< Attack time (ms) */
    float release_time_ms;          /**< Release time (ms) */
    float hold_time_ms;             /**< Hold time (ms) */

    /* Compression */
    voice_agc_compression_t compression;
    float compression_threshold_db; /**< Compression threshold (dBFS) */

    /* Noise gate */
    bool enable_noise_gate;
    float noise_gate_threshold_db;  /**< Noise gate threshold (dBFS) */

    /* Limiter */
    bool enable_limiter;
    float limiter_threshold_db;     /**< Limiter threshold (dBFS), typically -1 */
} voice_agc_config_t;

/* ============================================
 * State Information
 * ============================================ */

typedef struct {
    float current_gain_db;          /**< Current gain (dB) */
    float input_level_db;           /**< Input level (dBFS) */
    float output_level_db;          /**< Output level (dBFS) */
    float compression_ratio;        /**< Current compression ratio */
    bool gate_active;               /**< Whether noise gate is active */
    bool limiter_active;            /**< Whether limiter is active */
    uint32_t saturation_count;      /**< Saturation/clipping count */
} voice_agc_state_t;

/* ============================================
 * AGC API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_agc_config_init(voice_agc_config_t *config);

/**
 * @brief Create AGC instance
 */
VOICE_API voice_agc_t *voice_agc_create(const voice_agc_config_t *config);

/**
 * @brief Destroy AGC instance
 */
VOICE_API void voice_agc_destroy(voice_agc_t *agc);

/**
 * @brief Process audio frame (int16)
 * @param agc AGC instance
 * @param samples Audio samples (modified in-place)
 * @param num_samples Number of samples
 * @return Error code
 */
VOICE_API voice_error_t voice_agc_process(
    voice_agc_t *agc,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief Process audio frame (float)
 */
VOICE_API voice_error_t voice_agc_process_float(
    voice_agc_t *agc,
    float *samples,
    size_t num_samples
);

/**
 * @brief Set target level
 */
VOICE_API voice_error_t voice_agc_set_target_level(voice_agc_t *agc, float level_dbfs);

/**
 * @brief Set gain range
 */
VOICE_API voice_error_t voice_agc_set_gain_range(
    voice_agc_t *agc,
    float min_gain_db,
    float max_gain_db
);

/**
 * @brief Set mode
 */
VOICE_API voice_error_t voice_agc_set_mode(voice_agc_t *agc, voice_agc_mode_t mode);

/**
 * @brief Get current state
 */
VOICE_API voice_error_t voice_agc_get_state(voice_agc_t *agc, voice_agc_state_t *state);

/**
 * @brief Reset AGC state
 */
VOICE_API void voice_agc_reset(voice_agc_t *agc);

/**
 * @brief Analyze audio level (does not modify samples)
 */
VOICE_API float voice_agc_analyze_level(
    voice_agc_t *agc,
    const int16_t *samples,
    size_t num_samples
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AGC_H */
