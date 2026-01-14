/**
 * @file denoiser.c
 * @brief Audio denoiser implementation (SpeexDSP + RNNoise)
 */

#include "dsp/denoiser.h"
#include "dsp/resampler.h"
#include "voice/error.h"
#include <speex/speex_preprocess.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef VOICE_HAVE_RNNOISE
#include "rnnoise.h"
#endif

/* RNNoise 固定参数 */
#define RNNOISE_SAMPLE_RATE 48000
#define RNNOISE_FRAME_SIZE  480

/* ============================================
 * 去噪器结构
 * ============================================ */

struct voice_denoiser_s {
    voice_denoiser_config_t config;
    voice_denoise_engine_t current_engine;
    bool enabled;
    bool initialized;
    
    /* SpeexDSP 去噪器 */
    SpeexPreprocessState *speex_state;
    
    /* RNNoise 去噪器 */
#ifdef VOICE_HAVE_RNNOISE
    DenoiseState *rnnoise_state;
    voice_resampler_t *resampler_in;    /* 输入重采样到48kHz */
    voice_resampler_t *resampler_out;   /* 输出重采样回原始采样率 */
    float *rnnoise_buffer;              /* RNNoise处理缓冲区 */
    size_t rnnoise_buffer_size;
#endif
    
    /* 临时缓冲区 */
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
    config->frame_size = 320;  /* 20ms @ 16kHz */
    config->noise_suppress_db = -25;
    config->enable_agc = false;
    config->enable_vad = true;
    config->agc_level = 8000.0f;
}

/* ============================================
 * 去噪器创建/销毁
 * ============================================ */

static voice_error_t init_speex_denoiser(voice_denoiser_t *d)
{
    d->speex_state = speex_preprocess_state_init(
        d->config.frame_size,
        d->config.sample_rate
    );
    
    if (!d->speex_state) {
        return VOICE_ERROR_DSP_DENOISE_FAILED;
    }
    
    int val;
    
    /* 启用去噪 */
    val = 1;
    speex_preprocess_ctl(d->speex_state, SPEEX_PREPROCESS_SET_DENOISE, &val);
    
    /* 设置噪声抑制量 */
    val = d->config.noise_suppress_db;
    speex_preprocess_ctl(d->speex_state, SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &val);
    
    /* AGC */
    val = d->config.enable_agc ? 1 : 0;
    speex_preprocess_ctl(d->speex_state, SPEEX_PREPROCESS_SET_AGC, &val);
    
    if (d->config.enable_agc) {
        float level = d->config.agc_level;
        speex_preprocess_ctl(d->speex_state, SPEEX_PREPROCESS_SET_AGC_LEVEL, &level);
    }
    
    /* VAD */
    val = d->config.enable_vad ? 1 : 0;
    speex_preprocess_ctl(d->speex_state, SPEEX_PREPROCESS_SET_VAD, &val);
    
    return VOICE_OK;
}

#ifdef VOICE_HAVE_RNNOISE
static voice_error_t init_rnnoise_denoiser(voice_denoiser_t *d)
{
    d->rnnoise_state = rnnoise_create(NULL);
    if (!d->rnnoise_state) {
        return VOICE_ERROR_DSP_DENOISE_FAILED;
    }
    
    /* 如果采样率不是48kHz，需要重采样 */
    if (d->config.sample_rate != RNNOISE_SAMPLE_RATE) {
        d->resampler_in = voice_resampler_create(
            1, d->config.sample_rate, RNNOISE_SAMPLE_RATE, 
            VOICE_RESAMPLE_QUALITY_DEFAULT
        );
        
        d->resampler_out = voice_resampler_create(
            1, RNNOISE_SAMPLE_RATE, d->config.sample_rate,
            VOICE_RESAMPLE_QUALITY_DEFAULT
        );
        
        if (!d->resampler_in || !d->resampler_out) {
            return VOICE_ERROR_DSP_RESAMPLE_FAILED;
        }
    }
    
    /* 分配RNNoise处理缓冲区 */
    size_t max_frames = (d->config.frame_size * RNNOISE_SAMPLE_RATE / d->config.sample_rate) + RNNOISE_FRAME_SIZE;
    d->rnnoise_buffer_size = max_frames;
    d->rnnoise_buffer = (float *)calloc(max_frames, sizeof(float));
    if (!d->rnnoise_buffer) {
        return VOICE_ERROR_OUT_OF_MEMORY;
    }
    
    return VOICE_OK;
}
#endif

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
    
    /* 分配浮点缓冲区 */
    d->float_buffer_size = config->frame_size;
    d->float_buffer = (float *)calloc(d->float_buffer_size, sizeof(float));
    if (!d->float_buffer) {
        free(d);
        return NULL;
    }
    
    /* 根据引擎类型初始化 */
    voice_denoise_engine_t engine = config->engine;
    
