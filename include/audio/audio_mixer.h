/**
 * @file audio_mixer.h
 * @brief Audio mixer for multi-stream mixing
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Audio mixer with multi-stream mixing support
 */

#ifndef VOICE_AUDIO_MIXER_H
#define VOICE_AUDIO_MIXER_H

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

typedef struct voice_mixer_s voice_mixer_t;
typedef uint32_t mixer_source_id_t;

#define MIXER_INVALID_SOURCE_ID 0

/** Mixing algorithm */
typedef enum {
    VOICE_MIX_SIMPLE_ADD,       /**< Simple addition (may overflow) */
    VOICE_MIX_AVERAGE,          /**< Average */
    VOICE_MIX_NORMALIZED,       /**< Normalized mixing (recommended) */
    VOICE_MIX_LOUDEST_N,        /**< Mix only the N loudest sources */
} voice_mix_algorithm_t;

/* ============================================
 * Mixer Configuration
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    uint32_t frame_size;        /**< Samples per frame */
    size_t max_sources;         /**< Maximum number of sources */
    voice_mix_algorithm_t algorithm;

    /* Loudest N parameters */
    size_t loudest_n;           /**< Mix the N loudest sources */

    /* Output control */
    float master_gain;          /**< Master gain (0.0-2.0) */
    bool enable_limiter;        /**< Enable limiter to prevent clipping */
    float limiter_threshold_db; /**< Limiter threshold */
} voice_mixer_config_t;

/* ============================================
 * Source Configuration
 * ============================================ */

typedef struct {
    float gain;                 /**< Source gain (0.0-2.0) */
    float pan;                  /**< Panning (-1.0 left, 0 center, 1.0 right) */
    bool muted;                 /**< Muted */
    uint32_t priority;          /**< Priority (for Loudest N) */
    void *user_data;            /**< User data */
} voice_mixer_source_config_t;

/* ============================================
 * Mixer API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_mixer_config_init(voice_mixer_config_t *config);

/**
 * @brief Create mixer
 */
VOICE_API voice_mixer_t *voice_mixer_create(const voice_mixer_config_t *config);

/**
 * @brief Destroy mixer
 */
VOICE_API void voice_mixer_destroy(voice_mixer_t *mixer);

/**
 * @brief Add source
 * @return Source ID, returns MIXER_INVALID_SOURCE_ID on failure
 */
VOICE_API mixer_source_id_t voice_mixer_add_source(
    voice_mixer_t *mixer,
    const voice_mixer_source_config_t *config
);

/**
 * @brief Remove source
 */
VOICE_API voice_error_t voice_mixer_remove_source(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id
);

/**
 * @brief Push audio data to source
 */
VOICE_API voice_error_t voice_mixer_push_audio(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief Mix and get output
 * @param mixer Mixer
 * @param output Output buffer
 * @param num_samples Requested number of samples
 * @param samples_out Actual number of samples output
 */
VOICE_API voice_error_t voice_mixer_get_output(
    voice_mixer_t *mixer,
    int16_t *output,
    size_t num_samples,
    size_t *samples_out
);

/**
 * @brief Set source gain
 */
VOICE_API voice_error_t voice_mixer_set_source_gain(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    float gain
);

/**
 * @brief Set source muted state
 */
VOICE_API voice_error_t voice_mixer_set_source_muted(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    bool muted
);

/**
 * @brief Set master gain
 */
VOICE_API voice_error_t voice_mixer_set_master_gain(
    voice_mixer_t *mixer,
    float gain
);

/**
 * @brief Get active source count
 */
VOICE_API size_t voice_mixer_get_active_sources(voice_mixer_t *mixer);

/**
 * @brief Mixer statistics
 */
typedef struct {
    size_t active_sources;
    size_t total_sources;
    uint64_t frames_mixed;
    float peak_level_db;
    uint32_t clip_count;        /**< Clipping count */
} voice_mixer_stats_t;

VOICE_API voice_error_t voice_mixer_get_stats(
    voice_mixer_t *mixer,
    voice_mixer_stats_t *stats
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_MIXER_H */
