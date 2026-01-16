/**
 * @file audio_recorder.h
 * @brief Audio recording and playback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Audio recording module with support for file and memory buffer recording
 */

#ifndef VOICE_AUDIO_RECORDER_H
#define VOICE_AUDIO_RECORDER_H

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

typedef struct voice_recorder_s voice_recorder_t;
typedef struct voice_player_s voice_player_t;

/** Recording format */
typedef enum {
    VOICE_RECORD_WAV,           /**< WAV file */
    VOICE_RECORD_RAW,           /**< Raw PCM */
    VOICE_RECORD_OGG_OPUS,      /**< Ogg Opus */
    VOICE_RECORD_MEMORY,        /**< Memory buffer */
} voice_record_format_t;

/** Recording source */
typedef enum {
    VOICE_RECORD_SOURCE_INPUT,  /**< Input (microphone) */
    VOICE_RECORD_SOURCE_OUTPUT, /**< Output (speaker) */
    VOICE_RECORD_SOURCE_BOTH,   /**< Both (mixed) */
} voice_record_source_t;

/* ============================================
 * Recorder Configuration
 * ============================================ */

typedef struct {
    voice_record_format_t format;
    voice_record_source_t source;

    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;

    /* File recording */
    const char *filename;           /**< Output filename (if recording to file) */
    bool append;                    /**< Append to existing file */

    /* Memory recording */
    size_t max_memory_bytes;        /**< Maximum memory usage (0=unlimited) */
    bool circular_buffer;           /**< Use circular buffer */

    /* Limits */
    uint64_t max_duration_ms;       /**< Maximum recording duration (0=unlimited) */
    uint64_t max_file_size;         /**< Maximum file size (0=unlimited) */

    /* Callbacks */
    void (*on_data)(const int16_t *samples, size_t count, void *user_data);
    void (*on_complete)(const char *filename, uint64_t duration_ms, void *user_data);
    void *callback_user_data;
} voice_recorder_config_t;

/* ============================================
 * Recording Status
 * ============================================ */

typedef struct {
    bool is_recording;
    uint64_t duration_ms;           /**< Recorded duration */
    uint64_t samples_recorded;      /**< Recorded sample count */
    uint64_t bytes_written;         /**< Bytes written */
    float peak_level_db;            /**< Peak level */
    float avg_level_db;             /**< Average level */
} voice_recorder_status_t;

/* ============================================
 * Player Configuration
 * ============================================ */

typedef struct {
    uint32_t sample_rate;           /**< Target sample rate (0=use file's sample rate) */
    float playback_speed;           /**< Playback speed (1.0=normal) */
    float volume;                   /**< Volume (0.0-1.0) */
    bool loop;                      /**< Loop playback */

    /* Callbacks */
    void (*on_complete)(void *user_data);
    void (*on_position)(uint64_t position_ms, uint64_t duration_ms, void *user_data);
    void *callback_user_data;
} voice_player_config_t;

/* ============================================
 * Playback Status
 * ============================================ */

typedef struct {
    bool is_playing;
    bool is_paused;
    uint64_t position_ms;           /**< Current position */
    uint64_t duration_ms;           /**< Total duration */
    float volume;                   /**< Current volume */
    float playback_speed;           /**< Current speed */
} voice_player_status_t;

/* ============================================
 * Recorder API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_recorder_config_init(voice_recorder_config_t *config);

/**
 * @brief Create recorder
 */
VOICE_API voice_recorder_t *voice_recorder_create(const voice_recorder_config_t *config);

/**
 * @brief Destroy recorder
 */
VOICE_API void voice_recorder_destroy(voice_recorder_t *recorder);

/**
 * @brief Start recording
 */
VOICE_API voice_error_t voice_recorder_start(voice_recorder_t *recorder);

/**
 * @brief Stop recording
 */
VOICE_API voice_error_t voice_recorder_stop(voice_recorder_t *recorder);

/**
 * @brief Pause recording
 */
VOICE_API voice_error_t voice_recorder_pause(voice_recorder_t *recorder);

/**
 * @brief Resume recording
 */
VOICE_API voice_error_t voice_recorder_resume(voice_recorder_t *recorder);

/**
 * @brief Write audio data
 */
VOICE_API voice_error_t voice_recorder_write(
    voice_recorder_t *recorder,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief Get recording status
 */
VOICE_API voice_error_t voice_recorder_get_status(
    voice_recorder_t *recorder,
    voice_recorder_status_t *status
);

/**
 * @brief Get recorded data (memory mode)
 */
VOICE_API voice_error_t voice_recorder_get_data(
    voice_recorder_t *recorder,
    int16_t **samples,
    size_t *num_samples
);

/**
 * @brief Save to file (memory mode)
 */
VOICE_API voice_error_t voice_recorder_save_to_file(
    voice_recorder_t *recorder,
    const char *filename,
    voice_record_format_t format
);

/* ============================================
 * Player API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_player_config_init(voice_player_config_t *config);

/**
 * @brief Create player from file
 */
VOICE_API voice_player_t *voice_player_create_from_file(
    const char *filename,
    const voice_player_config_t *config
);

/**
 * @brief Create player from memory
 */
VOICE_API voice_player_t *voice_player_create_from_memory(
    const int16_t *samples,
    size_t num_samples,
    uint32_t sample_rate,
    const voice_player_config_t *config
);

/**
 * @brief Destroy player
 */
VOICE_API void voice_player_destroy(voice_player_t *player);

/**
 * @brief Start playback
 */
VOICE_API voice_error_t voice_player_play(voice_player_t *player);

/**
 * @brief Stop playback
 */
VOICE_API voice_error_t voice_player_stop(voice_player_t *player);

/**
 * @brief Pause playback
 */
VOICE_API voice_error_t voice_player_pause(voice_player_t *player);

/**
 * @brief Resume playback
 */
VOICE_API voice_error_t voice_player_resume(voice_player_t *player);

/**
 * @brief Seek to position
 */
VOICE_API voice_error_t voice_player_seek(voice_player_t *player, uint64_t position_ms);

/**
 * @brief Set volume
 */
VOICE_API voice_error_t voice_player_set_volume(voice_player_t *player, float volume);

/**
 * @brief Set playback speed
 */
VOICE_API voice_error_t voice_player_set_speed(voice_player_t *player, float speed);

/**
 * @brief Read audio data (for Pipeline use)
 */
VOICE_API size_t voice_player_read(
    voice_player_t *player,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief Get playback status
 */
VOICE_API voice_error_t voice_player_get_status(
    voice_player_t *player,
    voice_player_status_t *status
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_RECORDER_H */
