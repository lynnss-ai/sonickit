/**
 * @file equalizer.h
 * @brief Multi-band parametric equalizer
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Provides multi-band parametric equalizer for adjusting audio frequency response.
 */

#ifndef VOICE_EQUALIZER_H
#define VOICE_EQUALIZER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Type Definitions
 * ============================================ */

/**
 * @brief Filter type
 */
typedef enum {
    VOICE_EQ_LOWPASS,       /**< Low-pass filter */
    VOICE_EQ_HIGHPASS,      /**< High-pass filter */
    VOICE_EQ_BANDPASS,      /**< Band-pass filter */
    VOICE_EQ_NOTCH,         /**< Notch filter */
    VOICE_EQ_PEAK,          /**< Peak filter (parametric EQ) */
    VOICE_EQ_LOWSHELF,      /**< Low-shelf filter */
    VOICE_EQ_HIGHSHELF      /**< High-shelf filter */
} voice_eq_filter_type_t;

/**
 * @brief Equalizer band configuration
 */
typedef struct {
    bool enabled;                   /**< Whether band is enabled */
    voice_eq_filter_type_t type;    /**< Filter type */
    float frequency;                /**< Center/cutoff frequency (Hz) */
    float gain_db;                  /**< Gain (dB) [-24, +24] */
    float q;                        /**< Q value (quality factor) [0.1, 10] */
} voice_eq_band_t;

/**
 * @brief Preset type
 */
typedef enum {
    VOICE_EQ_PRESET_FLAT,           /**< Flat response */
    VOICE_EQ_PRESET_VOICE_ENHANCE,  /**< Voice enhancement */
    VOICE_EQ_PRESET_TELEPHONE,      /**< Telephone quality (300-3400Hz) */
    VOICE_EQ_PRESET_BASS_BOOST,     /**< Bass boost */
    VOICE_EQ_PRESET_TREBLE_BOOST,   /**< Treble boost */
    VOICE_EQ_PRESET_REDUCE_NOISE,   /**< Noise reduction tendency (attenuate high freq) */
    VOICE_EQ_PRESET_CLARITY,        /**< Clarity enhancement */
    VOICE_EQ_PRESET_CUSTOM          /**< Custom */
} voice_eq_preset_t;

/**
 * @brief Equalizer configuration
 */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate */
    uint32_t num_bands;         /**< Number of bands [1, 10] */
    voice_eq_band_t *bands;     /**< Band configuration array */
    float master_gain_db;       /**< Master gain (dB) */
} voice_eq_config_t;

/* ============================================
 * Forward Declarations
 * ============================================ */

typedef struct voice_eq_s voice_eq_t;

/* ============================================
 * Configuration Initialization
 * ============================================ */

/**
 * @brief Initialize equalizer configuration to default values
 */
VOICE_API void voice_eq_config_init(voice_eq_config_t *config);

/**
 * @brief Load configuration from preset
 */
VOICE_API voice_error_t voice_eq_config_from_preset(
    voice_eq_config_t *config,
    voice_eq_preset_t preset,
    uint32_t sample_rate
);

/* ============================================
 * Equalizer Lifecycle
 * ============================================ */

/**
 * @brief Create equalizer instance
 */
VOICE_API voice_eq_t *voice_eq_create(const voice_eq_config_t *config);

/**
 * @brief Destroy equalizer instance
 */
VOICE_API void voice_eq_destroy(voice_eq_t *eq);

/* ============================================
 * Processing Interface
 * ============================================ */

/**
 * @brief Process audio data (in-place)
 */
VOICE_API voice_error_t voice_eq_process(
    voice_eq_t *eq,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief Process float audio data
 */
VOICE_API voice_error_t voice_eq_process_float(
    voice_eq_t *eq,
    float *samples,
    size_t num_samples
);

/* ============================================
 * Band Control
 * ============================================ */

/**
 * @brief Set band parameters
 */
VOICE_API voice_error_t voice_eq_set_band(
    voice_eq_t *eq,
    uint32_t band_index,
    const voice_eq_band_t *band
);

/**
 * @brief Get band parameters
 */
VOICE_API VOICE_API voice_error_t voice_eq_get_band(
    voice_eq_t *eq,
    uint32_t band_index,
    voice_eq_band_t *band
);

/**
 * @brief Enable/disable band
 */
VOICE_API voice_error_t voice_eq_enable_band(
    voice_eq_t *eq,
    uint32_t band_index,
    bool enable
);

/**
 * @brief Set master gain
 */
VOICE_API voice_error_t voice_eq_set_master_gain(voice_eq_t *eq, float gain_db);

/**
 * @brief Apply preset
 */
VOICE_API voice_error_t voice_eq_apply_preset(
    voice_eq_t *eq,
    voice_eq_preset_t preset
);

/* ============================================
 * State Query
 * ============================================ */

/**
 * @brief Get frequency response
 * @param frequencies Frequency point array (Hz)
 * @param responses Response array (dB), output parameter
 * @param count Array size
 */
VOICE_API voice_error_t voice_eq_get_frequency_response(
    voice_eq_t *eq,
    const float *frequencies,
    float *responses,
    size_t count
);

/**
 * @brief Reset equalizer state
 */
VOICE_API void voice_eq_reset(voice_eq_t *eq);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_EQUALIZER_H */
