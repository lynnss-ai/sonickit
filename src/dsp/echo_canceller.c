/**
 * @file echo_canceller.c
 * @brief SpeexDSP AEC implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/echo_canceller.h"
#include "voice/error.h"
#include <speex/speex_echo.h>
#include <speex/speex_preprocess.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * 回声消除器结构
 * ============================================ */

struct voice_aec_s {
    voice_aec_config_t config;
    SpeexEchoState *echo_state;
    SpeexPreprocessState *preprocess_state;
    bool enabled;
    bool initialized;
    
    /* 临时缓冲区 */
    int16_t *temp_buffer;
    size_t temp_buffer_size;
};

/* ============================================
 * 默认配置
 * ============================================ */

void voice_aec_config_init(voice_aec_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(voice_aec_config_t));
    config->sample_rate = 16000;
    config->frame_size = 320;        /* 20ms @ 16kHz */
    config->filter_length = 3200;    /* 200ms @ 16kHz */
    config->echo_suppress_db = -40;
    config->echo_suppress_active_db = -15;
    config->enable_residual_echo_suppress = true;
    config->enable_comfort_noise = false;
}

/* ============================================
 * 回声消除器创建/销毁
 * ============================================ */

voice_aec_t *voice_aec_create(const voice_aec_config_t *config)
{
    if (!config) {
        return NULL;
    }
    
    voice_aec_t *aec = (voice_aec_t *)calloc(1, sizeof(voice_aec_t));
    if (!aec) {
        return NULL;
    }
    
    aec->config = *config;
    aec->enabled = true;
    
    /* 创建回声消除状态 */
    aec->echo_state = speex_echo_state_init(
        config->frame_size,
        config->filter_length
    );
    
    if (!aec->echo_state) {
        VOICE_LOG_E("Failed to create echo state");
        free(aec);
        return NULL;
    }
    
    /* 设置采样率 */
    int sample_rate = config->sample_rate;
    speex_echo_ctl(aec->echo_state, SPEEX_ECHO_SET_SAMPLING_RATE, &sample_rate);
    
    /* 创建预处理器用于残余回声抑制 */
    if (config->enable_residual_echo_suppress) {
        aec->preprocess_state = speex_preprocess_state_init(
            config->frame_size,
            config->sample_rate
        );
        
        if (aec->preprocess_state) {
            /* 关联回声状态到预处理器 */
            speex_preprocess_ctl(aec->preprocess_state,
                SPEEX_PREPROCESS_SET_ECHO_STATE, aec->echo_state);
            
            /* 设置回声抑制参数 */
            int suppress = config->echo_suppress_db;
            speex_preprocess_ctl(aec->preprocess_state,
                SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &suppress);
            
            int suppress_active = config->echo_suppress_active_db;
            speex_preprocess_ctl(aec->preprocess_state,
                SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &suppress_active);
            
            /* 启用去噪 */
            int denoise = 1;
            speex_preprocess_ctl(aec->preprocess_state,
                SPEEX_PREPROCESS_SET_DENOISE, &denoise);
        }
    }
    
    /* 分配临时缓冲区 */
    aec->temp_buffer_size = config->frame_size;
    aec->temp_buffer = (int16_t *)calloc(aec->temp_buffer_size, sizeof(int16_t));
    if (!aec->temp_buffer) {
        speex_echo_state_destroy(aec->echo_state);
        if (aec->preprocess_state) {
            speex_preprocess_state_destroy(aec->preprocess_state);
        }
        free(aec);
        return NULL;
    }
    
    aec->initialized = true;
    VOICE_LOG_I("AEC created: frame_size=%u, filter_length=%u",
        config->frame_size, config->filter_length);
    
    return aec;
}

void voice_aec_destroy(voice_aec_t *aec)
{
    if (!aec) return;
    
    if (aec->echo_state) {
        speex_echo_state_destroy(aec->echo_state);
    }
    
    if (aec->preprocess_state) {
        speex_preprocess_state_destroy(aec->preprocess_state);
    }
    
    if (aec->temp_buffer) {
        free(aec->temp_buffer);
    }
    
    free(aec);
}

/* ============================================
 * 回声消除处理
 * ============================================ */

