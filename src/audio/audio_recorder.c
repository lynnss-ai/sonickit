/**
 * @file audio_recorder.c
 * @brief Audio recording and playback implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/audio_recorder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_recorder_s {
    voice_recorder_config_t config;
    voice_recorder_status_t status;

    /* 内存录制 */
    int16_t *memory_buffer;
    size_t memory_capacity;
    size_t memory_used;
    size_t write_pos;

    /* 文件录制 */
    FILE *file;
    size_t wav_data_pos;  /* WAV data chunk position for finalization */
};

struct voice_player_s {
    voice_player_config_t config;
    voice_player_status_t status;

    /* 播放源数据 */
    int16_t *samples;
    size_t num_samples;
    size_t current_pos;
    uint32_t sample_rate;
    uint8_t channels;

    bool owns_samples;  /* 是否需要释放 samples */
};

/* ============================================
 * WAV 文件辅助
 * ============================================ */

static void write_wav_header(FILE *file, uint32_t sample_rate,
                             uint8_t channels, uint8_t bits_per_sample) {
    /* RIFF header */
    fwrite("RIFF", 1, 4, file);
    uint32_t file_size = 0;  /* Will be updated at end */
    fwrite(&file_size, 4, 1, file);
    fwrite("WAVE", 1, 4, file);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, file);
    uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, file);
    uint16_t audio_format = 1;  /* PCM */
    fwrite(&audio_format, 2, 1, file);
    uint16_t num_channels = channels;
    fwrite(&num_channels, 2, 1, file);
    fwrite(&sample_rate, 4, 1, file);
    uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
    fwrite(&byte_rate, 4, 1, file);
    uint16_t block_align = channels * bits_per_sample / 8;
    fwrite(&block_align, 2, 1, file);
    uint16_t bps = bits_per_sample;
    fwrite(&bps, 2, 1, file);

    /* data chunk */
    fwrite("data", 1, 4, file);
    uint32_t data_size = 0;  /* Will be updated at end */
    fwrite(&data_size, 4, 1, file);
}

static void finalize_wav_file(FILE *file, size_t data_size) {
    /* Update file size at offset 4 */
    uint32_t file_size = (uint32_t)(data_size + 36);
    fseek(file, 4, SEEK_SET);
    fwrite(&file_size, 4, 1, file);

    /* Update data size at offset 40 */
    uint32_t ds = (uint32_t)data_size;
    fseek(file, 40, SEEK_SET);
    fwrite(&ds, 4, 1, file);
}

/* ============================================
 * Recorder Implementation
 * ============================================ */

void voice_recorder_config_init(voice_recorder_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->format = VOICE_RECORD_WAV;
    config->source = VOICE_RECORD_SOURCE_INPUT;
    config->sample_rate = 48000;
    config->channels = 1;
    config->bits_per_sample = 16;
    config->filename = NULL;
    config->append = false;
    config->max_memory_bytes = 0;
    config->circular_buffer = false;
    config->max_duration_ms = 0;
    config->max_file_size = 0;
    config->on_data = NULL;
    config->on_complete = NULL;
    config->callback_user_data = NULL;
}

voice_recorder_t *voice_recorder_create(const voice_recorder_config_t *config) {
    if (!config) return NULL;

    voice_recorder_t *recorder = (voice_recorder_t *)calloc(1, sizeof(voice_recorder_t));
    if (!recorder) return NULL;

    recorder->config = *config;
    recorder->status.is_recording = false;
    recorder->status.duration_ms = 0;
    recorder->status.samples_recorded = 0;
    recorder->status.bytes_written = 0;
    recorder->status.peak_level_db = -96.0f;
    recorder->status.avg_level_db = -96.0f;

    /* 根据格式初始化 */
    if (config->format == VOICE_RECORD_MEMORY) {
        size_t initial_size = config->max_memory_bytes > 0 ?
            config->max_memory_bytes : (1024 * 1024);  /* 1MB default */
        recorder->memory_buffer = (int16_t *)malloc(initial_size);
        if (!recorder->memory_buffer) {
            free(recorder);
            return NULL;
        }
        recorder->memory_capacity = initial_size / sizeof(int16_t);
        recorder->memory_used = 0;
        recorder->write_pos = 0;
    }

    return recorder;
}

