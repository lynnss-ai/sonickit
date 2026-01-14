/**
 * @file audio_mixer.c
 * @brief Multi-stream audio mixer implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
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
    voice_ring_buffer_t *buffer;
    float current_level;
} mixer_source_t;

struct voice_mixer_s {
    voice_mixer_config_t config;

    mixer_source_t *sources;
    size_t source_count;
    size_t active_count;
    mixer_source_id_t next_id;

    float *mix_buffer;
    float *source_buffer;
    size_t buffer_size;

    float limiter_gain;
    float limiter_release_rate;

    voice_mixer_stats_t stats;

    mixer_mutex_t mutex;
};

/* ============================================
 * 辅助函数
 * ============================================ */

static void apply_limiter(
    voice_mixer_t *mixer,
    float *samples,
    size_t count,
    float threshold
) {
    for (size_t i = 0; i < count; i++) {
        float abs_sample = fabsf(samples[i]);

        if (abs_sample > threshold) {
            float target_gain = threshold / abs_sample;
            if (target_gain < mixer->limiter_gain) {
                mixer->limiter_gain = target_gain;
            }
        }

        samples[i] *= mixer->limiter_gain;

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
    config->frame_size = 960;
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

    mixer->sources = (mixer_source_t *)calloc(
        mixer->config.max_sources, sizeof(mixer_source_t)
    );
    if (!mixer->sources) {
        free(mixer);
        return NULL;
    }

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

    mixer->limiter_gain = 1.0f;
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

    for (size_t i = 0; i < mixer->config.max_sources; i++) {
        if (mixer->sources[i].buffer) {
            voice_ring_buffer_destroy(mixer->sources[i].buffer);
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
    size_t buf_capacity = mixer->buffer_size * 4 * sizeof(int16_t);
    mixer->sources[slot].buffer = voice_ring_buffer_create(buf_capacity, sizeof(int16_t));
    if (!mixer->sources[slot].buffer) {
        MIXER_MUTEX_UNLOCK(mixer->mutex);
        return MIXER_INVALID_SOURCE_ID;
    }

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

    mixer->source_count++;
    mixer->active_count++;

    mixer_source_id_t id = mixer->sources[slot].id;

    MIXER_MUTEX_UNLOCK(mixer->mutex);

    return id;
}

voice_error_t voice_mixer_remove_source(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id
) {
    if (!mixer) return VOICE_ERROR_NULL_POINTER;

    MIXER_MUTEX_LOCK(mixer->mutex);

    for (size_t i = 0; i < mixer->config.max_sources; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            if (mixer->sources[i].buffer) {
                voice_ring_buffer_destroy(mixer->sources[i].buffer);
                mixer->sources[i].buffer = NULL;
            }
            mixer->sources[i].active = false;
            mixer->active_count--;

            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return VOICE_OK;
        }
    }

    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

/* ============================================
 * 音频处理
 * ============================================ */

voice_error_t voice_mixer_push_audio(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    const int16_t *samples,
    size_t num_samples
) {
    if (!mixer || !samples) return VOICE_ERROR_NULL_POINTER;

    MIXER_MUTEX_LOCK(mixer->mutex);

    for (size_t i = 0; i < mixer->config.max_sources; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            if (mixer->sources[i].buffer) {
                voice_ring_buffer_write(
                    mixer->sources[i].buffer,
                    samples,
                    num_samples * sizeof(int16_t)
                );
            }
            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return VOICE_OK;
        }
    }

    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

voice_error_t voice_mixer_get_output(
    voice_mixer_t *mixer,
    int16_t *output,
    size_t num_samples,
    size_t *samples_out
) {
    if (!mixer || !output) return VOICE_ERROR_NULL_POINTER;

    MIXER_MUTEX_LOCK(mixer->mutex);

    /* 清空混音缓冲区 */
    memset(mixer->mix_buffer, 0, mixer->buffer_size * sizeof(float));

    size_t active_sources = 0;
    int16_t *temp_i16 = (int16_t *)malloc(num_samples * sizeof(int16_t));
    if (!temp_i16) {
        MIXER_MUTEX_UNLOCK(mixer->mutex);
        return VOICE_ERROR_OUT_OF_MEMORY;
    }

    for (size_t i = 0; i < mixer->config.max_sources; i++) {
        mixer_source_t *src = &mixer->sources[i];

        if (!src->active || src->config.muted || !src->buffer) continue;
        if (voice_ring_buffer_available(src->buffer) < num_samples * sizeof(int16_t)) continue;

        size_t read = voice_ring_buffer_read(src->buffer, temp_i16, num_samples * sizeof(int16_t));
        if (read < num_samples * sizeof(int16_t)) continue;

        /* 转换为浮点并应用增益 */
        float gain = src->config.gain;
        for (size_t j = 0; j < num_samples; j++) {
            mixer->mix_buffer[j] += (float)temp_i16[j] / 32768.0f * gain;
        }

        active_sources++;
    }

    free(temp_i16);

    /* 归一化 */
    if (active_sources > 1 && mixer->config.algorithm == VOICE_MIX_NORMALIZED) {
        float scale = 1.0f / sqrtf((float)active_sources);
        for (size_t i = 0; i < num_samples; i++) {
            mixer->mix_buffer[i] *= scale;
        }
    }

    /* 应用主增益 */
    for (size_t i = 0; i < num_samples; i++) {
        mixer->mix_buffer[i] *= mixer->config.master_gain;
    }

    /* 应用限制器 */
    if (mixer->config.enable_limiter) {
        float threshold = powf(10.0f, mixer->config.limiter_threshold_db / 20.0f);
        apply_limiter(mixer, mixer->mix_buffer, num_samples, threshold);
    }

    /* 转换回 int16 */
    for (size_t i = 0; i < num_samples; i++) {
        float val = mixer->mix_buffer[i] * 32768.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        output[i] = (int16_t)val;
    }

    if (samples_out) {
        *samples_out = num_samples;
    }

    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_OK;
}

/* ============================================
 * 控制
 * ============================================ */

voice_error_t voice_mixer_set_source_config(
    voice_mixer_t *mixer,
    mixer_source_id_t source_id,
    const voice_mixer_source_config_t *config
) {
    if (!mixer || !config) return VOICE_ERROR_NULL_POINTER;

    MIXER_MUTEX_LOCK(mixer->mutex);

    for (size_t i = 0; i < mixer->config.max_sources; i++) {
        if (mixer->sources[i].active && mixer->sources[i].id == source_id) {
            mixer->sources[i].config = *config;
            MIXER_MUTEX_UNLOCK(mixer->mutex);
            return VOICE_OK;
        }
    }

    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_ERROR_NOT_FOUND;
}

voice_error_t voice_mixer_set_master_gain(voice_mixer_t *mixer, float gain) {
    if (!mixer) return VOICE_ERROR_NULL_POINTER;
    mixer->config.master_gain = gain;
    return VOICE_OK;
}

voice_error_t voice_mixer_get_stats(voice_mixer_t *mixer, voice_mixer_stats_t *stats) {
    if (!mixer || !stats) return VOICE_ERROR_NULL_POINTER;

    MIXER_MUTEX_LOCK(mixer->mutex);
    *stats = mixer->stats;
    MIXER_MUTEX_UNLOCK(mixer->mutex);
    return VOICE_OK;
}