#ifndef VOICE_HAVE_RNNOISE
    if (engine == VOICE_DENOISE_RNNOISE || engine == VOICE_DENOISE_AUTO) {
        engine = VOICE_DENOISE_SPEEXDSP;
    }
#endif
    
    voice_error_t err = VOICE_OK;
    
    if (engine == VOICE_DENOISE_SPEEXDSP || engine == VOICE_DENOISE_AUTO) {
        err = init_speex_denoiser(d);
        if (err != VOICE_OK) {
            free(d->float_buffer);
            free(d);
            return NULL;
        }
    }
    
#ifdef VOICE_HAVE_RNNOISE
    if (engine == VOICE_DENOISE_RNNOISE || engine == VOICE_DENOISE_AUTO) {
        err = init_rnnoise_denoiser(d);
        if (err != VOICE_OK && engine == VOICE_DENOISE_RNNOISE) {
            /* RNNoise初始化失败且明确要求使用RNNoise */
            if (d->speex_state) {
                speex_preprocess_state_destroy(d->speex_state);
            }
            free(d->float_buffer);
            free(d);
            return NULL;
        }
    }
#endif
    
    /* 设置当前引擎 */
    if (engine == VOICE_DENOISE_AUTO) {
#ifdef VOICE_HAVE_RNNOISE
        d->current_engine = VOICE_DENOISE_RNNOISE;  /* 默认使用高质量引擎 */
#else
        d->current_engine = VOICE_DENOISE_SPEEXDSP;
#endif
    } else {
        d->current_engine = engine;
    }
    
    d->initialized = true;
    VOICE_LOG_I("Denoiser created with engine: %d", d->current_engine);
    
    return d;
}

void voice_denoiser_destroy(voice_denoiser_t *denoiser)
{
    if (!denoiser) return;
    
    if (denoiser->speex_state) {
        speex_preprocess_state_destroy(denoiser->speex_state);
    }
    
#ifdef VOICE_HAVE_RNNOISE
    if (denoiser->rnnoise_state) {
        rnnoise_destroy(denoiser->rnnoise_state);
    }
    if (denoiser->resampler_in) {
        voice_resampler_destroy(denoiser->resampler_in);
    }
    if (denoiser->resampler_out) {
        voice_resampler_destroy(denoiser->resampler_out);
    }
    if (denoiser->rnnoise_buffer) {
        free(denoiser->rnnoise_buffer);
    }
#endif
    
    if (denoiser->float_buffer) {
        free(denoiser->float_buffer);
    }
    
    free(denoiser);
}

/* ============================================
 * 去噪处理
 * ============================================ */

static float process_speex(voice_denoiser_t *d, int16_t *samples, size_t count)
{
    if (!d->speex_state) {
        return -1.0f;
    }
    
    /* SpeexDSP 需要按帧处理 */
    size_t frame_size = d->config.frame_size;
    size_t offset = 0;
    int vad = 1;
    
    while (offset + frame_size <= count) {
        vad = speex_preprocess_run(d->speex_state, samples + offset);
        offset += frame_size;
    }
    
    /* 处理剩余样本 (填充静音) */
    if (offset < count) {
        int16_t temp[frame_size];
        memset(temp, 0, sizeof(temp));
        memcpy(temp, samples + offset, (count - offset) * sizeof(int16_t));
        vad = speex_preprocess_run(d->speex_state, temp);
        memcpy(samples + offset, temp, (count - offset) * sizeof(int16_t));
    }
    
    return vad ? 1.0f : 0.0f;
}

#ifdef VOICE_HAVE_RNNOISE
static float process_rnnoise(voice_denoiser_t *d, int16_t *samples, size_t count)
{
    if (!d->rnnoise_state) {
        return -1.0f;
    }
    
    float vad_prob = 0.0f;
    
    /* 转换为float并缩放到 [-1, 1] (RNNoise需要原始范围) */
    /* 注意: RNNoise 实际上期望 [-32768, 32768] 范围 */
    for (size_t i = 0; i < count; i++) {
        d->float_buffer[i] = (float)samples[i];
    }
    
    /* 如果需要重采样 */
    if (d->resampler_in && d->config.sample_rate != RNNOISE_SAMPLE_RATE) {
        /* 重采样到48kHz */
        size_t out_frames = voice_resampler_get_output_frames(d->resampler_in, count);
        int processed = voice_resampler_process_float(
            d->resampler_in,
            d->float_buffer, count,
            d->rnnoise_buffer, out_frames
        );
        
        if (processed <= 0) {
            return -1.0f;
        }
        
        /* 按480样本帧处理 */
        size_t offset = 0;
        while (offset + RNNOISE_FRAME_SIZE <= (size_t)processed) {
            vad_prob = rnnoise_process_frame(
                d->rnnoise_state,
                d->rnnoise_buffer + offset,
                d->rnnoise_buffer + offset
            );
            offset += RNNOISE_FRAME_SIZE;
        }
        
        /* 重采样回原始采样率 */
        int final_count = voice_resampler_process_float(
            d->resampler_out,
            d->rnnoise_buffer, processed,
            d->float_buffer, count
        );
        
        /* 转换回int16 */
        for (size_t i = 0; i < count && i < (size_t)final_count; i++) {
            float val = d->float_buffer[i];
            if (val > 32767.0f) val = 32767.0f;
            if (val < -32768.0f) val = -32768.0f;
            samples[i] = (int16_t)val;
        }
    } else {
        /* 直接处理 (已经是48kHz) */
        size_t offset = 0;
        while (offset + RNNOISE_FRAME_SIZE <= count) {
            vad_prob = rnnoise_process_frame(
                d->rnnoise_state,
                d->float_buffer + offset,
                d->float_buffer + offset
            );
            offset += RNNOISE_FRAME_SIZE;
        }
        
        /* 转换回int16 */
        for (size_t i = 0; i < count; i++) {
            float val = d->float_buffer[i];
            if (val > 32767.0f) val = 32767.0f;
            if (val < -32768.0f) val = -32768.0f;
            samples[i] = (int16_t)val;
        }
    }
    
    return vad_prob;
}
#endif

