/**
 * @file file_io.c
 * @brief Audio file I/O implementation using miniaudio
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/file_io.h"
#include <stdlib.h>
#include <string.h>

/* miniaudio decoder configuration - use third_party implementation */
#ifndef MA_NO_DEVICE_IO
#define MA_NO_DEVICE_IO
#endif
#include "miniaudio.h"

/* ============================================
 * Reader Structure
 * ============================================ */

struct audio_reader_s {
    ma_decoder decoder;
    audio_file_info_t info;
    bool initialized;
    bool is_eof;
};

/* ============================================
 * Writer Structure
 * ============================================ */

struct audio_writer_s {
    ma_encoder encoder;
    audio_writer_config_t config;
    uint64_t frames_written;
    bool initialized;
};

/* ============================================
 * Format Utility Functions
 * ============================================ */

audio_file_format_t audio_file_format_from_path(const char *path)
{
    if (!path) {
        return AUDIO_FILE_FORMAT_UNKNOWN;
    }

    const char *ext = strrchr(path, '.');
    if (!ext) {
        return AUDIO_FILE_FORMAT_UNKNOWN;
    }

    ext++;  /* Skip the '.' character */

    if (strcasecmp(ext, "wav") == 0 || strcasecmp(ext, "wave") == 0) {
        return AUDIO_FILE_FORMAT_WAV;
    }
    if (strcasecmp(ext, "mp3") == 0) {
        return AUDIO_FILE_FORMAT_MP3;
    }
    if (strcasecmp(ext, "flac") == 0) {
        return AUDIO_FILE_FORMAT_FLAC;
    }
    if (strcasecmp(ext, "ogg") == 0) {
        return AUDIO_FILE_FORMAT_OGG;
    }

    return AUDIO_FILE_FORMAT_UNKNOWN;
}

const char *audio_file_format_name(audio_file_format_t format)
{
    switch (format) {
    case AUDIO_FILE_FORMAT_WAV:
        return "WAV";
    case AUDIO_FILE_FORMAT_MP3:
        return "MP3";
    case AUDIO_FILE_FORMAT_FLAC:
        return "FLAC";
    case AUDIO_FILE_FORMAT_OGG:
        return "OGG";
    default:
        return "Unknown";
    }
}

bool audio_file_format_supported(audio_file_format_t format)
{
    switch (format) {
    case AUDIO_FILE_FORMAT_WAV:
    case AUDIO_FILE_FORMAT_MP3:
    case AUDIO_FILE_FORMAT_FLAC:
        return true;
    default:
        return false;
    }
}

/* ============================================
 * Reader Implementation
 * ============================================ */

/**
 * @brief Open an audio file for reading
 * @details This function initializes an audio reader by opening the specified audio file.
 *          It uses miniaudio decoder to handle various audio formats (WAV, MP3, FLAC, etc.).
 *          The decoder is configured to output 16-bit PCM samples with the source's
 *          original sample rate and channel count. After successful initialization,
 *          file metadata including format, sample rate, channels, total frames,
 *          and duration are extracted and stored in the reader structure.
 * @param path The file path to the audio file to be opened
 * @return Pointer to the initialized audio_reader_t structure on success, NULL on failure
 * @note The caller is responsible for closing the reader using audio_reader_close()
 *       when finished to free allocated resources.
 */
audio_reader_t *audio_reader_open(const char *path)
{
    if (!path) {
        return NULL;
    }

    audio_reader_t *reader = (audio_reader_t *)calloc(1, sizeof(audio_reader_t));
    if (!reader) {
        return NULL;
    }

    ma_decoder_config config = ma_decoder_config_init(
        ma_format_s16,  /* Default output format: 16-bit signed integer */
        0,              /* Use source channel count */
        0               /* Use source sample rate */
    );

    ma_result result = ma_decoder_init_file(path, &config, &reader->decoder);
    if (result != MA_SUCCESS) {
        VOICE_LOG_E("Failed to open audio file: %s, error: %d", path, result);
        free(reader);
        return NULL;
    }

    /* Retrieve file information */
    reader->info.format = audio_file_format_from_path(path);
    reader->info.sample_rate = reader->decoder.outputSampleRate;
    reader->info.channels = reader->decoder.outputChannels;
    reader->info.bits_per_sample = 16;  /* miniaudio default output bit depth */

    /* Retrieve total frame count */
    ma_uint64 total_frames;
    if (ma_decoder_get_length_in_pcm_frames(&reader->decoder, &total_frames) == MA_SUCCESS) {
        reader->info.total_frames = total_frames;
        reader->info.duration_seconds =
            (double)total_frames / reader->info.sample_rate;
    }

    reader->initialized = true;

    VOICE_LOG_I("Audio file opened: %s, %uHz, %uch, %.2fs",
        path, reader->info.sample_rate, reader->info.channels,
        reader->info.duration_seconds);

    return reader;
}

