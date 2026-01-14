/**
 * @file denoiser.c
 * @brief Audio denoiser implementation (SpeexDSP + RNNoise)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/denoiser.h"
#include "dsp/resampler.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef SONICKIT_HAVE_SPEEXDSP
#include <speex/speex_preprocess.h>
#endif

#ifdef VOICE_HAVE_RNNOISE
#include "rnnoise.h"
#endif

/* ============================================
 * 去噪器结构
 * ============================================ */

struct voice_denoiser_s {
    voice_denoiser_config_t config;
    voice_denoise_engine_t current_engine;
    bool enabled;
    bool initialized;

#ifdef SONICKIT_HAVE_SPEEXDSP
    void *speex_state;
#endif

    float *float_buffer;
    size_t float_buffer_size;
};

/* ============================================
 * 默认配置
 * ============================================ */

void voice_denoiser_config_init(voice_denoiser_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(voice_denoiser_config_t));
    config->engine = VOICE_DENOISE_SPEEXDSP;
    config->sample_rate = 16000;
    config->frame_size = 320;
    config->noise_suppress_db = -25;
    config->enable_agc = false;
    config->enable_vad = true;
    config->agc_level = 8000.0f;
}

/* ============================================
 * 去噪器创建/销毁
 * ============================================ */

voice_denoiser_t *voice_denoiser_create(const voice_denoiser_config_t *config)
{
    if (!config) {
        return NULL;
    }

    voice_denoiser_t *d = (voice_denoiser_t *)calloc(1, sizeof(voice_denoiser_t));
    if (!d) {
        return NULL;
    }

    d->config = *config;
    d->enabled = true;

    d->float_buffer_size = config->frame_size;
    d->float_buffer = (float *)calloc(d->float_buffer_size, sizeof(float));
    if (!d->float_buffer) {
        free(d);
        return NULL;
    }

    /* Stub: 无实际去噪引擎 */
    d->current_engine = VOICE_DENOISE_NONE;
    d->initialized = true;

    return d;
}

void voice_denoiser_destroy(voice_denoiser_t *denoiser)
{
    if (!denoiser) return;

    if (denoiser->float_buffer) {
        free(denoiser->float_buffer);
    }

    free(denoiser);
}

/* ============================================
 * 去噪处理 (Stub)
 * ============================================ */

float voice_denoiser_process_int16(
    voice_denoiser_t *denoiser,
    int16_t *samples,
    size_t count)
{
    if (!denoiser || !denoiser->initialized || !samples || count == 0) {
        return -1.0f;
    }

    if (!denoiser->enabled) {
        return 1.0f;
    }

    /* Stub: 不做任何处理，假设是语音 */
    (void)samples;
    (void)count;
    return 1.0f;
}

float voice_denoiser_process_float(
    voice_denoiser_t *denoiser,
    float *samples,
    size_t count)
{
    if (!denoiser || !denoiser->initialized || !samples || count == 0) {
        return -1.0f;
    }

    /* Stub: 不做任何处理 */
    (void)samples;
    (void)count;
    return 1.0f;
}

/* ============================================
 * 去噪器控制
 * ============================================ */

voice_error_t voice_denoiser_set_noise_suppress(voice_denoiser_t *denoiser, int db)
{
    if (!denoiser || !denoiser->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    denoiser->config.noise_suppress_db = db;
    return VOICE_OK;
}

voice_error_t voice_denoiser_set_enabled(voice_denoiser_t *denoiser, bool enabled)
{
    if (!denoiser) {
        return VOICE_ERROR_NULL_POINTER;
    }
    denoiser->enabled = enabled;
    return VOICE_OK;
}

voice_denoise_engine_t voice_denoiser_get_engine(voice_denoiser_t *denoiser)
{
    if (!denoiser) {
        return VOICE_DENOISE_NONE;
    }
    return denoiser->current_engine;
}

voice_error_t voice_denoiser_switch_engine(
    voice_denoiser_t *denoiser,
    voice_denoise_engine_t engine)
{
    if (!denoiser || !denoiser->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    /* Stub: 只接受 NONE */
    if (engine != VOICE_DENOISE_NONE) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }

    denoiser->current_engine = engine;
    return VOICE_OK;
}

void voice_denoiser_reset(voice_denoiser_t *denoiser)
{
    (void)denoiser;
}

/* ============================================
 * 自适应去噪器
 * ============================================ */

struct voice_adaptive_denoiser_s {
    voice_denoiser_t *denoiser;
    voice_adaptive_denoiser_config_t config;
};

void voice_adaptive_denoiser_config_init(voice_adaptive_denoiser_config_t *config)
{
    if (!config) return;

    voice_denoiser_config_init(&config->base);
    config->base.engine = VOICE_DENOISE_AUTO;
    config->cpu_threshold_high = 80.0f;
    config->cpu_threshold_low = 40.0f;
    config->battery_threshold = 20;
    config->switch_interval_ms = 5000;
}

voice_adaptive_denoiser_t *voice_adaptive_denoiser_create(
    const voice_adaptive_denoiser_config_t *config)
{
    if (!config) return NULL;

    voice_adaptive_denoiser_t *ad = (voice_adaptive_denoiser_t *)calloc(
        1, sizeof(voice_adaptive_denoiser_t));
    if (!ad) return NULL;

    ad->config = *config;
    ad->denoiser = voice_denoiser_create(&config->base);
    if (!ad->denoiser) {
        free(ad);
        return NULL;
    }

    return ad;
}

void voice_adaptive_denoiser_destroy(voice_adaptive_denoiser_t *denoiser)
{
    if (denoiser) {
        if (denoiser->denoiser) {
            voice_denoiser_destroy(denoiser->denoiser);
        }
        free(denoiser);
    }
}

float voice_adaptive_denoiser_process(
    voice_adaptive_denoiser_t *denoiser,
    int16_t *samples,
    size_t count)
{
    if (!denoiser || !denoiser->denoiser) {
        return -1.0f;
    }

    return voice_denoiser_process_int16(denoiser->denoiser, samples, count);
}

voice_denoise_engine_t voice_adaptive_denoiser_get_engine(
    voice_adaptive_denoiser_t *denoiser)
{
    if (!denoiser || !denoiser->denoiser) {
        return VOICE_DENOISE_NONE;
    }
    return voice_denoiser_get_engine(denoiser->denoiser);
}
