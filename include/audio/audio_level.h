/**
 * @file audio_level.h
 * @brief Audio level metering and analysis
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Audio level metering and analysis tools
 */

#ifndef VOICE_AUDIO_LEVEL_H
#define VOICE_AUDIO_LEVEL_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Level Meter Types
 * ============================================ */

typedef struct voice_level_meter_s voice_level_meter_t;

/** Measurement type */
typedef enum {
    VOICE_LEVEL_PEAK,           /**< Peak level */
    VOICE_LEVEL_RMS,            /**< RMS level */
    VOICE_LEVEL_LUFS,           /**< LUFS loudness (ITU-R BS.1770) */
} voice_level_type_t;

/* ============================================
 * Configuration
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t window_size_ms;    /**< Measurement window size */
    float attack_ms;            /**< Attack time (ms) */
    float release_ms;           /**< Release time (ms) */
    bool enable_true_peak;      /**< Enable true peak measurement (oversampling) */
} voice_level_meter_config_t;

/* ============================================
 * Measurement Results
 * ============================================ */

typedef struct {
    float peak_db;              /**< Peak level (dBFS) */
    float rms_db;               /**< RMS level (dBFS) */
    float peak_sample;          /**< Peak sample value (linear) */
    float rms_linear;           /**< RMS value (linear) */
    bool clipping;              /**< Clipping detected */
    uint32_t clip_count;        /**< Clipping sample count */
} voice_level_result_t;

/* ============================================
 * Level Meter API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_level_meter_config_init(voice_level_meter_config_t *config);

/**
 * @brief Create level meter
 */
VOICE_API voice_level_meter_t *voice_level_meter_create(const voice_level_meter_config_t *config);

/**
 * @brief Destroy level meter
 */
VOICE_API void voice_level_meter_destroy(voice_level_meter_t *meter);

/**
 * @brief Process audio samples
 */
VOICE_API voice_error_t voice_level_meter_process(
    voice_level_meter_t *meter,
    const int16_t *samples,
    size_t num_samples,
    voice_level_result_t *result
);

/**
 * @brief Process float audio samples
 */
VOICE_API voice_error_t voice_level_meter_process_float(
    voice_level_meter_t *meter,
    const float *samples,
    size_t num_samples,
    voice_level_result_t *result
);

/**
 * @brief Get current level
 */
VOICE_API float voice_level_meter_get_level_db(voice_level_meter_t *meter);

/**
 * @brief Reset level meter
 */
VOICE_API void voice_level_meter_reset(voice_level_meter_t *meter);

/* ============================================
 * Shortcut Functions (stateless)
 * ============================================ */

/**
 * @brief Calculate peak level of audio block (dB)
 */
VOICE_API float voice_audio_peak_db(const int16_t *samples, size_t num_samples);

/**
 * @brief Calculate RMS level of audio block (dB)
 */
VOICE_API float voice_audio_rms_db(const int16_t *samples, size_t num_samples);

/**
 * @brief Convert linear value to dB
 */
static inline float voice_linear_to_db(float linear) {
    if (linear <= 0.0f) return -96.0f;
    float db = 20.0f * log10f(linear);
    return db < -96.0f ? -96.0f : db;
}

/**
 * @brief Convert dB to linear value
 */
static inline float voice_db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

/* ============================================
 * RTP Audio Level Extension (RFC 6464)
 * ============================================ */

/**
 * @brief Calculate audio level for RTP extension header (RFC 6464)
 * @param samples Audio samples
 * @param num_samples Number of samples
 * @return Audio level (0-127, 0 = 0dBov, 127 = -127dBov)
 */
VOICE_API uint8_t voice_audio_level_rfc6464(const int16_t *samples, size_t num_samples);

/**
 * @brief Parse RFC 6464 audio level
 * @param level RFC 6464 level value
 * @return dBov value (0 to -127)
 */
static inline float voice_audio_level_rfc6464_to_db(uint8_t level) {
    return -(float)(level & 0x7F);
}

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_LEVEL_H */