/**
 * @brief Open an audio file from memory buffer for reading
 * @details This function creates an audio reader from a memory buffer containing
 *          audio file data. Similar to audio_reader_open(), it initializes a miniaudio
 *          decoder but reads from the provided memory buffer instead of a file.
 *          The decoder outputs 16-bit PCM samples and preserves the source audio's
 *          original sample rate and channel configuration. This is useful for
 *          processing audio data that has been loaded into memory from network,
 *          embedded resources, or other non-file sources.
 * @param data Pointer to the memory buffer containing the audio file data
 * @param size Size of the memory buffer in bytes
 * @return Pointer to the initialized audio_reader_t structure on success, NULL on failure
 * @note The caller must ensure the memory buffer remains valid for the lifetime of the reader.
 *       Call audio_reader_close() when finished to free the reader resources.
 */
audio_reader_t *audio_reader_open_memory(const void *data, size_t size)
{
    if (!data || size == 0) {
        return NULL;
    }

    audio_reader_t *reader = (audio_reader_t *)calloc(1, sizeof(audio_reader_t));
    if (!reader) {
        return NULL;
    }

    ma_decoder_config config = ma_decoder_config_init(
        ma_format_s16,
        0,
        0
    );

    ma_result result = ma_decoder_init_memory(data, size, &config, &reader->decoder);
    if (result != MA_SUCCESS) {
        VOICE_LOG_E("Failed to open audio from memory, error: %d", result);
        free(reader);
        return NULL;
    }

    reader->info.format = AUDIO_FILE_FORMAT_UNKNOWN;
    reader->info.sample_rate = reader->decoder.outputSampleRate;
    reader->info.channels = reader->decoder.outputChannels;
    reader->info.bits_per_sample = 16;

    ma_uint64 total_frames;
    if (ma_decoder_get_length_in_pcm_frames(&reader->decoder, &total_frames) == MA_SUCCESS) {
        reader->info.total_frames = total_frames;
        reader->info.duration_seconds =
            (double)total_frames / reader->info.sample_rate;
    }

    reader->initialized = true;

    return reader;
}

/**
 * @brief Close an audio reader and release all associated resources
 * @details This function uninitializes the miniaudio decoder and frees all memory
 *          allocated for the reader. After calling this function, the reader
 *          pointer becomes invalid and should not be used.
 * @param reader Pointer to the audio reader to close (can be NULL)
 */
void audio_reader_close(audio_reader_t *reader)
{
    if (!reader) return;

    if (reader->initialized) {
        ma_decoder_uninit(&reader->decoder);
    }

    free(reader);
}

/**
 * @brief Retrieve metadata information from an audio reader
 * @details This function returns comprehensive information about the audio file,
 *          including format type, sample rate, channel count, bits per sample,
 *          total frame count, and duration. This metadata can be used to
 *          allocate appropriate buffers or display file properties to users.
 * @param reader Pointer to the initialized audio reader
 * @param info Pointer to the structure where file information will be stored
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if reader is not initialized,
 *         VOICE_ERROR_NULL_POINTER if parameters are NULL
 */
voice_error_t audio_reader_get_info(
    audio_reader_t *reader,
    audio_file_info_t *info)
{
    if (!reader || !reader->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!info) {
        return VOICE_ERROR_NULL_POINTER;
    }

    *info = reader->info;
    return VOICE_OK;
}

/**
 * @brief Read audio frames as 16-bit signed integer PCM samples
 * @details This function reads the specified number of frames from the audio reader
 *          and outputs them as 16-bit signed integer samples. Each frame contains
 *          one sample per channel. For stereo audio, each frame contains 2 samples.
 *          The function automatically handles format conversion from the source format
 *          to 16-bit PCM. Reading continues until the requested frames are read or
 *          end-of-file is reached.
 * @param reader Pointer to the initialized audio reader
 * @param output Buffer to store the decoded 16-bit PCM samples
 * @param frames_to_read Number of frames to read from the file
 * @param frames_read Output parameter: actual number of frames successfully read
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if reader is not initialized,
 *         VOICE_ERROR_NULL_POINTER if parameters are NULL,
 *         VOICE_ERROR_FILE_READ on read error
 * @note The output buffer must be large enough to hold (frames_to_read * channels) samples
 */