void voice_recorder_destroy(voice_recorder_t *recorder) {
    if (!recorder) return;

    if (recorder->status.is_recording) {
        voice_recorder_stop(recorder);
    }

    if (recorder->memory_buffer) {
        free(recorder->memory_buffer);
    }

    if (recorder->file) {
        fclose(recorder->file);
    }

    free(recorder);
}

voice_error_t voice_recorder_start(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_NULL_POINTER;

    if (recorder->status.is_recording) {
        return VOICE_ERROR_INVALID_STATE;
    }

    /* 重置状态 */
    recorder->status.duration_ms = 0;
    recorder->status.samples_recorded = 0;
    recorder->status.bytes_written = 0;
    recorder->status.peak_level_db = -96.0f;
    recorder->status.avg_level_db = -96.0f;
    recorder->memory_used = 0;
    recorder->write_pos = 0;

    /* 打开文件 (如果需要) */
    if (recorder->config.format == VOICE_RECORD_WAV && recorder->config.filename) {
        const char *mode = recorder->config.append ? "ab" : "wb";
        recorder->file = fopen(recorder->config.filename, mode);
        if (!recorder->file) {
            return VOICE_ERROR_INVALID_PARAM;
        }

        if (!recorder->config.append) {
            write_wav_header(recorder->file, recorder->config.sample_rate,
                           recorder->config.channels, recorder->config.bits_per_sample);
        }
        recorder->wav_data_pos = ftell(recorder->file);
    } else if (recorder->config.format == VOICE_RECORD_RAW && recorder->config.filename) {
        const char *mode = recorder->config.append ? "ab" : "wb";
        recorder->file = fopen(recorder->config.filename, mode);
        if (!recorder->file) {
            return VOICE_ERROR_INVALID_PARAM;
        }
    }

    recorder->status.is_recording = true;
    return VOICE_OK;
}

voice_error_t voice_recorder_stop(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_NULL_POINTER;

    if (!recorder->status.is_recording) {
        return VOICE_OK;  /* 已经停止 */
    }

    recorder->status.is_recording = false;

    /* 关闭文件 (如果打开) */
    if (recorder->file) {
        if (recorder->config.format == VOICE_RECORD_WAV) {
            finalize_wav_file(recorder->file, recorder->status.bytes_written);
        }
        fclose(recorder->file);
        recorder->file = NULL;

        /* 触发完成回调 */
        if (recorder->config.on_complete) {
            recorder->config.on_complete(recorder->config.filename,
                                         recorder->status.duration_ms,
                                         recorder->config.callback_user_data);
        }
    }

    return VOICE_OK;
}

voice_error_t voice_recorder_pause(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_NULL_POINTER;

    if (!recorder->status.is_recording) {
        return VOICE_ERROR_INVALID_STATE;
    }

    recorder->status.is_recording = false;
    return VOICE_OK;
}

voice_error_t voice_recorder_resume(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_NULL_POINTER;

    recorder->status.is_recording = true;
    return VOICE_OK;
}