voice_error_t voice_aec_process(
    voice_aec_t *aec,
    const int16_t *mic_input,
    const int16_t *speaker_ref,
    int16_t *output,
    size_t frame_count)
{
    if (!aec || !aec->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!mic_input || !speaker_ref || !output) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    if (!aec->enabled) {
        /* 禁用时直接复制输入到输出 */
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }
    
    /* 按帧处理 */
    size_t frame_size = aec->config.frame_size;
    size_t offset = 0;
    
    while (offset + frame_size <= frame_count) {
        /* 执行回声消除 */
        speex_echo_cancellation(
            aec->echo_state,
            mic_input + offset,
            speaker_ref + offset,
            output + offset
        );
        
        /* 残余回声抑制和去噪 */
        if (aec->preprocess_state) {
            speex_preprocess_run(aec->preprocess_state, output + offset);
        }
        
        offset += frame_size;
    }
    
    /* 处理剩余样本 */
    if (offset < frame_count) {
        size_t remaining = frame_count - offset;
        
        /* 填充临时缓冲区 */
        memset(aec->temp_buffer, 0, frame_size * sizeof(int16_t));
        memcpy(aec->temp_buffer, mic_input + offset, remaining * sizeof(int16_t));
        
        int16_t ref_temp[frame_size];
        memset(ref_temp, 0, sizeof(ref_temp));
        memcpy(ref_temp, speaker_ref + offset, remaining * sizeof(int16_t));
        
        int16_t out_temp[frame_size];
        speex_echo_cancellation(aec->echo_state, aec->temp_buffer, ref_temp, out_temp);
        
        if (aec->preprocess_state) {
            speex_preprocess_run(aec->preprocess_state, out_temp);
        }
        
        memcpy(output + offset, out_temp, remaining * sizeof(int16_t));
    }
    
    return VOICE_OK;
}

voice_error_t voice_aec_playback(
    voice_aec_t *aec,
    const int16_t *speaker_data,
    size_t frame_count)
{
    if (!aec || !aec->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!speaker_data) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    if (!aec->enabled) {
        return VOICE_OK;
    }
    
    /* 按帧处理 */
    size_t frame_size = aec->config.frame_size;
    size_t offset = 0;
    
    while (offset + frame_size <= frame_count) {
        speex_echo_playback(aec->echo_state, speaker_data + offset);
        offset += frame_size;
    }
    
    return VOICE_OK;
}

voice_error_t voice_aec_capture(
    voice_aec_t *aec,
    const int16_t *mic_input,
    int16_t *output,
    size_t frame_count)
{
    if (!aec || !aec->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!mic_input || !output) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    if (!aec->enabled) {
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }
    
    /* 按帧处理 */
    size_t frame_size = aec->config.frame_size;
    size_t offset = 0;
    
    while (offset + frame_size <= frame_count) {
        speex_echo_capture(aec->echo_state, mic_input + offset, output + offset);
        
        if (aec->preprocess_state) {
            speex_preprocess_run(aec->preprocess_state, output + offset);
        }
        
        offset += frame_size;
    }
    
    /* 处理剩余样本 */
    if (offset < frame_count) {
        memcpy(output + offset, mic_input + offset, 
            (frame_count - offset) * sizeof(int16_t));
    }
    
    return VOICE_OK;
}

/* ============================================
 * 回声消除器控制
 * ============================================ */

voice_error_t voice_aec_set_suppress(
    voice_aec_t *aec,
    int suppress_db,
    int suppress_active_db)
{
    if (!aec || !aec->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    aec->config.echo_suppress_db = suppress_db;
    aec->config.echo_suppress_active_db = suppress_active_db;
    
    if (aec->preprocess_state) {
        speex_preprocess_ctl(aec->preprocess_state,
            SPEEX_PREPROCESS_SET_ECHO_SUPPRESS, &suppress_db);
        speex_preprocess_ctl(aec->preprocess_state,
            SPEEX_PREPROCESS_SET_ECHO_SUPPRESS_ACTIVE, &suppress_active_db);
    }
    
    return VOICE_OK;
}

voice_error_t voice_aec_set_enabled(voice_aec_t *aec, bool enabled)
{
    if (!aec) {
        return VOICE_ERROR_NULL_POINTER;
    }
    aec->enabled = enabled;
    return VOICE_OK;
}

bool voice_aec_is_enabled(voice_aec_t *aec)
{
    return aec && aec->enabled;
}

void voice_aec_reset(voice_aec_t *aec)
{
    if (!aec || !aec->initialized) {
        return;
    }
    
    if (aec->echo_state) {
        speex_echo_state_reset(aec->echo_state);
    }
}

int voice_aec_get_delay(voice_aec_t *aec)
{
    if (!aec || !aec->initialized || !aec->echo_state) {
        return 0;
    }
    
    /* SpeexDSP 没有直接获取延迟的API，返回滤波器长度的一半作为估计 */
    return aec->config.filter_length / 2;
}
