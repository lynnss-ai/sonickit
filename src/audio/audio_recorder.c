/**
 * @file audio_recorder.c
 * @brief Audio recording and playback implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/audio_recorder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================
 * WAV 文件头结构
 * ============================================ */

#pragma pack(push, 1)
typedef struct {
    char riff[4];           /* "RIFF" */
    uint32_t file_size;     /* 文件大小 - 8 */
    char wave[4];           /* "WAVE" */
    char fmt[4];            /* "fmt " */
    uint32_t fmt_size;      /* 16 for PCM */
    uint16_t audio_format;  /* 1 for PCM */
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];           /* "data" */
    uint32_t data_size;
} wav_header_t;
#pragma pack(pop)

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_recorder_s {
    voice_recorder_config_t config;
    voice_recorder_status_t status;
    
    /* 文件处理 */
    FILE *file;
    size_t data_written;
    size_t file_offset;
    
    /* 内存缓冲区 */
    int16_t *memory_buffer;
    size_t memory_capacity;
    size_t memory_used;
    size_t memory_write_pos;
    
    /* 时间戳 */
    uint64_t start_time;
};

struct voice_player_s {
    voice_player_config_t config;
    voice_player_status_t status;
    
    /* 文件处理 */
    FILE *file;
    size_t data_offset;
    size_t data_size;
    
    /* 内存缓冲区 */
    const int16_t *memory_buffer;
    size_t memory_size;
    
    /* 播放状态 */
    size_t position_samples;
    size_t total_samples;
    
    /* 音频参数 */
    uint32_t sample_rate;
    uint32_t channels;
};

/* ============================================
 * 时间函数
 * ============================================ */

#ifdef _WIN32
#include <windows.h>
static uint64_t get_time_ms(void) {
    return GetTickCount64();
}
#else
#include <time.h>
static uint64_t get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

/* ============================================
 * 录音器配置
 * ============================================ */

void voice_recorder_config_init(voice_recorder_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_recorder_config_t));
    
    config->format = VOICE_RECORD_WAV;
    config->source = VOICE_RECORD_INPUT;
    
    config->sample_rate = 16000;
    config->channels = 1;
    config->bits_per_sample = 16;
    
    config->max_duration_ms = 0;    /* 无限制 */
    config->max_file_size = 0;      /* 无限制 */
    
    config->circular_buffer = false;
    config->buffer_size = 0;
}

/* ============================================
 * 录音器实现
 * ============================================ */

voice_recorder_t *voice_recorder_create(const voice_recorder_config_t *config) {
    voice_recorder_t *rec = (voice_recorder_t *)calloc(1, sizeof(voice_recorder_t));
    if (!rec) return NULL;
    
    if (config) {
        rec->config = *config;
    } else {
        voice_recorder_config_init(&rec->config);
    }
    
    rec->status.state = VOICE_RECORD_IDLE;
    
    return rec;
}

void voice_recorder_destroy(voice_recorder_t *recorder) {
    if (!recorder) return;
    
    voice_recorder_stop(recorder);
    
    free(recorder->memory_buffer);
    free(recorder);
}

