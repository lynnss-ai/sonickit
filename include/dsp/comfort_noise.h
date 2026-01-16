/**
 * @file comfort_noise.h
 * @brief Comfort Noise Generation (CNG)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Comfort noise generation module for producing background noise during silence.
 * Prevents the "dead silence" feeling during calls.
 */

#ifndef VOICE_COMFORT_NOISE_H
#define VOICE_COMFORT_NOISE_H

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

typedef struct voice_cng_s voice_cng_t;

/** CNG noise type */
typedef enum {
    VOICE_CNG_WHITE,            /**< White noise */
    VOICE_CNG_PINK,             /**< Pink noise */
    VOICE_CNG_BROWN,            /**< Brown noise */
    VOICE_CNG_SPECTRAL,         /**< Spectral matching based */
} voice_cng_type_t;

/* ============================================
 * SID Frame (Silence Insertion Descriptor)
 * RFC 3389
 * ============================================ */

typedef struct {
    uint8_t noise_level;        /**< Noise level (0-127) */
    uint8_t spectral_params[12];/**< Spectral parameters (optional) */
    size_t param_count;         /**< Parameter count */
} voice_sid_frame_t;

/* ============================================
 * Configuration
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    voice_cng_type_t noise_type;

    /* Level control */
    float noise_level_db;       /**< Comfort noise level (dBFS), typically -45 to -60 */
    bool auto_level;            /**< Auto-adjust level (based on input noise) */

    /* Smoothing */
    float transition_time_ms;   /**< Silence/speech transition time */

    /* Spectral matching */
    bool enable_spectral_match; /**< Enable spectral matching */
    size_t spectral_bands;      /**< Number of spectral analysis bands */
} voice_cng_config_t;

/* ============================================
 * CNG API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_cng_config_init(voice_cng_config_t *config);

/**
 * @brief Create CNG instance
 */
VOICE_API voice_cng_t *voice_cng_create(const voice_cng_config_t *config);

/**
 * @brief Destroy CNG instance
 */
VOICE_API void voice_cng_destroy(voice_cng_t *cng);

/**
 * @brief Analyze input noise characteristics
 * Called when silence is detected, used to learn background noise properties
 */
VOICE_API voice_error_t voice_cng_analyze(
    voice_cng_t *cng,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief Generate comfort noise
 * @param cng CNG instance
 * @param output Output buffer
 * @param num_samples Number of samples to generate
 */
VOICE_API voice_error_t voice_cng_generate(
    voice_cng_t *cng,
    int16_t *output,
    size_t num_samples
);

/**
 * @brief Generate comfort noise (float)
 */
VOICE_API voice_error_t voice_cng_generate_float(
    voice_cng_t *cng,
    float *output,
    size_t num_samples
);

/**
 * @brief Set noise level
 */
VOICE_API voice_error_t voice_cng_set_level(voice_cng_t *cng, float level_db);

/**
 * @brief Get current noise level
 */
VOICE_API float voice_cng_get_level(voice_cng_t *cng);

/**
 * @brief Encode SID frame (RFC 3389)
 */
VOICE_API voice_error_t voice_cng_encode_sid(
    voice_cng_t *cng,
    voice_sid_frame_t *sid
);

/**
 * @brief Decode SID frame and update CNG parameters
 */
VOICE_API voice_error_t voice_cng_decode_sid(
    voice_cng_t *cng,
    const voice_sid_frame_t *sid
);

/**
 * @brief Reset CNG
 */
VOICE_API void voice_cng_reset(voice_cng_t *cng);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_COMFORT_NOISE_H */
