/**
 * @file file_io.c
 * @brief Audio file I/O implementation using miniaudio
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/file_io.h"
#include <stdlib.h>
#include <string.h>

/* miniaudio 解码器配置 - 使用 third_party 的实现 */
#define MA_NO_DEVICE_IO
#include "miniaudio.h"

/* ============================================
 * 读取器结构
 * ============================================ */

struct audio_reader_s {
    ma_decoder decoder;
    audio_file_info_t info;
    bool initialized;
    bool is_eof;
};

/* ============================================
 * 写入器结构
 * ============================================ */

struct audio_writer_s {
    ma_encoder encoder;
    audio_writer_config_t config;
    uint64_t frames_written;
    bool initialized;
};

/* ============================================
 * 格式工具函数
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

    ext++;  /* 跳过 '.' */

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
 * 读取器实现
 * ============================================ */

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
        ma_format_s16,  /* 默认输出s16 */
        0,              /* 使用源通道数 */
        0               /* 使用源采样率 */
    );

    ma_result result = ma_decoder_init_file(path, &config, &reader->decoder);
    if (result != MA_SUCCESS) {
        VOICE_LOG_E("Failed to open audio file: %s, error: %d", path, result);
        free(reader);
        return NULL;
    }

    /* 获取文件信息 */
    reader->info.format = audio_file_format_from_path(path);
    reader->info.sample_rate = reader->decoder.outputSampleRate;
    reader->info.channels = reader->decoder.outputChannels;
    reader->info.bits_per_sample = 16;  /* miniaudio 默认输出 */

    /* 获取总帧数 */
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

void audio_reader_close(audio_reader_t *reader)
{
    if (!reader) return;

    if (reader->initialized) {
        ma_decoder_uninit(&reader->decoder);
    }

    free(reader);
}

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

    /* 读取s16然后转换 */
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

bool audio_reader_is_eof(audio_reader_t *reader)
{
    return reader ? reader->is_eof : true;
}

/* ============================================
 * 写入器实现
 * ============================================ */

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

    /* 目前只支持 WAV 写入 */
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

    /* 转换为 s16 */
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

uint64_t audio_writer_get_frames_written(audio_writer_t *writer)
{
    return writer ? writer->frames_written : 0;
}

/* ============================================
 * 兼容性 API 实现
 * ============================================ */

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
        config.format = AUDIO_FILE_FORMAT_WAV;  /* 默认 WAV */
    }

    return audio_writer_create(path, &config);
}
