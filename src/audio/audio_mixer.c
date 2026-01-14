/**
 * @file audio_mixer.c
 * @brief Multi-stream audio mixer implementation
 */

#include "audio/audio_mixer.h"
#include "audio/audio_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    typedef CRITICAL_SECTION mixer_mutex_t;
    #define MIXER_MUTEX_INIT(m)     InitializeCriticalSection(&(m))
    #define MIXER_MUTEX_DESTROY(m)  DeleteCriticalSection(&(m))
    #define MIXER_MUTEX_LOCK(m)     EnterCriticalSection(&(m))
    #define MIXER_MUTEX_UNLOCK(m)   LeaveCriticalSection(&(m))
#else
    #include <pthread.h>
    typedef pthread_mutex_t mixer_mutex_t;
    #define MIXER_MUTEX_INIT(m)     pthread_mutex_init(&(m), NULL)
    #define MIXER_MUTEX_DESTROY(m)  pthread_mutex_destroy(&(m))
    #define MIXER_MUTEX_LOCK(m)     pthread_mutex_lock(&(m))
    #define MIXER_MUTEX_UNLOCK(m)   pthread_mutex_unlock(&(m))
#endif

/* ============================================
 * 内部结构
 * ============================================ */

typedef struct {
    mixer_source_id_t id;
    bool active;
    voice_mixer_source_config_t config;
    voice_audio_buffer_t *buffer;
    float current_level;            /**< 当前电平 (用于 Loudest N) */
} mixer_source_t;

struct voice_mixer_s {
    voice_mixer_config_t config;
    
    /* 音源数组 */
    mixer_source_t *sources;
    size_t source_count;
    size_t active_count;
    mixer_source_id_t next_id;
    
    /* 混音缓冲区 */
    float *mix_buffer;          /**< 浮点混音缓冲区 */
    float *source_buffer;       /**< 单个音源临时缓冲区 */
    size_t buffer_size;
    
    /* 限制器状态 */
    float limiter_gain;
    float limiter_release_rate;
    
    /* 统计 */
    voice_mixer_stats_t stats;
    
    /* 同步 */
    mixer_mutex_t mutex;
};

/* ============================================
 * 辅助函数
 * ============================================ */

static float compute_rms(const float *samples, size_t count) {
    if (count == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum_sq += samples[i] * samples[i];
    }
    return sqrtf(sum_sq / count);
}

static float linear_to_db(float linear) {
    if (linear <= 0.0f) return -96.0f;
    float db = 20.0f * log10f(linear);
    return db < -96.0f ? -96.0f : db;
}

/* 软限制器 */
static void apply_limiter(
    voice_mixer_t *mixer,
    float *samples,
    size_t count,
    float threshold
) {
    float target_gain = 1.0f;
    
    for (size_t i = 0; i < count; i++) {
        float abs_sample = fabsf(samples[i]);
        
        if (abs_sample > threshold) {
            /* 计算需要的增益 */
            target_gain = threshold / abs_sample;
            if (target_gain < mixer->limiter_gain) {
                mixer->limiter_gain = target_gain;
            }
        }
        
        /* 应用增益 */
        samples[i] *= mixer->limiter_gain;
        
        /* 释放阶段 */
        if (mixer->limiter_gain < 1.0f) {
            mixer->limiter_gain += mixer->limiter_release_rate;
            if (mixer->limiter_gain > 1.0f) {
                mixer->limiter_gain = 1.0f;
            }
        }
    }
}

/* ============================================
 * 配置
 * ============================================ */

void voice_mixer_config_init(voice_mixer_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_mixer_config_t));
    
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_size = 960;       /* 20ms @ 48kHz */
    config->max_sources = 16;
    config->algorithm = VOICE_MIX_NORMALIZED;
    config->loudest_n = 3;
    config->master_gain = 1.0f;
    config->enable_limiter = true;
    config->limiter_threshold_db = -3.0f;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_mixer_t *voice_mixer_create(const voice_mixer_config_t *config) {
    voice_mixer_t *mixer = (voice_mixer_t *)calloc(1, sizeof(voice_mixer_t));
    if (!mixer) return NULL;
    
    if (config) {
        mixer->config = *config;
    } else {
        voice_mixer_config_init(&mixer->config);
    }
    
    /* 分配音源数组 */
    mixer->sources = (mixer_source_t *)calloc(
        mixer->config.max_sources, sizeof(mixer_source_t)
    );
    if (!mixer->sources) {
        free(mixer);
        return NULL;
    }
    
    /* 分配混音缓冲区 */
    mixer->buffer_size = mixer->config.frame_size * mixer->config.channels;
    mixer->mix_buffer = (float *)calloc(mixer->buffer_size, sizeof(float));
    mixer->source_buffer = (float *)calloc(mixer->buffer_size, sizeof(float));
    
    if (!mixer->mix_buffer || !mixer->source_buffer) {
        free(mixer->sources);
        free(mixer->mix_buffer);
        free(mixer->source_buffer);
        free(mixer);
        return NULL;
    }
    
    /* 初始化限制器 */
    mixer->limiter_gain = 1.0f;
    /* 释放时间约 100ms */
    float release_time_ms = 100.0f;
    float frames_per_ms = (float)mixer->config.sample_rate / 1000.0f;
    mixer->limiter_release_rate = 1.0f / (release_time_ms * frames_per_ms);
    
    mixer->next_id = 1;
    
    MIXER_MUTEX_INIT(mixer->mutex);
    
    return mixer;
}