voice_error_t voice_recorder_write(
    voice_recorder_t *recorder,
    const int16_t *samples,
    size_t num_samples
) {
    if (!recorder || !samples) return VOICE_ERROR_NULL_POINTER;

    if (!recorder->status.is_recording) {
        return VOICE_ERROR_INVALID_STATE;
    }

    /* 检查时长限制 */
    if (recorder->config.max_duration_ms > 0) {
        uint64_t new_duration = (recorder->status.samples_recorded + num_samples) * 1000
                               / recorder->config.sample_rate;
        if (new_duration > recorder->config.max_duration_ms) {
            voice_recorder_stop(recorder);
            return VOICE_ERROR_INVALID_STATE;
        }
    }

    /* 计算电平 */
    float peak = 0.0f;
    float sum = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        float level = (float)abs(samples[i]) / 32768.0f;
        if (level > peak) peak = level;
        sum += level;
    }
    float peak_db = peak > 0 ? 20.0f * log10f(peak) : -96.0f;
    float avg_db = sum > 0 ? 20.0f * log10f(sum / num_samples) : -96.0f;

    if (peak_db > recorder->status.peak_level_db) {
        recorder->status.peak_level_db = peak_db;
    }
    recorder->status.avg_level_db = avg_db;

    /* 触发数据回调 */
    if (recorder->config.on_data) {
        recorder->config.on_data(samples, num_samples, recorder->config.callback_user_data);
    }

    size_t bytes_to_write = num_samples * sizeof(int16_t);

    /* 写入文件或内存 */
    if (recorder->file) {
        size_t written = fwrite(samples, sizeof(int16_t), num_samples, recorder->file);
        if (written != num_samples) {
            return VOICE_ERROR_INVALID_PARAM;
        }
        recorder->status.bytes_written += written * sizeof(int16_t);
    } else if (recorder->memory_buffer) {
        if (recorder->config.circular_buffer) {
            /* 循环缓冲区模式 */
            for (size_t i = 0; i < num_samples; i++) {
                recorder->memory_buffer[recorder->write_pos] = samples[i];
                recorder->write_pos = (recorder->write_pos + 1) % recorder->memory_capacity;
            }
            if (recorder->memory_used < recorder->memory_capacity) {
                recorder->memory_used += num_samples;
                if (recorder->memory_used > recorder->memory_capacity) {
                    recorder->memory_used = recorder->memory_capacity;
                }
            }
        } else {
            /* 线性缓冲区模式 */
            if (recorder->memory_used + num_samples > recorder->memory_capacity) {
                return VOICE_ERROR_OUT_OF_MEMORY;
            }
            memcpy(&recorder->memory_buffer[recorder->memory_used],
                   samples, num_samples * sizeof(int16_t));
            recorder->memory_used += num_samples;
        }
        recorder->status.bytes_written += bytes_to_write;
    }

    recorder->status.samples_recorded += num_samples;
    recorder->status.duration_ms = recorder->status.samples_recorded * 1000
                                  / recorder->config.sample_rate;

    return VOICE_OK;
}

voice_error_t voice_recorder_get_status(
    voice_recorder_t *recorder,
    voice_recorder_status_t *status
) {
    if (!recorder || !status) return VOICE_ERROR_NULL_POINTER;

    *status = recorder->status;
    return VOICE_OK;
}

voice_error_t voice_recorder_get_data(
    voice_recorder_t *recorder,
    int16_t **samples,
    size_t *num_samples
) {
    if (!recorder || !samples || !num_samples) return VOICE_ERROR_NULL_POINTER;

    if (recorder->config.format != VOICE_RECORD_MEMORY || !recorder->memory_buffer) {
        return VOICE_ERROR_INVALID_STATE;
    }

    *samples = recorder->memory_buffer;
    *num_samples = recorder->memory_used;

    return VOICE_OK;
}