voice_error_t voice_recorder_start(
    voice_recorder_t *recorder,
    const char *filename
) {
    if (!recorder) return VOICE_ERROR_INVALID_PARAM;
    
    if (recorder->status.state == VOICE_RECORD_RECORDING) {
        return VOICE_ERROR_ALREADY_RUNNING;
    }
    
    /* 关闭之前的文件 */
    if (recorder->file) {
        fclose(recorder->file);
        recorder->file = NULL;
    }
    
    /* 重置状态 */
    recorder->data_written = 0;
    recorder->status.samples_recorded = 0;
    recorder->status.bytes_written = 0;
    recorder->status.duration_ms = 0;
    recorder->status.peak_level = 0.0f;
    
    if (filename) {
        /* 录制到文件 */
        recorder->file = fopen(filename, "wb");
        if (!recorder->file) {
            recorder->status.state = VOICE_RECORD_ERROR;
            return VOICE_ERROR_IO;
        }
        
        strncpy(recorder->status.filename, filename, 
                sizeof(recorder->status.filename) - 1);
        
        /* 写入 WAV 头 (占位) */
        if (recorder->config.format == VOICE_RECORD_WAV) {
            wav_header_t header = {0};
            memcpy(header.riff, "RIFF", 4);
            memcpy(header.wave, "WAVE", 4);
            memcpy(header.fmt, "fmt ", 4);
            memcpy(header.data, "data", 4);
            
            header.fmt_size = 16;
            header.audio_format = 1;    /* PCM */
            header.channels = (uint16_t)recorder->config.channels;
            header.sample_rate = recorder->config.sample_rate;
            header.bits_per_sample = (uint16_t)recorder->config.bits_per_sample;
            header.block_align = header.channels * header.bits_per_sample / 8;
            header.byte_rate = header.sample_rate * header.block_align;
            
            fwrite(&header, sizeof(header), 1, recorder->file);
            recorder->file_offset = sizeof(header);
        }
    } else if (recorder->config.buffer_size > 0) {
        /* 录制到内存 */
        recorder->memory_capacity = recorder->config.buffer_size;
        recorder->memory_buffer = (int16_t *)malloc(
            recorder->memory_capacity * sizeof(int16_t));
        if (!recorder->memory_buffer) {
            recorder->status.state = VOICE_RECORD_ERROR;
            return VOICE_ERROR_NO_MEMORY;
        }
        recorder->memory_used = 0;
        recorder->memory_write_pos = 0;
    } else {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    recorder->start_time = get_time_ms();
    recorder->status.state = VOICE_RECORD_RECORDING;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_recorder_stop(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_INVALID_PARAM;
    
    if (recorder->status.state != VOICE_RECORD_RECORDING &&
        recorder->status.state != VOICE_RECORD_PAUSED) {
        return VOICE_SUCCESS;
    }
    
    /* 更新 WAV 头 */
    if (recorder->file && recorder->config.format == VOICE_RECORD_WAV) {
        fseek(recorder->file, 4, SEEK_SET);
        uint32_t file_size = (uint32_t)(recorder->data_written + sizeof(wav_header_t) - 8);
        fwrite(&file_size, 4, 1, recorder->file);
        
        fseek(recorder->file, 40, SEEK_SET);
        uint32_t data_size = (uint32_t)recorder->data_written;
        fwrite(&data_size, 4, 1, recorder->file);
        
        fclose(recorder->file);
        recorder->file = NULL;
    }
    
    recorder->status.state = VOICE_RECORD_STOPPED;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_recorder_pause(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_INVALID_PARAM;
    
    if (recorder->status.state != VOICE_RECORD_RECORDING) {
        return VOICE_ERROR_INVALID_STATE;
    }
    
    recorder->status.state = VOICE_RECORD_PAUSED;
    return VOICE_SUCCESS;
}

voice_error_t voice_recorder_resume(voice_recorder_t *recorder) {
    if (!recorder) return VOICE_ERROR_INVALID_PARAM;
    
    if (recorder->status.state != VOICE_RECORD_PAUSED) {
        return VOICE_ERROR_INVALID_STATE;
    }
    
    recorder->status.state = VOICE_RECORD_RECORDING;
    return VOICE_SUCCESS;
}

voice_error_t voice_recorder_write(
    voice_recorder_t *recorder,
    const int16_t *samples,
    size_t num_samples
) {
    if (!recorder || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    if (recorder->status.state != VOICE_RECORD_RECORDING) {
        return VOICE_ERROR_INVALID_STATE;
    }
    
    /* 计算峰值电平 */
    for (size_t i = 0; i < num_samples; i++) {
        float level = (float)abs(samples[i]) / 32768.0f;
        if (level > recorder->status.peak_level) {
            recorder->status.peak_level = level;
        }
    }
    
    size_t bytes = num_samples * sizeof(int16_t);
    
    if (recorder->file) {
        /* 写入文件 */
        size_t written = fwrite(samples, 1, bytes, recorder->file);
        if (written != bytes) {
            recorder->status.state = VOICE_RECORD_ERROR;
            return VOICE_ERROR_IO;
        }
        recorder->data_written += written;
        recorder->status.bytes_written += written;
    } else if (recorder->memory_buffer) {
        /* 写入内存 */
        if (recorder->config.circular_buffer) {
            /* 环形缓冲区 */
            for (size_t i = 0; i < num_samples; i++) {
                recorder->memory_buffer[recorder->memory_write_pos++] = samples[i];
                if (recorder->memory_write_pos >= recorder->memory_capacity) {
                    recorder->memory_write_pos = 0;
                }
            }
            if (recorder->memory_used < recorder->memory_capacity) {
                recorder->memory_used += num_samples;
                if (recorder->memory_used > recorder->memory_capacity) {
                    recorder->memory_used = recorder->memory_capacity;
                }
            }
        } else {
            /* 线性缓冲区 */
            size_t remaining = recorder->memory_capacity - recorder->memory_used;
            if (num_samples > remaining) num_samples = remaining;
            
            memcpy(recorder->memory_buffer + recorder->memory_used,
                   samples, num_samples * sizeof(int16_t));
            recorder->memory_used += num_samples;
        }
        recorder->status.bytes_written = recorder->memory_used * sizeof(int16_t);
    }
    
    recorder->status.samples_recorded += num_samples;
    recorder->status.duration_ms = (uint32_t)(
        (uint64_t)recorder->status.samples_recorded * 1000 / 
        recorder->config.sample_rate);
    
    /* 检查限制 */
    if (recorder->config.max_duration_ms > 0 &&
        recorder->status.duration_ms >= recorder->config.max_duration_ms) {
        voice_recorder_stop(recorder);
    }
    
    if (recorder->config.max_file_size > 0 &&
        recorder->status.bytes_written >= recorder->config.max_file_size) {
        voice_recorder_stop(recorder);
    }
    
    return VOICE_SUCCESS;
}

voice_error_t voice_recorder_get_status(
    voice_recorder_t *recorder,
    voice_recorder_status_t *status
) {
    if (!recorder || !status) return VOICE_ERROR_INVALID_PARAM;
    
    *status = recorder->status;
    return VOICE_SUCCESS;
}

voice_error_t voice_recorder_get_buffer(
    voice_recorder_t *recorder,
    int16_t *buffer,
    size_t *num_samples
) {
    if (!recorder || !buffer || !num_samples) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (!recorder->memory_buffer) {
        return VOICE_ERROR_INVALID_STATE;
    }
    
    size_t to_copy = recorder->memory_used;
    if (to_copy > *num_samples) to_copy = *num_samples;
    
    memcpy(buffer, recorder->memory_buffer, to_copy * sizeof(int16_t));
    *num_samples = to_copy;
    
    return VOICE_SUCCESS;
}

/* ============================================
 * 播放器配置
 * ============================================ */

void voice_player_config_init(voice_player_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_player_config_t));
    
    config->sample_rate = 16000;
    config->channels = 1;
    config->volume = 1.0f;
    config->speed = 1.0f;
    config->loop = false;
    config->auto_start = true;
}

/* ============================================
 * 播放器实现
 * ============================================ */

voice_player_t *voice_player_create_from_file(
    const char *filename,
    const voice_player_config_t *config
) {
    if (!filename) return NULL;
    
    voice_player_t *player = (voice_player_t *)calloc(1, sizeof(voice_player_t));
    if (!player) return NULL;
    
    if (config) {
        player->config = *config;
    } else {
        voice_player_config_init(&player->config);
    }
    
    /* 打开文件 */
    player->file = fopen(filename, "rb");
    if (!player->file) {
        free(player);
        return NULL;
    }
    
    /* 读取 WAV 头 */
    wav_header_t header;
    if (fread(&header, sizeof(header), 1, player->file) != 1) {
        fclose(player->file);
        free(player);
        return NULL;
    }
    
    /* 验证格式 */
    if (memcmp(header.riff, "RIFF", 4) != 0 ||
        memcmp(header.wave, "WAVE", 4) != 0 ||
        header.audio_format != 1 ||
        header.bits_per_sample != 16) {
        fclose(player->file);
        free(player);
        return NULL;
    }
    
    player->sample_rate = header.sample_rate;
    player->channels = header.channels;
    player->data_offset = sizeof(wav_header_t);
    player->data_size = header.data_size;
    player->total_samples = header.data_size / (header.channels * 2);
    
    strncpy(player->status.filename, filename,
            sizeof(player->status.filename) - 1);
    
    player->status.state = VOICE_PLAYER_STOPPED;
    player->status.total_duration_ms = (uint32_t)(
        (uint64_t)player->total_samples * 1000 / player->sample_rate);
    player->status.volume = player->config.volume;
    player->status.speed = player->config.speed;
    
    if (player->config.auto_start) {
        voice_player_play(player);
    }
    
    return player;
}

voice_player_t *voice_player_create_from_memory(
    const int16_t *samples,
    size_t num_samples,
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
    
    player->memory_buffer = samples;
    player->memory_size = num_samples;
    player->total_samples = num_samples;
    player->sample_rate = player->config.sample_rate;
    player->channels = player->config.channels;
    
    player->status.state = VOICE_PLAYER_STOPPED;
    player->status.total_duration_ms = (uint32_t)(
        (uint64_t)player->total_samples * 1000 / player->sample_rate);
    player->status.volume = player->config.volume;
    player->status.speed = player->config.speed;
    
    if (player->config.auto_start) {
        voice_player_play(player);
    }
    
    return player;
}

void voice_player_destroy(voice_player_t *player) {
    if (!player) return;
    
    if (player->file) {
        fclose(player->file);
    }
    
    free(player);
}

voice_error_t voice_player_play(voice_player_t *player) {
    if (!player) return VOICE_ERROR_INVALID_PARAM;
    
    player->status.state = VOICE_PLAYER_PLAYING;
    return VOICE_SUCCESS;
}

voice_error_t voice_player_pause(voice_player_t *player) {
    if (!player) return VOICE_ERROR_INVALID_PARAM;
    
    if (player->status.state == VOICE_PLAYER_PLAYING) {
        player->status.state = VOICE_PLAYER_PAUSED;
    }
    return VOICE_SUCCESS;
}

voice_error_t voice_player_stop(voice_player_t *player) {
    if (!player) return VOICE_ERROR_INVALID_PARAM;
    
    player->status.state = VOICE_PLAYER_STOPPED;
    player->position_samples = 0;
    player->status.position_ms = 0;
    
    if (player->file) {
        fseek(player->file, (long)player->data_offset, SEEK_SET);
    }
    
    return VOICE_SUCCESS;
}

voice_error_t voice_player_seek(voice_player_t *player, uint32_t position_ms) {
    if (!player) return VOICE_ERROR_INVALID_PARAM;
    
    size_t target_sample = (size_t)(
        (uint64_t)position_ms * player->sample_rate / 1000);
    
    if (target_sample >= player->total_samples) {
        target_sample = player->total_samples - 1;
    }
    
    player->position_samples = target_sample;
    player->status.position_ms = position_ms;
    
    if (player->file) {
        size_t byte_offset = player->data_offset + 
                             target_sample * player->channels * 2;
        fseek(player->file, (long)byte_offset, SEEK_SET);
    }
    
    return VOICE_SUCCESS;
}

voice_error_t voice_player_set_volume(voice_player_t *player, float volume) {
    if (!player) return VOICE_ERROR_INVALID_PARAM;
    
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 2.0f) volume = 2.0f;
    
    player->config.volume = volume;
    player->status.volume = volume;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_player_set_speed(voice_player_t *player, float speed) {
    if (!player) return VOICE_ERROR_INVALID_PARAM;
    
    if (speed < 0.5f) speed = 0.5f;
    if (speed > 2.0f) speed = 2.0f;
    
    player->config.speed = speed;
    player->status.speed = speed;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_player_read(
    voice_player_t *player,
    int16_t *samples,
    size_t num_samples,
    size_t *samples_read
) {
    if (!player || !samples || !samples_read) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    *samples_read = 0;
    
    if (player->status.state != VOICE_PLAYER_PLAYING) {
        return VOICE_SUCCESS;
    }
    
    size_t remaining = player->total_samples - player->position_samples;
    size_t to_read = num_samples;
    if (to_read > remaining) to_read = remaining;
    
    if (to_read == 0) {
        if (player->config.loop) {
            voice_player_seek(player, 0);
            remaining = player->total_samples;
            to_read = num_samples;
            if (to_read > remaining) to_read = remaining;
        } else {
            player->status.state = VOICE_PLAYER_STOPPED;
            return VOICE_SUCCESS;
        }
    }
    
    if (player->file) {
        *samples_read = fread(samples, sizeof(int16_t), to_read, player->file);
    } else if (player->memory_buffer) {
        memcpy(samples, player->memory_buffer + player->position_samples,
               to_read * sizeof(int16_t));
        *samples_read = to_read;
    }
    
    /* 应用音量 */
    if (player->config.volume != 1.0f) {
        for (size_t i = 0; i < *samples_read; i++) {
            float s = (float)samples[i] * player->config.volume;
            if (s > 32767.0f) s = 32767.0f;
            if (s < -32768.0f) s = -32768.0f;
            samples[i] = (int16_t)s;
        }
    }
    
    player->position_samples += *samples_read;
    player->status.position_ms = (uint32_t)(
        (uint64_t)player->position_samples * 1000 / player->sample_rate);
    
    return VOICE_SUCCESS;
}

voice_error_t voice_player_get_status(
    voice_player_t *player,
    voice_player_status_t *status
) {
    if (!player || !status) return VOICE_ERROR_INVALID_PARAM;
    
    *status = player->status;
    return VOICE_SUCCESS;
}