voice_error_t audio_reader_read_s16(
    audio_reader_t *reader,
    int16_t *output,
    size_t frames_to_read,
    size_t *frames_read)
{
    if (!reader || !reader->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!output || !frames_read) {
        return VOICE_ERROR_NULL_POINTER;
    }

    ma_uint64 read;
    ma_result result = ma_decoder_read_pcm_frames(
        &reader->decoder,
        output,
        frames_to_read,
        &read
    );

    *frames_read = (size_t)read;

    if (result != MA_SUCCESS && result != MA_AT_END) {
        return VOICE_ERROR_FILE_READ;
    }

    if (read == 0 || result == MA_AT_END) {
        reader->is_eof = true;
    }

    return VOICE_OK;
}

/**
 * @brief Read audio frames as 32-bit floating-point samples
 * @details This function reads audio frames and converts them to 32-bit floating-point
 *          format with values in the range [-1.0, 1.0]. It internally reads 16-bit
 *          PCM samples first, then performs conversion to normalized float values.
 *          This format is preferred for audio processing and DSP operations as it
 *          provides better precision and avoids clipping during intermediate calculations.
 * @param reader Pointer to the initialized audio reader
 * @param output Buffer to store the decoded 32-bit float samples
 * @param frames_to_read Number of frames to read from the file
 * @param frames_read Output parameter: actual number of frames successfully read
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if reader is not initialized,
 *         VOICE_ERROR_NULL_POINTER if parameters are NULL,
 *         VOICE_ERROR_NO_MEMORY if temporary buffer allocation fails,
 *         VOICE_ERROR_FILE_READ on read error
 * @note The output buffer must be large enough to hold (frames_to_read * channels) samples.
 *       Sample values are normalized to [-1.0, 1.0] range.
 */
voice_error_t audio_reader_read_f32(
    audio_reader_t *reader,
    float *output,
    size_t frames_to_read,
    size_t *frames_read)
{
    if (!reader || !reader->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!output || !frames_read) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* Read as s16 then convert to float */
    size_t total_samples = frames_to_read * reader->info.channels;
    int16_t *temp = (int16_t *)malloc(total_samples * sizeof(int16_t));
    if (!temp) {
        return VOICE_ERROR_NO_MEMORY;
    }

    voice_error_t err = audio_reader_read_s16(reader, temp, frames_to_read, frames_read);

    if (err == VOICE_OK) {
        size_t samples_read = *frames_read * reader->info.channels;
        for (size_t i = 0; i < samples_read; i++) {
            output[i] = temp[i] / 32768.0f;
        }
    }

    free(temp);
    return err;
}

/**
 * @brief Seek to a specific frame position in the audio file
 * @details This function changes the read position to the specified frame number,
 *          allowing non-sequential reading of audio data. Seeking is useful for
 *          implementing features like audio scrubbing, loop playback, or random access.
 *          After a successful seek, the EOF flag is cleared, and subsequent read
 *          operations will start from the new position.
 * @param reader Pointer to the initialized audio reader
 * @param frame The target frame position (0-based index)
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if reader is not initialized,
 *         VOICE_ERROR_FILE_SEEK if seeking fails
 * @note Not all audio formats support seeking (e.g., some streaming formats).
 *       Frame numbers are 0-based, where 0 is the first frame.
 */
