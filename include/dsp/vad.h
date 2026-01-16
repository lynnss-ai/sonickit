/**
 * @file vad.h
 * @brief Voice Activity Detection (VAD) interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Voice activity detection module, supports multiple VAD algorithms.
 */

#ifndef VOICE_VAD_H
#define VOICE_VAD_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * VAD Type Definitions
 * ============================================ */

typedef struct voice_vad_s voice_vad_t;

/** VAD algorithm type */
typedef enum {
    VOICE_VAD_ENERGY,      /**< Energy threshold based */
    VOICE_VAD_SPEEX,       /**< SpeexDSP VAD */
    VOICE_VAD_WEBRTC,      /**< WebRTC VAD (if available) */
    VOICE_VAD_RNNOISE,     /**< RNNoise VAD (based on speech probability) */
} voice_vad_mode_t;

/** VAD aggressiveness */
typedef enum {
    VOICE_VAD_QUALITY = 0,    /**< High quality - fewer false positives */
    VOICE_VAD_LOW_BITRATE,    /**< Low bitrate - balanced */
    VOICE_VAD_AGGRESSIVE,     /**< Aggressive - fast response */
    VOICE_VAD_VERY_AGGRESSIVE /**< Very aggressive - may miss speech start */
} voice_vad_aggressiveness_t;

/* ============================================
 * VAD Configuration
 * ============================================ */

typedef struct {
    voice_vad_mode_t mode;
    voice_vad_aggressiveness_t aggressiveness;
    uint32_t sample_rate;
    uint32_t frame_size_ms;     /**< Frame duration (10/20/30 ms) */

    /* Energy VAD parameters */
    float energy_threshold_db;   /**< Energy threshold (dB) */
    float speech_hangover_ms;    /**< Delay after speech ends (ms) */

    /* Adaptive parameters */
    bool adaptive_threshold;     /**< Adaptive threshold */
    float adaptation_rate;       /**< Adaptation rate */
} voice_vad_config_t;

/* ============================================
 * VAD Result
 * ============================================ */

typedef struct {
    bool is_speech;             /**< Whether contains speech */
    float speech_probability;   /**< Speech probability [0, 1] */
    float energy_db;            /**< Frame energy (dB) */
    uint32_t speech_frames;     /**< Consecutive speech frames */
    uint32_t silence_frames;    /**< Consecutive silence frames */
} voice_vad_result_t;

/* ============================================
 * VAD API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_vad_config_init(voice_vad_config_t *config);

/**
 * @brief Create VAD instance
 */
VOICE_API voice_vad_t *voice_vad_create(const voice_vad_config_t *config);

/**
 * @brief Destroy VAD instance
 */
VOICE_API void voice_vad_destroy(voice_vad_t *vad);

/**
 * @brief Process audio frame
 * @param vad VAD instance
 * @param samples Audio samples (16-bit PCM)
 * @param num_samples Number of samples
 * @param result Output result
 * @return Error code
 */
VOICE_API voice_error_t voice_vad_process(
    voice_vad_t *vad,
    const int16_t *samples,
    size_t num_samples,
    voice_vad_result_t *result
);

/**
 * @brief Reset VAD state
 */
VOICE_API void voice_vad_reset(voice_vad_t *vad);

/**
 * @brief Set aggressiveness
 */
VOICE_API voice_error_t voice_vad_set_aggressiveness(
    voice_vad_t *vad,
    voice_vad_aggressiveness_t aggressiveness
);

/**
 * @brief Get VAD statistics
 */
typedef struct {
    uint64_t total_frames;
    uint64_t speech_frames;
    uint64_t silence_frames;
    float average_speech_probability;
    float speech_ratio;         /**< Speech ratio */
} voice_vad_stats_t;

VOICE_API voice_error_t voice_vad_get_stats(
    voice_vad_t *vad,
    voice_vad_stats_t *stats
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_VAD_H */
