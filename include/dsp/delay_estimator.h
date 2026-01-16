/**
 * @file delay_estimator.h
 * @brief Audio Delay Estimator for AEC Alignment
 *
 * Implements GCC-PHAT (Generalized Cross-Correlation with Phase Transform)
 * for estimating the delay between playback and capture signals.
 *
 * Critical for proper AEC operation - ensures the reference signal
 * is aligned with the captured echo.
 */

#ifndef VOICE_DELAY_ESTIMATOR_H
#define VOICE_DELAY_ESTIMATOR_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Delay estimator configuration
 */
typedef struct {
    uint32_t sample_rate;           /**< Sample rate in Hz */
    uint32_t frame_size;            /**< Analysis frame size in samples */
    uint32_t max_delay_samples;     /**< Maximum delay to search (samples) */
    uint32_t min_delay_samples;     /**< Minimum delay to search (samples) */
    uint32_t history_size;          /**< Number of estimates to average */
    float    confidence_threshold;  /**< Min correlation for valid estimate (0-1) */
    bool     use_phat;              /**< Use PHAT weighting (recommended) */
} voice_delay_estimator_config_t;

/**
 * @brief Delay estimation result
 */
typedef struct {
    int32_t  delay_samples;         /**< Estimated delay in samples */
    float    delay_ms;              /**< Estimated delay in milliseconds */
    float    confidence;            /**< Estimation confidence (0-1) */
    float    correlation_peak;      /**< Peak correlation value */
    bool     valid;                 /**< Whether estimate is reliable */
} voice_delay_estimate_t;

/**
 * @brief Delay estimator state
 */
typedef struct {
    int32_t  current_delay;         /**< Current delay estimate (samples) */
    float    average_delay_ms;      /**< Smoothed delay estimate (ms) */
    float    delay_variance;        /**< Variance in delay estimates */
    uint32_t total_estimates;       /**< Number of estimates computed */
    uint32_t valid_estimates;       /**< Number of valid (high-confidence) estimates */
} voice_delay_estimator_state_t;

/**
 * @brief Opaque delay estimator handle
 */
typedef struct voice_delay_estimator voice_delay_estimator_t;

/**
 * @brief Initialize configuration with default values
 *
 * @param config Configuration to initialize
 */
VOICE_API void voice_delay_estimator_config_init(voice_delay_estimator_config_t *config);

/**
 * @brief Create a delay estimator instance
 *
 * @param config Configuration parameters
 * @return Delay estimator handle, or NULL on failure
 */
VOICE_API voice_delay_estimator_t *voice_delay_estimator_create(
    const voice_delay_estimator_config_t *config
);

/**
 * @brief Destroy a delay estimator instance
 *
 * @param de Delay estimator handle
 */
VOICE_API void voice_delay_estimator_destroy(voice_delay_estimator_t *de);

/**
 * @brief Reset the delay estimator state
 *
 * @param de Delay estimator handle
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_delay_estimator_reset(voice_delay_estimator_t *de);

/**
 * @brief Estimate delay between reference and capture signals
 *
 * Uses GCC-PHAT algorithm to find the time offset that maximizes
 * cross-correlation between the two signals.
 *
 * @param de Delay estimator handle
 * @param reference Reference signal (playback/far-end)
 * @param capture Capture signal (microphone/near-end)
 * @param sample_count Number of samples in each signal
 * @param result Estimation result output
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_delay_estimator_estimate(
    voice_delay_estimator_t *de,
    const int16_t *reference,
    const int16_t *capture,
    size_t sample_count,
    voice_delay_estimate_t *result
);

/**
 * @brief Estimate delay (float version)
 *
 * @param de Delay estimator handle
 * @param reference Reference signal (float)
 * @param capture Capture signal (float)
 * @param sample_count Number of samples
 * @param result Estimation result
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_delay_estimator_estimate_float(
    voice_delay_estimator_t *de,
    const float *reference,
    const float *capture,
    size_t sample_count,
    voice_delay_estimate_t *result
);

/**
 * @brief Get the current smoothed delay estimate
 *
 * Returns the averaged delay based on history.
 * More stable than individual estimates.
 *
 * @param de Delay estimator handle
 * @return Current delay in samples, or 0 if not yet estimated
 */
VOICE_API int32_t voice_delay_estimator_get_delay(voice_delay_estimator_t *de);

/**
 * @brief Get delay in milliseconds
 *
 * @param de Delay estimator handle
 * @return Current delay in milliseconds
 */
VOICE_API float voice_delay_estimator_get_delay_ms(voice_delay_estimator_t *de);

/**
 * @brief Set a known/fixed delay
 *
 * Useful when delay is known from external source (e.g., audio driver).
 *
 * @param de Delay estimator handle
 * @param delay_samples Known delay in samples
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_delay_estimator_set_delay(
    voice_delay_estimator_t *de,
    int32_t delay_samples
);

/**
 * @brief Get current state
 *
 * @param de Delay estimator handle
 * @param state State output
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_delay_estimator_get_state(
    voice_delay_estimator_t *de,
    voice_delay_estimator_state_t *state
);

/**
 * @brief Check if delay estimate is stable
 *
 * Returns true if the delay estimate has low variance
 * over the recent history.
 *
 * @param de Delay estimator handle
 * @return true if stable, false otherwise
 */
VOICE_API bool voice_delay_estimator_is_stable(voice_delay_estimator_t *de);

/**
 * @brief Get cross-correlation values for debugging
 *
 * @param de Delay estimator handle
 * @param correlation Output array for correlation values
 * @param max_lags Maximum number of lag values to return
 * @param actual_lags Actual number of lags written
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_delay_estimator_get_correlation(
    voice_delay_estimator_t *de,
    float *correlation,
    size_t max_lags,
    size_t *actual_lags
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_DELAY_ESTIMATOR_H */