voice_error_t audio_reader_seek(audio_reader_t *reader, uint64_t frame)
{
    if (!reader || !reader->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    ma_result result = ma_decoder_seek_to_pcm_frame(&reader->decoder, frame);
    if (result != MA_SUCCESS) {
        return VOICE_ERROR_FILE_SEEK;
    }

    reader->is_eof = false;
    return VOICE_OK;
}

/**
 * @brief Get the current read position in the audio file
 * @details This function returns the current frame position of the reader's cursor.
 *          The position indicates which frame will be read by the next read operation.
 *          This is useful for tracking playback progress or implementing position displays.
 * @param reader Pointer to the initialized audio reader
 * @return Current frame position (0-based), or 0 if reader is invalid or query fails
 */
uint64_t audio_reader_tell(audio_reader_t *reader)
{
    if (!reader || !reader->initialized) {
        return 0;
    }

    ma_uint64 cursor;
    if (ma_decoder_get_cursor_in_pcm_frames(&reader->decoder, &cursor) != MA_SUCCESS) {
        return 0;
    }

    return cursor;
}

/**
 * @brief Check if the reader has reached the end of the audio file
 * @details This function returns the EOF (End Of File) status of the reader.
 *          The EOF flag is set when a read operation reaches the end of the file
 *          or when no more data is available. It is cleared by successful seek operations.
 * @param reader Pointer to the audio reader
 * @return true if EOF has been reached or reader is invalid, false otherwise
 */
bool audio_reader_is_eof(audio_reader_t *reader)
{
    return reader ? reader->is_eof : true;
}

/* ============================================
 * Writer Implementation
 * ============================================ */

/**
 * @brief Initialize an audio writer configuration with default values
 * @details This function sets up a configuration structure with sensible defaults:
 *          - Format: WAV (uncompressed PCM)
 *          - Sample rate: 48000 Hz
 *          - Channels: 1 (mono)
 *          - Bits per sample: 16
 *          - Bitrate: 128000 bps (for compressed formats)
 *          - Quality: 5 (medium, for compressed formats)
 * @param config Pointer to the configuration structure to initialize
 */
void audio_writer_config_init(audio_writer_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(audio_writer_config_t));
    config->format = AUDIO_FILE_FORMAT_WAV;
    config->sample_rate = 48000;
    config->channels = 1;
    config->bits_per_sample = 16;
    config->bitrate = 128000;
    config->quality = 5;
}

/**
 * @brief Create an audio writer to save audio data to a file
 * @details This function initializes an audio file writer using the specified configuration.
 *          Currently, only WAV format writing is supported. The miniaudio encoder is
 *          configured to write 16-bit PCM samples at the specified sample rate and
 *          channel count. The file is opened for writing, and the WAV header is written.
 *          Subsequent write operations will append audio data to the file.
 * @param path The file path where the audio file will be created
 * @param config Pointer to the writer configuration specifying format, sample rate, etc.
 * @return Pointer to the initialized audio_writer_t structure on success, NULL on failure
 * @note Currently only WAV format is supported. The caller must call audio_writer_close()
 *       when finished to properly finalize the file and free resources.
 */
audio_writer_t *audio_writer_create(
    const char *path,
    const audio_writer_config_t *config)
{
    if (!path || !config) {
        return NULL;
    }

    audio_writer_t *writer = (audio_writer_t *)calloc(1, sizeof(audio_writer_t));
    if (!writer) {
        return NULL;
    }

    writer->config = *config;

    /* Currently only WAV format writing is supported */
    if (config->format != AUDIO_FILE_FORMAT_WAV) {
        VOICE_LOG_E("Only WAV format is supported for writing");
        free(writer);
        return NULL;
    }

    ma_encoder_config enc_config = ma_encoder_config_init(
        ma_encoding_format_wav,
        ma_format_s16,
        config->channels,
        config->sample_rate
    );

    ma_result result = ma_encoder_init_file(path, &enc_config, &writer->encoder);
    if (result != MA_SUCCESS) {
        VOICE_LOG_E("Failed to create audio file: %s, error: %d", path, result);
        free(writer);
        return NULL;
    }

    writer->initialized = true;

    VOICE_LOG_I("Audio file created: %s, %uHz, %uch",
        path, config->sample_rate, config->channels);

    return writer;
}

/**
 * @brief Close an audio writer and finalize the output file
 * @details This function properly finalizes the audio file by updating headers
 *          (e.g., file size in WAV files), uninitializes the encoder, and releases
 *          all allocated resources. It is essential to call this function to ensure
 *          the output file is valid and all data is flushed to disk.
 * @param writer Pointer to the audio writer to close
 * @return VOICE_OK on success, VOICE_ERROR_NULL_POINTER if writer is NULL
 */
voice_error_t audio_writer_close(audio_writer_t *writer)
{
    if (!writer) {
        return VOICE_ERROR_NULL_POINTER;
    }

    if (writer->initialized) {
        ma_encoder_uninit(&writer->encoder);
    }

    free(writer);
    return VOICE_OK;
}