float voice_denoiser_process_int16(
    voice_denoiser_t *denoiser,
    int16_t *samples,
    size_t count)
{
    if (!denoiser || !denoiser->initialized || !samples || count == 0) {
        return -1.0f;
    }
    
    if (!denoiser->enabled) {
        return 1.0f;  /* 禁用时假设是语音 */
    }
    
    switch (denoiser->current_engine) {
        case VOICE_DENOISE_SPEEXDSP:
            return process_speex(denoiser, samples, count);
            
#ifdef VOICE_HAVE_RNNOISE
        case VOICE_DENOISE_RNNOISE:
            return process_rnnoise(denoiser, samples, count);
#endif
            
        default:
            return process_speex(denoiser, samples, count);
    }
}

float voice_denoiser_process_float(
    voice_denoiser_t *denoiser,
    float *samples,
    size_t count)
{
    if (!denoiser || !denoiser->initialized || !samples || count == 0) {
        return -1.0f;
    }
    
    /* 转换为int16处理，然后转回float */
    int16_t *temp = (int16_t *)malloc(count * sizeof(int16_t));
    if (!temp) {
        return -1.0f;
    }
    
    /* float -> int16 */
    for (size_t i = 0; i < count; i++) {
        float val = samples[i] * 32768.0f;
        if (val > 32767.0f) val = 32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        temp[i] = (int16_t)val;
    }
    
    float vad = voice_denoiser_process_int16(denoiser, temp, count);
    
    /* int16 -> float */
    for (size_t i = 0; i < count; i++) {
        samples[i] = (float)temp[i] / 32768.0f;
    }
    
    free(temp);
    return vad;
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
    
    if (denoiser->speex_state) {
        speex_preprocess_ctl(denoiser->speex_state, 
            SPEEX_PREPROCESS_SET_NOISE_SUPPRESS, &db);
    }
    
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
    
#ifndef VOICE_HAVE_RNNOISE
    if (engine == VOICE_DENOISE_RNNOISE) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }
#endif
    
    if (engine == denoiser->current_engine) {
        return VOICE_OK;
    }
    
    /* 验证目标引擎可用 */
    if (engine == VOICE_DENOISE_SPEEXDSP && !denoiser->speex_state) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
#ifdef VOICE_HAVE_RNNOISE
    if (engine == VOICE_DENOISE_RNNOISE && !denoiser->rnnoise_state) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
#endif
    
    denoiser->current_engine = engine;
    VOICE_LOG_I("Denoiser engine switched to: %d", engine);
    
    return VOICE_OK;
}

void voice_denoiser_reset(voice_denoiser_t *denoiser)
{
    if (!denoiser || !denoiser->initialized) {
        return;
    }
    
    /* 重新初始化 SpeexDSP */
    if (denoiser->speex_state) {
        speex_preprocess_state_destroy(denoiser->speex_state);
        init_speex_denoiser(denoiser);
    }
    
#ifdef VOICE_HAVE_RNNOISE
    /* 重新初始化 RNNoise */
    if (denoiser->rnnoise_state) {
        rnnoise_destroy(denoiser->rnnoise_state);
        denoiser->rnnoise_state = rnnoise_create(NULL);
    }
    
    if (denoiser->resampler_in) {
        voice_resampler_reset(denoiser->resampler_in);
    }
    if (denoiser->resampler_out) {
        voice_resampler_reset(denoiser->resampler_out);
    }
#endif
}

/* ============================================
 * 自适应去噪器 (简化实现)
 * ============================================ */

struct voice_adaptive_denoiser_s {
    voice_denoiser_t *denoiser;
    voice_adaptive_denoiser_config_t config;
    uint64_t last_switch_time;
    float avg_process_time_us;
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
    
    /* TODO: 实现自适应切换逻辑
     * 1. 监控CPU使用率
     * 2. 检查电池状态
     * 3. 根据阈值切换引擎
     */
    
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
