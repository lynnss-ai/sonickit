/**
 * @file time_stretcher.h
 * @brief WSOLA (Waveform Similarity Overlap-Add) Time Stretcher
 *
 * Provides time-scale modification without pitch change.
 * Used by Jitter Buffer for adaptive playout speed control.
 *
 * Supported rates: 0.5x to 2.0x
 */

#ifndef VOICE_TIME_STRETCHER_H
#define VOICE_TIME_STRETCHER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Time stretcher configuration
 */
typedef struct {
    uint32_t sample_rate;           /**< Sample rate in Hz */
    uint32_t channels;              /**< Number of channels (1 or 2) */
    uint32_t frame_size_ms;         /**< Analysis frame size in ms (default: 20) */
    uint32_t overlap_ms;            /**< Overlap size in ms (default: 10) */
    uint32_t search_range_ms;       /**< WSOLA search range in ms (default: 15) */
    float    initial_rate;          /**< Initial time stretch rate (default: 1.0) */
} voice_time_stretcher_config_t;

/**
 * @brief Time stretcher state
 */
typedef struct {
    float    current_rate;          /**< Current stretch rate */
    uint32_t input_samples;         /**< Total input samples processed */
    uint32_t output_samples;        /**< Total output samples generated */
    float    latency_ms;            /**< Current processing latency */
} voice_time_stretcher_state_t;

/**
 * @brief Opaque time stretcher handle
 */
typedef struct voice_time_stretcher voice_time_stretcher_t;

/**
 * @brief Initialize configuration with default values
 *
 * @param config Configuration to initialize
 */
VOICE_API void voice_time_stretcher_config_init(voice_time_stretcher_config_t *config);

/**
 * @brief Create a time stretcher instance
 *
 * @param config Configuration parameters
 * @return Time stretcher handle, or NULL on failure
 */
VOICE_API voice_time_stretcher_t *voice_time_stretcher_create(const voice_time_stretcher_config_t *config);

/**
 * @brief Destroy a time stretcher instance
 *
 * @param ts Time stretcher handle
 */
VOICE_API void voice_time_stretcher_destroy(voice_time_stretcher_t *ts);

/**
 * @brief Reset the time stretcher state
 *
 * @param ts Time stretcher handle
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_time_stretcher_reset(voice_time_stretcher_t *ts);

/**
 * @brief Set the time stretch rate
 *
 * Rate < 1.0: Slow down (stretch)
 * Rate = 1.0: No change
 * Rate > 1.0: Speed up (compress)
 *
 * @param ts Time stretcher handle
 * @param rate Stretch rate (0.5 to 2.0)
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_time_stretcher_set_rate(voice_time_stretcher_t *ts, float rate);

/**
 * @brief Get the current stretch rate
 *
 * @param ts Time stretcher handle
 * @return Current rate
 */
VOICE_API float voice_time_stretcher_get_rate(voice_time_stretcher_t *ts);

/**
 * @brief Process audio samples
 *
 * Performs time-scale modification on input samples.
 * Output size will vary based on the stretch rate.
 *
 * @param ts Time stretcher handle
 * @param input Input samples (int16)
 * @param input_count Number of input samples
 * @param output Output buffer (must be large enough)
 * @param output_capacity Output buffer capacity in samples
 * @param output_count Actual number of output samples written
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_time_stretcher_process(
    voice_time_stretcher_t *ts,
    const int16_t *input,
    size_t input_count,
    int16_t *output,
    size_t output_capacity,
    size_t *output_count
);

/**
 * @brief Process audio samples (float version)
 *
 * @param ts Time stretcher handle
 * @param input Input samples (float, -1.0 to 1.0)
 * @param input_count Number of input samples
 * @param output Output buffer
 * @param output_capacity Output buffer capacity
 * @param output_count Actual output count
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_time_stretcher_process_float(
    voice_time_stretcher_t *ts,
    const float *input,
    size_t input_count,
    float *output,
    size_t output_capacity,
    size_t *output_count
);

/**
 * @brief Get the number of samples buffered internally
 *
 * @param ts Time stretcher handle
 * @return Number of buffered samples
 */
VOICE_API size_t voice_time_stretcher_get_buffered(voice_time_stretcher_t *ts);

/**
 * @brief Flush remaining samples from internal buffer
 *
 * @param ts Time stretcher handle
 * @param output Output buffer
 * @param output_capacity Output capacity
 * @param output_count Actual output count
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_time_stretcher_flush(
    voice_time_stretcher_t *ts,
    int16_t *output,
    size_t output_capacity,
    size_t *output_count
);

/**
 * @brief Get current state
 *
 * @param ts Time stretcher handle
 * @param state State output
 * @return VOICE_SUCCESS on success
 */
VOICE_API voice_error_t voice_time_stretcher_get_state(
    voice_time_stretcher_t *ts,
    voice_time_stretcher_state_t *state
);

/**
 * @brief Calculate required output buffer size
 *
 * @param input_count Number of input samples
 * @param rate Current stretch rate
 * @return Required output buffer size (with safety margin)
 */
VOICE_API size_t voice_time_stretcher_get_output_size(size_t input_count, float rate);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_TIME_STRETCHER_H */