voice_error_t voice_recorder_save_to_file(
    voice_recorder_t *recorder,
    const char *filename,
    voice_record_format_t format
) {
    if (!recorder || !filename) return VOICE_ERROR_NULL_POINTER;

    if (recorder->config.format != VOICE_RECORD_MEMORY || !recorder->memory_buffer) {
        return VOICE_ERROR_INVALID_STATE;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    if (format == VOICE_RECORD_WAV) {
        write_wav_header(file, recorder->config.sample_rate,
                        recorder->config.channels, recorder->config.bits_per_sample);
    }

    size_t written = fwrite(recorder->memory_buffer, sizeof(int16_t),
                           recorder->memory_used, file);

    if (format == VOICE_RECORD_WAV) {
        finalize_wav_file(file, written * sizeof(int16_t));
    }

    fclose(file);

    return (written == recorder->memory_used) ? VOICE_OK : VOICE_ERROR_INVALID_PARAM;
}

/* ============================================
 * Player Implementation
 * ============================================ */

void voice_player_config_init(voice_player_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->sample_rate = 0;  /* Use file sample rate */
    config->playback_speed = 1.0f;
    config->volume = 1.0f;
    config->loop = false;
    config->on_complete = NULL;
    config->on_position = NULL;
    config->callback_user_data = NULL;
}

voice_player_t *voice_player_create_from_file(
    const char *filename,
    const voice_player_config_t *config
) {
    if (!filename) return NULL;

    FILE *file = fopen(filename, "rb");
    if (!file) return NULL;

    /* 读取 WAV 头 */
    char riff[4];
    if (fread(riff, 1, 4, file) != 4 || memcmp(riff, "RIFF", 4) != 0) {
        fclose(file);
        return NULL;
    }

    fseek(file, 8, SEEK_SET);
    char wave[4];
    if (fread(wave, 1, 4, file) != 4 || memcmp(wave, "WAVE", 4) != 0) {
        fclose(file);
        return NULL;
    }

    /* 查找 fmt chunk */
    fseek(file, 12, SEEK_SET);
    uint16_t channels = 1;
    uint32_t sample_rate = 48000;
    uint16_t bits_per_sample = 16;
    uint32_t data_size = 0;

    while (!feof(file)) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, file) != 4) break;
        if (fread(&chunk_size, 4, 1, file) != 1) break;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            uint16_t format;
            if (fread(&format, 2, 1, file) != 1) break;
            if (fread(&channels, 2, 1, file) != 1) break;
            if (fread(&sample_rate, 4, 1, file) != 1) break;
            fseek(file, 6, SEEK_CUR);  /* Skip byte rate and block align */
            if (fread(&bits_per_sample, 2, 1, file) != 1) break;
            if (chunk_size > 16) {
                fseek(file, chunk_size - 16, SEEK_CUR);
            }
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            break;
        } else {
            fseek(file, chunk_size, SEEK_CUR);
        }
    }

    if (data_size == 0) {
        fclose(file);
        return NULL;
    }

    /* 读取音频数据 */
    size_t num_samples = data_size / (bits_per_sample / 8);
    int16_t *samples = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (!samples) {
        fclose(file);
        return NULL;
    }

    if (bits_per_sample == 16) {
        size_t read_count = fread(samples, sizeof(int16_t), num_samples, file);
        (void)read_count;  /* Suppress unused warning */
    } else {
        /* 8-bit to 16-bit conversion */
        uint8_t *temp = (uint8_t *)malloc(data_size);
        if (temp) {
            size_t read_count = fread(temp, 1, data_size, file);
            (void)read_count;
            for (size_t i = 0; i < data_size; i++) {
                samples[i] = (int16_t)((temp[i] - 128) * 256);
            }
            free(temp);
        }
    }

    fclose(file);

    /* 创建播放器 */
    voice_player_t *player = (voice_player_t *)calloc(1, sizeof(voice_player_t));
    if (!player) {
        free(samples);
        return NULL;
    }

    if (config) {
        player->config = *config;
    } else {
        voice_player_config_init(&player->config);
    }

    player->samples = samples;
    player->num_samples = num_samples / channels;  /* Per channel */
    player->current_pos = 0;
    player->sample_rate = sample_rate;
    player->channels = (uint8_t)channels;
    player->owns_samples = true;

    player->status.is_playing = false;
    player->status.is_paused = false;
    player->status.position_ms = 0;
    player->status.duration_ms = (player->num_samples * 1000) / sample_rate;
    player->status.volume = player->config.volume;
    player->status.playback_speed = player->config.playback_speed;

    return player;
}

