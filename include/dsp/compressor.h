/**
 * @file compressor.h
 * @brief Dynamic range compressor/expander/limiter
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Provides dynamic range processing: compression, expansion, limiting.
 */

#ifndef VOICE_COMPRESSOR_H
#define VOICE_COMPRESSOR_H

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
 * @brief Processor type
 */
typedef enum {
    VOICE_DRC_COMPRESSOR,       /**< Compressor (reduce dynamic range) */
    VOICE_DRC_EXPANDER,         /**< Expander (increase dynamic range) */
    VOICE_DRC_LIMITER,          /**< Limiter (hard limit peaks) */
    VOICE_DRC_GATE              /**< Noise gate (mute low levels) */
} voice_drc_type_t;

/**
 * @brief Detection mode
 */
typedef enum {
    VOICE_DRC_PEAK,             /**< Peak detection */
    VOICE_DRC_RMS,              /**< RMS detection */
    VOICE_DRC_TRUE_PEAK         /**< True peak detection (oversampled) */
} voice_drc_detection_t;

/**
 * @brief Knee mode
 */
typedef enum {
    VOICE_DRC_HARD_KNEE,        /**< Hard knee */
    VOICE_DRC_SOFT_KNEE         /**< Soft knee */
} voice_drc_knee_t;

/**
 * @brief Compressor configuration
 */
typedef struct {
    /* Basic parameters */
    voice_drc_type_t type;          /**< Processor type */
    uint32_t sample_rate;           /**< Sample rate */

    /* Level parameters */
    float threshold_db;             /**< Threshold (dBFS) [-60, 0] */
    float ratio;                    /**< Compression ratio [1:1, infinity:1] (compressor: >1, expander: <1) */
    float knee_width_db;            /**< Soft knee width (dB) [0, 24] */
    voice_drc_knee_t knee_type;     /**< Knee type */

    /* Time parameters */
    float attack_ms;                /**< Attack time (ms) [0.1, 100] */
    float release_ms;               /**< Release time (ms) [10, 5000] */
    float hold_ms;                  /**< Hold time (ms) [0, 500] */

    /* Gain parameters */
    float makeup_gain_db;           /**< Makeup gain (dB) */
    bool auto_makeup;               /**< Auto makeup gain */

    /* Detection parameters */
    voice_drc_detection_t detection; /**< Detection mode */
    float lookahead_ms;             /**< Lookahead time (ms) [0, 10] */

    /* Sidechain */
    bool enable_sidechain;          /**< Enable external sidechain */
    float sidechain_hpf;            /**< Sidechain high-pass filter frequency (Hz), 0=disabled */
} voice_compressor_config_t;

/**
 * @brief Compressor state
 */
typedef struct {
    float input_level_db;       /**< Input level (dBFS) */
    float output_level_db;      /**< Output level (dBFS) */
    float gain_reduction_db;    /**< Gain reduction amount (dB) */
    float current_ratio;        /**< Current compression ratio */
    bool is_compressing;        /**< Whether currently compressing */
} voice_compressor_state_t;

/* ============================================
 * Forward Declarations
 * ============================================ */

typedef struct voice_compressor_s voice_compressor_t;

/* ============================================
 * Configuration Initialization
 * ============================================ */

/**
 * @brief Initialize compressor configuration (default for voice compression)
 */
VOICE_API void voice_compressor_config_init(voice_compressor_config_t *config);

/**
 * @brief Initialize limiter configuration
 */
VOICE_API void voice_limiter_config_init(voice_compressor_config_t *config);

/**
 * @brief Initialize noise gate configuration
 */
VOICE_API void voice_gate_config_init(voice_compressor_config_t *config);

/* ============================================
 * Lifecycle
 * ============================================ */

/**
 * @brief Create compressor instance
 */
VOICE_API voice_compressor_t *voice_compressor_create(const voice_compressor_config_t *config);

/**
 * @brief Destroy compressor instance
 */
VOICE_API void voice_compressor_destroy(voice_compressor_t *comp);

/* ============================================
 * Processing Interface
 * ============================================ */

/**
 * @brief Process audio data (in-place)
 */
VOICE_API voice_error_t voice_compressor_process(
    voice_compressor_t *comp,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief Process float audio data
 */
VOICE_API voice_error_t voice_compressor_process_float(
    voice_compressor_t *comp,
    float *samples,
    size_t num_samples
);

/**
 * @brief Process using external sidechain signal
 */
VOICE_API voice_error_t voice_compressor_process_sidechain(
    voice_compressor_t *comp,
    int16_t *samples,
    const int16_t *sidechain,
    size_t num_samples
);

/* ============================================
 * Parameter Control
 * ============================================ */

/**
 * @brief Set threshold
 */
VOICE_API voice_error_t voice_compressor_set_threshold(
    voice_compressor_t *comp,
    float threshold_db
);

/**
 * @brief Set compression ratio
 */
VOICE_API voice_error_t voice_compressor_set_ratio(
    voice_compressor_t *comp,
    float ratio
);

/**
 * @brief Set attack/release time
 */
VOICE_API voice_error_t voice_compressor_set_times(
    voice_compressor_t *comp,
    float attack_ms,
    float release_ms
);

/**
 * @brief Set makeup gain
 */
VOICE_API voice_error_t voice_compressor_set_makeup_gain(
    voice_compressor_t *comp,
    float gain_db
);

/* ============================================
 * State Query
 * ============================================ */

/**
 * @brief Get compressor state
 */
VOICE_API voice_error_t voice_compressor_get_state(
    voice_compressor_t *comp,
    voice_compressor_state_t *state
);

/**
 * @brief Reset compressor state
 */
VOICE_API void voice_compressor_reset(voice_compressor_t *comp);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_COMPRESSOR_H */
