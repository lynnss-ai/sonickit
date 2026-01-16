/**
 * @file file_io.h
 * @brief Audio file I/O interface (WAV/MP3/FLAC)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef AUDIO_FILE_IO_H
#define AUDIO_FILE_IO_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * File Formats
 * ============================================ */

typedef enum {
    AUDIO_FILE_FORMAT_UNKNOWN = 0,
    AUDIO_FILE_FORMAT_WAV,
    AUDIO_FILE_FORMAT_MP3,
    AUDIO_FILE_FORMAT_FLAC,
    AUDIO_FILE_FORMAT_OGG,
} audio_file_format_t;

/* ============================================
 * File Info
 * ============================================ */

typedef struct {
    audio_file_format_t format;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_frames;      /**< Total frames (samples per channel) */
    double duration_seconds;    /**< Duration (seconds) */
    uint32_t bitrate;           /**< Bitrate (bps, compressed formats only) */
} audio_file_info_t;

/* ============================================
 * File Reader
 * ============================================ */

typedef struct audio_reader_s audio_reader_t;

/**
 * @brief Open audio file for reading
 * @param path File path
 * @return Reader handle
 */
VOICE_API audio_reader_t *audio_reader_open(const char *path);

/**
 * @brief Open audio data from memory
 * @param data Audio data
 * @param size Data size
 * @return Reader handle
 */
VOICE_API audio_reader_t *audio_reader_open_memory(const void *data, size_t size);

/**
 * @brief Close reader
 */
VOICE_API void audio_reader_close(audio_reader_t *reader);

/**
 * @brief Get audio info
 */
VOICE_API voice_error_t audio_reader_get_info(
    audio_reader_t *reader,
    audio_file_info_t *info
);

/**
 * @brief Read PCM data (int16)
 * @param reader Reader handle
 * @param output Output buffer
 * @param frames_to_read Frames to read
 * @param frames_read Actual frames read
 * @return Error code
 */
VOICE_API voice_error_t audio_reader_read_s16(
    audio_reader_t *reader,
    int16_t *output,
    size_t frames_to_read,
    size_t *frames_read
);

/**
 * @brief Read PCM data (float)
 * @param reader Reader handle
 * @param output Output buffer
 * @param frames_to_read Frames to read
 * @param frames_read Actual frames read
 * @return Error code
 */
VOICE_API voice_error_t audio_reader_read_f32(
    audio_reader_t *reader,
    float *output,
    size_t frames_to_read,
    size_t *frames_read
);

/**
 * @brief Seek to specified frame
 * @param reader Reader handle
 * @param frame Target frame number
 * @return Error code
 */
VOICE_API voice_error_t audio_reader_seek(
    audio_reader_t *reader,
    uint64_t frame
);

/**
 * @brief Get current frame position
 */
VOICE_API uint64_t audio_reader_tell(audio_reader_t *reader);

/**
 * @brief Check if end of file reached
 */
VOICE_API bool audio_reader_is_eof(audio_reader_t *reader);

/* ============================================
 * File Writer
 * ============================================ */

typedef struct audio_writer_s audio_writer_t;

/** Writer configuration */
typedef struct {
    audio_file_format_t format;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;    /**< WAV: 8/16/24/32 */
    uint32_t bitrate;           /**< MP3/compressed format bitrate */
    int quality;                /**< Encoding quality (0-10) */
} audio_writer_config_t;

/**
 * @brief Initialize default writer configuration
 */
VOICE_API void audio_writer_config_init(audio_writer_config_t *config);

/**
 * @brief Create audio file writer
 * @param path File path
 * @param config Configuration
 * @return Writer handle
 */
VOICE_API audio_writer_t *audio_writer_create(
    const char *path,
    const audio_writer_config_t *config
);

/**
 * @brief Close writer (complete file writing)
 */
VOICE_API voice_error_t audio_writer_close(audio_writer_t *writer);

/**
 * @brief Write PCM data (int16)
 * @param writer Writer handle
 * @param input Input buffer
 * @param frames Frame count
 * @return Error code
 */
VOICE_API voice_error_t audio_writer_write_s16(
    audio_writer_t *writer,
    const int16_t *input,
    size_t frames
);

/**
 * @brief Write PCM data (float)
 * @param writer Writer handle
 * @param input Input buffer
 * @param frames Frame count
 * @return Error code
 */
VOICE_API voice_error_t audio_writer_write_f32(
    audio_writer_t *writer,
    const float *input,
    size_t frames
);

/**
 * @brief Get frames written
 */
VOICE_API uint64_t audio_writer_get_frames_written(audio_writer_t *writer);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Infer format from file extension
 */
VOICE_API audio_file_format_t audio_file_format_from_path(const char *path);

/**
 * @brief Get format name
 */
VOICE_API const char *audio_file_format_name(audio_file_format_t format);

/**
 * @brief Check if format is supported
 */
VOICE_API bool audio_file_format_supported(audio_file_format_t format);

/* ============================================
 * Compatibility API (for example code)
 * ============================================ */

/**
 * @brief Get audio info (simplified - legacy example compatibility)
 * @param reader Reader handle
 * @param sample_rate Sample rate output
 * @param channels Channel count output
 * @param total_frames Total frames output
 * @return Error code
 */
static inline voice_error_t audio_reader_get_info_ex(
    audio_reader_t *reader,
    uint32_t *sample_rate,
    uint8_t *channels,
    uint64_t *total_frames)
{
    audio_file_info_t info;
    voice_error_t err = audio_reader_get_info(reader, &info);
    if (err == VOICE_OK) {
        if (sample_rate) *sample_rate = info.sample_rate;
        if (channels) *channels = info.channels;
        if (total_frames) *total_frames = info.total_frames;
    }
    return err;
}

/**
 * @brief Create audio writer (simplified - legacy example compatibility)
 * @param path File path
 * @param sample_rate Sample rate
 * @param channels Channel count
 * @return Writer handle
 */
VOICE_API audio_writer_t *audio_writer_create_simple(
    const char *path,
    uint32_t sample_rate,
    uint8_t channels
);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_FILE_IO_H */