voice_player_t *voice_player_create_from_memory(
    const int16_t *samples,
    size_t num_samples,
    uint32_t sample_rate,
    const voice_player_config_t *config
) {
    if (!samples || num_samples == 0) return NULL;

    voice_player_t *player = (voice_player_t *)calloc(1, sizeof(voice_player_t));
    if (!player) return NULL;

    if (config) {
        player->config = *config;
    } else {
        voice_player_config_init(&player->config);
    }

    /* 复制样本数据 */
    player->samples = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (!player->samples) {
        free(player);
        return NULL;
    }
    memcpy(player->samples, samples, num_samples * sizeof(int16_t));

    player->num_samples = num_samples;
    player->current_pos = 0;
    player->sample_rate = sample_rate;
    player->channels = 1;
    player->owns_samples = true;

    player->status.is_playing = false;
    player->status.is_paused = false;
    player->status.position_ms = 0;
    player->status.duration_ms = (num_samples * 1000) / sample_rate;
    player->status.volume = player->config.volume;
    player->status.playback_speed = player->config.playback_speed;

    return player;
}

void voice_player_destroy(voice_player_t *player) {
    if (!player) return;

    if (player->owns_samples && player->samples) {
        free(player->samples);
    }

    free(player);
}

voice_error_t voice_player_play(voice_player_t *player) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    player->status.is_playing = true;
    player->status.is_paused = false;

    return VOICE_OK;
}

voice_error_t voice_player_stop(voice_player_t *player) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    player->status.is_playing = false;
    player->status.is_paused = false;
    player->current_pos = 0;
    player->status.position_ms = 0;

    return VOICE_OK;
}

voice_error_t voice_player_pause(voice_player_t *player) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    if (player->status.is_playing) {
        player->status.is_paused = true;
        player->status.is_playing = false;
    }

    return VOICE_OK;
}

voice_error_t voice_player_resume(voice_player_t *player) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    if (player->status.is_paused) {
        player->status.is_paused = false;
        player->status.is_playing = true;
    }

    return VOICE_OK;
}

voice_error_t voice_player_seek(voice_player_t *player, uint64_t position_ms) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    size_t sample_pos = (size_t)((position_ms * player->sample_rate) / 1000);
    if (sample_pos > player->num_samples) {
        sample_pos = player->num_samples;
    }

    player->current_pos = sample_pos;
    player->status.position_ms = position_ms;

    return VOICE_OK;
}

voice_error_t voice_player_set_volume(voice_player_t *player, float volume) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    player->config.volume = volume;
    player->status.volume = volume;

    return VOICE_OK;
}

voice_error_t voice_player_set_speed(voice_player_t *player, float speed) {
    if (!player) return VOICE_ERROR_NULL_POINTER;

    if (speed < 0.25f) speed = 0.25f;
    if (speed > 4.0f) speed = 4.0f;

    player->config.playback_speed = speed;
    player->status.playback_speed = speed;

    return VOICE_OK;
}

size_t voice_player_read(
    voice_player_t *player,
    int16_t *samples,
    size_t num_samples
) {
    if (!player || !samples) return 0;

    if (!player->status.is_playing) {
        return 0;
    }

    size_t remaining = player->num_samples - player->current_pos;
    size_t to_read = (num_samples < remaining) ? num_samples : remaining;

    if (to_read == 0) {
        if (player->config.loop) {
            player->current_pos = 0;
            remaining = player->num_samples;
            to_read = (num_samples < remaining) ? num_samples : remaining;
        } else {
            player->status.is_playing = false;
            if (player->config.on_complete) {
                player->config.on_complete(player->config.callback_user_data);
            }
            return 0;
        }
    }

    /* 应用音量 */
    float volume = player->config.volume;
    for (size_t i = 0; i < to_read; i++) {
        float sample = (float)player->samples[player->current_pos + i] * volume;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        samples[i] = (int16_t)sample;
    }

    player->current_pos += to_read;
    player->status.position_ms = (player->current_pos * 1000) / player->sample_rate;

    /* 位置回调 */
    if (player->config.on_position) {
        player->config.on_position(player->status.position_ms,
                                  player->status.duration_ms,
                                  player->config.callback_user_data);
    }

    return to_read;
}

voice_error_t voice_player_get_status(
    voice_player_t *player,
    voice_player_status_t *status
) {
    if (!player || !status) return VOICE_ERROR_NULL_POINTER;

    *status = player->status;
    return VOICE_OK;
}
