/**
 * @file bandwidth_estimator.h
 * @brief Network bandwidth estimation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Network bandwidth estimator for adaptive bitrate control
 */

#ifndef VOICE_BANDWIDTH_ESTIMATOR_H
#define VOICE_BANDWIDTH_ESTIMATOR_H

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

typedef struct voice_bwe_s voice_bwe_t;

/** Network quality */
typedef enum {
    VOICE_NETWORK_EXCELLENT,   /**< Excellent (< 1% loss, < 50ms latency) */
    VOICE_NETWORK_GOOD,        /**< Good (< 3% loss, < 100ms latency) */
    VOICE_NETWORK_FAIR,        /**< Fair (< 10% loss, < 200ms latency) */
    VOICE_NETWORK_POOR,        /**< Poor (< 20% loss, < 400ms latency) */
    VOICE_NETWORK_BAD,         /**< Bad (>= 20% loss or >= 400ms latency) */
} voice_network_quality_t;

/* ============================================
 * Configuration
 * ============================================ */

typedef struct {
    uint32_t initial_bitrate;       /**< Initial bitrate */
    uint32_t min_bitrate;           /**< Minimum bitrate */
    uint32_t max_bitrate;           /**< Maximum bitrate */

    /* Estimation window */
    uint32_t window_size_ms;        /**< Statistics window size */

    /* AIMD parameters */
    float increase_rate;            /**< Increase rate (additive increase) */
    float decrease_factor;          /**< Decrease factor (multiplicative decrease) */

    /* Thresholds */
    float loss_threshold_increase;  /**< Loss threshold to allow increase */
    float loss_threshold_decrease;  /**< Loss threshold to trigger decrease */
    uint32_t rtt_threshold_ms;      /**< RTT threshold */

    /* Hold period */
    uint32_t hold_time_ms;          /**< Hold period after adjustment */
} voice_bwe_config_t;

/* ============================================
 * Feedback Data
 * ============================================ */

typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t bytes_sent;
    uint32_t rtt_ms;
    uint32_t jitter_ms;
    uint32_t timestamp;             /**< Feedback timestamp */
} voice_bwe_feedback_t;

/* ============================================
 * Estimation Result
 * ============================================ */

typedef struct {
    uint32_t estimated_bitrate;     /**< Estimated available bandwidth */
    uint32_t target_bitrate;        /**< Recommended target bitrate */
    float packet_loss_rate;         /**< Packet loss rate */
    uint32_t rtt_ms;                /**< Round-trip time */
    uint32_t jitter_ms;             /**< Jitter */
    voice_network_quality_t quality;/**< Network quality */
    bool should_adjust;             /**< Whether bitrate adjustment is needed */
} voice_bwe_estimate_t;

/* ============================================
 * BWE API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_bwe_config_init(voice_bwe_config_t *config);

/**
 * @brief Create bandwidth estimator
 */
VOICE_API voice_bwe_t *voice_bwe_create(const voice_bwe_config_t *config);

/**
 * @brief Destroy bandwidth estimator
 */
VOICE_API void voice_bwe_destroy(voice_bwe_t *bwe);

/**
 * @brief Update send statistics
 */
VOICE_API void voice_bwe_on_packet_sent(
    voice_bwe_t *bwe,
    uint16_t sequence,
    size_t size,
    uint32_t timestamp
);

/**
 * @brief Process received feedback (RTCP RR)
 */
VOICE_API voice_error_t voice_bwe_on_feedback(
    voice_bwe_t *bwe,
    const voice_bwe_feedback_t *feedback
);

/**
 * @brief Get bandwidth estimate
 */
VOICE_API voice_error_t voice_bwe_get_estimate(
    voice_bwe_t *bwe,
    voice_bwe_estimate_t *estimate
);

/**
 * @brief Get recommended bitrate
 */
VOICE_API uint32_t voice_bwe_get_target_bitrate(voice_bwe_t *bwe);

/**
 * @brief Get network quality
 */
VOICE_API voice_network_quality_t voice_bwe_get_network_quality(voice_bwe_t *bwe);

/**
 * @brief Reset estimator
 */
VOICE_API void voice_bwe_reset(voice_bwe_t *bwe);

/**
 * @brief Bandwidth change callback
 */
typedef void (*voice_bwe_callback_t)(
    voice_bwe_t *bwe,
    uint32_t old_bitrate,
    uint32_t new_bitrate,
    voice_network_quality_t quality,
    void *user_data
);

VOICE_API void voice_bwe_set_callback(
    voice_bwe_t *bwe,
    voice_bwe_callback_t callback,
    void *user_data
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_BANDWIDTH_ESTIMATOR_H */