void voice_mixer_destroy(voice_mixer_t *mixer) {
    if (!mixer) return;
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    /* 销毁所有音源缓冲区 */
    for (size_t i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].buffer) {
            voice_audio_buffer_destroy(mixer->sources[i].buffer);
        }
    }
    
    free(mixer->sources);
    free(mixer->mix_buffer);
    free(mixer->source_buffer);
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    MIXER_MUTEX_DESTROY(mixer->mutex);
    
    free(mixer);
}

/* ============================================
 * 音源管理
 * ============================================ */

mixer_source_id_t voice_mixer_add_source(
    voice_mixer_t *mixer,
    const voice_mixer_source_config_t *config
) {
    if (!mixer) return MIXER_INVALID_SOURCE_ID;
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    /* 查找空闲槽位 */
    size_t slot = mixer->config.max_sources;
    for (size_t i = 0; i < mixer->config.max_sources; i++) {
        if (!mixer->sources[i].active) {
            slot = i;
            break;
        }
    }
    
    if (slot >= mixer->config.max_sources) {
        MIXER_MUTEX_UNLOCK(mixer->mutex);
        return MIXER_INVALID_SOURCE_ID;
    }
    
    /* 创建音源缓冲区 */
    voice_audio_buffer_config_t buf_config;
    voice_audio_buffer_config_init(&buf_config);
    buf_config.capacity = mixer->buffer_size * 4;  /* 缓存 4 帧 */
    
    mixer->sources[slot].buffer = voice_audio_buffer_create(&buf_config);
    if (!mixer->sources[slot].buffer) {
        MIXER_MUTEX_UNLOCK(mixer->mutex);
        return MIXER_INVALID_SOURCE_ID;
    }
    
    /* 初始化音源 */
    mixer->sources[slot].id = mixer->next_id++;
    mixer->sources[slot].active = true;
    mixer->sources[slot].current_level = 0.0f;
    
    if (config) {
        mixer->sources[slot].config = *config;
    } else {
        mixer->sources[slot].config.gain = 1.0f;
        mixer->sources[slot].config.pan = 0.0f;
        mixer->sources[slot].config.muted = false;
        mixer->sources[slot].config.priority = 0;
    }
    
    if (slot >= mixer->source_count) {
        mixer->source_count = slot + 1;
    }
    mixer->active_count++;
    
    mixer_source_id_t id = mixer->sources[slot].id;
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    
    return id;
}

voice_error_t voice_mixer_remove_source(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id
) {
    if (!mixer || source_id == MIXER_INVALID_SOURCE_ID) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    for (size_t i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            voice_audio_buffer_destroy(mixer->sources[i].buffer);
            mixer->sources[i].buffer = NULL;
            mixer->sources[i].active = false;
            mixer->active_count--;
            
            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return VOICE_SUCCESS;
        }
    }
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

voice_error_t voice_mixer_push_audio(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    const int16_t *samples,
    size_t num_samples
) {
    if (!mixer || !samples || source_id == MIXER_INVALID_SOURCE_ID) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    for (size_t i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            size_t written = voice_audio_buffer_write(
                mixer->sources[i].buffer,
                samples,
                num_samples
            );
            
            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return (written == num_samples) ? VOICE_SUCCESS : VOICE_ERROR_BUFFER_FULL;
        }
    }
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

/* ============================================
 * 混音
 * ============================================ */