/**
 * @brief Write audio frames in 16-bit signed integer PCM format
 * @details This function writes the specified number of frames to the audio file.
 *          The input should contain 16-bit signed integer samples. Each frame
 *          consists of one sample per channel. The data is written directly to
 *          the encoder, and the frame count is updated internally.
 * @param writer Pointer to the initialized audio writer
 * @param input Buffer containing the 16-bit PCM samples to write
 * @param frames Number of frames to write
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if writer is not initialized,
 *         VOICE_ERROR_NULL_POINTER if input is NULL,
 *         VOICE_ERROR_FILE_WRITE on write error
 * @note The input buffer must contain at least (frames * channels) samples
 */
voice_error_t audio_writer_write_s16(
    audio_writer_t *writer,
    const int16_t *input,
    size_t frames)
{
    if (!writer || !writer->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!input) {
        return VOICE_ERROR_NULL_POINTER;
    }

    ma_uint64 written;
    ma_result result = ma_encoder_write_pcm_frames(
        &writer->encoder,
        input,
        frames,
        &written
    );

    if (result != MA_SUCCESS) {
        return VOICE_ERROR_FILE_WRITE;
    }

    writer->frames_written += written;
    return VOICE_OK;
}

/**
 * @brief Write audio frames in 32-bit floating-point format
 * @details This function converts 32-bit floating-point samples to 16-bit PCM
 *          and writes them to the audio file. Input samples are expected to be
 *          in the range [-1.0, 1.0] and are clamped to this range before conversion.
 *          Values outside this range are clipped to prevent overflow. The conversion
 *          multiplies by 32767 to map float values to the 16-bit integer range.
 * @param writer Pointer to the initialized audio writer
 * @param input Buffer containing the 32-bit float samples to write (range: [-1.0, 1.0])
 * @param frames Number of frames to write
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if writer is not initialized,
 *         VOICE_ERROR_NULL_POINTER if input is NULL,
 *         VOICE_ERROR_NO_MEMORY if temporary buffer allocation fails,
 *         VOICE_ERROR_FILE_WRITE on write error
 * @note The input buffer must contain at least (frames * channels) samples.
 *       Float samples outside [-1.0, 1.0] will be clipped.
 */
voice_error_t audio_writer_write_f32(
    audio_writer_t *writer,
    const float *input,
    size_t frames)
{
    if (!writer || !writer->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!input) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* Convert float samples to 16-bit integer */
    size_t total_samples = frames * writer->config.channels;
    int16_t *temp = (int16_t *)malloc(total_samples * sizeof(int16_t));
    if (!temp) {
        return VOICE_ERROR_NO_MEMORY;
    }

    for (size_t i = 0; i < total_samples; i++) {
        float sample = input[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        temp[i] = (int16_t)(sample * 32767.0f);
    }

    voice_error_t err = audio_writer_write_s16(writer, temp, frames);

    free(temp);
    return err;
}

/**
 * @brief Get the total number of frames written to the audio file
 * @details This function returns the cumulative count of frames that have been
 *          successfully written to the file since the writer was created.
 *          This can be used to track recording progress or calculate file duration.
 * @param writer Pointer to the audio writer
 * @return Total number of frames written, or 0 if writer is invalid
 */
uint64_t audio_writer_get_frames_written(audio_writer_t *writer)
{
    return writer ? writer->frames_written : 0;
}

/* ============================================
 * Compatibility API Implementation
 * ============================================ */

/**
 * @brief Create an audio writer with simplified parameters
 * @details This is a convenience function that creates an audio writer with
 *          minimal configuration. It automatically initializes a default configuration
 *          and sets the sample rate and channel count. The audio format is inferred
 *          from the file extension, defaulting to WAV if unknown. The bit depth is
 *          fixed at 16 bits per sample.
 * @param path The file path for the output audio file
 * @param sample_rate Sample rate in Hz (e.g., 44100, 48000)
 * @param channels Number of audio channels (1 for mono, 2 for stereo)
 * @return Pointer to the initialized audio_writer_t structure on success, NULL on failure
 * @note This is equivalent to calling audio_writer_config_init(), setting parameters,
 *       and calling audio_writer_create().
 */
audio_writer_t *audio_writer_create_simple(
    const char *path,
    uint32_t sample_rate,
    uint8_t channels)
{
    audio_writer_config_t config;
    audio_writer_config_init(&config);

    config.sample_rate = sample_rate;
    config.channels = channels;
    config.bits_per_sample = 16;
    config.format = audio_file_format_from_path(path);

    if (config.format == AUDIO_FILE_FORMAT_UNKNOWN) {
        config.format = AUDIO_FILE_FORMAT_WAV;  /* Default to WAV format */
    }

    return audio_writer_create(path, &config);
}