voice_error_t voice_mixer_get_output(
    voice_mixer_t *mixer,
    int16_t *output,
    size_t num_samples,
    size_t *samples_out
) {
    if (!mixer || !output) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (num_samples > mixer->buffer_size) {
        num_samples = mixer->buffer_size;
    }
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    /* 清零混音缓冲区 */
    memset(mixer->mix_buffer, 0, num_samples * sizeof(float));
    
    size_t active_sources = 0;
    const float scale = 1.0f / 32768.0f;
    
    /* 收集活跃音源 */
    for (size_t i = 0; i < mixer->source_count; i++) {
        mixer_source_t *src = &mixer->sources[i];
        
        if (!src->active || src->config.muted) continue;
        if (voice_audio_buffer_available(src->buffer) < num_samples) continue;
        
        /* 读取音频 */
        int16_t *temp_i16 = (int16_t *)malloc(num_samples * sizeof(int16_t));
        if (!temp_i16) continue;
        
        size_t read = voice_audio_buffer_read(src->buffer, temp_i16, num_samples);
        if (read == 0) {
            free(temp_i16);
            continue;
        }
        
        /* 转换到浮点并计算电平 */
        float sum_sq = 0.0f;
        for (size_t j = 0; j < read; j++) {
            mixer->source_buffer[j] = (float)temp_i16[j] * scale * src->config.gain;
            sum_sq += mixer->source_buffer[j] * mixer->source_buffer[j];
        }
        src->current_level = sqrtf(sum_sq / read);
        
        free(temp_i16);
        active_sources++;
        
        /* 累加到混音缓冲区 */
        for (size_t j = 0; j < read; j++) {
            mixer->mix_buffer[j] += mixer->source_buffer[j];
        }
    }
    
    /* 归一化 */
    if (mixer->config.algorithm == VOICE_MIX_NORMALIZED && active_sources > 1) {
        float norm_factor = 1.0f / sqrtf((float)active_sources);
        for (size_t i = 0; i < num_samples; i++) {
            mixer->mix_buffer[i] *= norm_factor;
        }
    } else if (mixer->config.algorithm == VOICE_MIX_AVERAGE && active_sources > 0) {
        float avg_factor = 1.0f / (float)active_sources;
        for (size_t i = 0; i < num_samples; i++) {
            mixer->mix_buffer[i] *= avg_factor;
        }
    }
    
    /* 应用主增益 */
    if (mixer->config.master_gain != 1.0f) {
        for (size_t i = 0; i < num_samples; i++) {
            mixer->mix_buffer[i] *= mixer->config.master_gain;
        }
    }
    
    /* 应用限制器 */
    if (mixer->config.enable_limiter) {
        float threshold = powf(10.0f, mixer->config.limiter_threshold_db / 20.0f);
        apply_limiter(mixer, mixer->mix_buffer, num_samples, threshold);
    }
    
    /* 更新峰值统计 */
    float peak = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        float abs_val = fabsf(mixer->mix_buffer[i]);
        if (abs_val > peak) peak = abs_val;
    }
    float peak_db = linear_to_db(peak);
    if (peak_db > mixer->stats.peak_level_db) {
        mixer->stats.peak_level_db = peak_db;
    }
    
    /* 转换回 int16 */
    uint32_t clips = 0;
    for (size_t i = 0; i < num_samples; i++) {
        float sample = mixer->mix_buffer[i] * 32768.0f;
        if (sample > 32767.0f) {
            sample = 32767.0f;
            clips++;
        } else if (sample < -32768.0f) {
            sample = -32768.0f;
            clips++;
        }
        output[i] = (int16_t)sample;
    }
    
    mixer->stats.clip_count += clips;
    mixer->stats.frames_mixed++;
    mixer->stats.active_sources = active_sources;
    mixer->stats.total_sources = mixer->active_count;
    
    if (samples_out) {
        *samples_out = num_samples;
    }
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    
    return VOICE_SUCCESS;
}

/* ============================================
 * 控制接口
 * ============================================ */

voice_error_t voice_mixer_set_source_gain(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    float gain
) {
    if (!mixer || source_id == MIXER_INVALID_SOURCE_ID) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    for (size_t i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            mixer->sources[i].config.gain = gain;
            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return VOICE_SUCCESS;
        }
    }
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

voice_error_t voice_mixer_set_source_muted(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    bool muted
) {
    if (!mixer || source_id == MIXER_INVALID_SOURCE_ID) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    
    for (size_t i = 0; i < mixer->source_count; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            mixer->sources[i].config.muted = muted;
            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return VOICE_SUCCESS;
        }
    }
    
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

voice_error_t voice_mixer_set_master_gain(voice_mixer_t *mixer, float gain) {
    if (!mixer) return VOICE_ERROR_INVALID_PARAM;
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    mixer->config.master_gain = gain;
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    
    return VOICE_SUCCESS;
}

size_t voice_mixer_get_active_sources(voice_mixer_t *mixer) {
    if (!mixer) return 0;
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    size_t count = mixer->active_count;
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    
    return count;
}

voice_error_t voice_mixer_get_stats(
    voice_mixer_t *mixer,
    voice_mixer_stats_t *stats
) {
    if (!mixer || !stats) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    MIXER_MUTEX_LOCK(mixer->mutex);
    *stats = mixer->stats;
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    
    return VOICE_SUCCESS;
}
