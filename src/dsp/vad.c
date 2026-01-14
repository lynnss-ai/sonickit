/**
 * @file vad.c
 * @brief Voice Activity Detection implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/vad.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_vad_s {
    voice_vad_config_t config;
    voice_vad_mode_t mode;
    
    /* 状态 */
    bool last_speech;
    uint32_t speech_frames;
    uint32_t silence_frames;
    uint32_t hangover_frames;       /**< 语音结束后的延迟帧数 */
    uint32_t hangover_counter;      /**< 当前延迟计数 */
    
    /* 能量 VAD 状态 */
    float energy_threshold;         /**< 当前能量阈值 */
    float noise_estimate;           /**< 噪声估计 */
    float signal_estimate;          /**< 信号估计 */
    
    /* 自适应 */
    float adaptation_rate;
    float long_term_energy;
    
    /* 统计 */
    voice_vad_stats_t stats;
    float sum_probability;
    
#ifdef USE_SPEEXDSP
    void *speex_preprocess;         /**< SpeexDSP 预处理状态 */
#endif
};

/* ============================================
 * 辅助函数
 * ============================================ */

static float compute_energy_db(const int16_t *samples, size_t count) {
    if (count == 0) return -96.0f;
    
    int64_t sum_sq = 0;
    for (size_t i = 0; i < count; i++) {
        sum_sq += (int32_t)samples[i] * (int32_t)samples[i];
    }
    
    float energy = (float)sum_sq / (float)count;
    if (energy <= 0.0f) return -96.0f;
    
    /* 转换到 dB */
    float db = 10.0f * log10f(energy) - 10.0f * log10f(32768.0f * 32768.0f);
    return db < -96.0f ? -96.0f : db;
}

static float compute_zero_crossing_rate(const int16_t *samples, size_t count) {
    if (count < 2) return 0.0f;
    
    size_t crossings = 0;
    for (size_t i = 1; i < count; i++) {
        if ((samples[i-1] >= 0 && samples[i] < 0) ||
            (samples[i-1] < 0 && samples[i] >= 0)) {
            crossings++;
        }
    }
    
    return (float)crossings / (float)(count - 1);
}

/* ============================================
 * 配置
 * ============================================ */

void voice_vad_config_init(voice_vad_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_vad_config_t));
    
    config->mode = VOICE_VAD_ENERGY;
    config->aggressiveness = VOICE_VAD_LOW_BITRATE;
    config->sample_rate = 16000;
    config->frame_size_ms = 20;
    
    config->energy_threshold_db = -30.0f;
    config->speech_hangover_ms = 200.0f;
    
    config->adaptive_threshold = true;
    config->adaptation_rate = 0.01f;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_vad_t *voice_vad_create(const voice_vad_config_t *config) {
    voice_vad_t *vad = (voice_vad_t *)calloc(1, sizeof(voice_vad_t));
    if (!vad) return NULL;
    
    if (config) {
        vad->config = *config;
    } else {
        voice_vad_config_init(&vad->config);
    }
    
    vad->mode = vad->config.mode;
    vad->energy_threshold = vad->config.energy_threshold_db;
    vad->adaptation_rate = vad->config.adaptation_rate;
    
    /* 计算 hangover 帧数 */
    float frames_per_second = 1000.0f / vad->config.frame_size_ms;
    vad->hangover_frames = (uint32_t)(vad->config.speech_hangover_ms / 1000.0f * frames_per_second);
    if (vad->hangover_frames < 1) vad->hangover_frames = 1;
    
    /* 初始化噪声估计为较低值 */
    vad->noise_estimate = -50.0f;
    vad->signal_estimate = -30.0f;
    
#ifdef USE_SPEEXDSP
    if (vad->mode == VOICE_VAD_SPEEX) {
        /* 初始化 SpeexDSP VAD */
        uint32_t frame_size = vad->config.sample_rate * vad->config.frame_size_ms / 1000;
        vad->speex_preprocess = speex_preprocess_state_init(frame_size, vad->config.sample_rate);
        
        if (vad->speex_preprocess) {
            int val = 1;
            speex_preprocess_ctl(vad->speex_preprocess, SPEEX_PREPROCESS_SET_VAD, &val);
            
            /* 设置 VAD 概率 */
            int prob = 50 + (int)vad->config.aggressiveness * 15;
            speex_preprocess_ctl(vad->speex_preprocess, SPEEX_PREPROCESS_SET_PROB_START, &prob);
        }
    }
#endif
    
    return vad;
}

void voice_vad_destroy(voice_vad_t *vad) {
    if (!vad) return;
    
#ifdef USE_SPEEXDSP
    if (vad->speex_preprocess) {
        speex_preprocess_state_destroy(vad->speex_preprocess);
    }
#endif
    
    free(vad);
}

/* ============================================
 * 能量 VAD 处理
 * ============================================ */

static bool process_energy_vad(
    voice_vad_t *vad,
    const int16_t *samples,
    size_t num_samples,
    voice_vad_result_t *result
) {
    float energy_db = compute_energy_db(samples, num_samples);
    result->energy_db = energy_db;
    
    /* 自适应阈值更新 */
    if (vad->config.adaptive_threshold) {
        if (!vad->last_speech) {
            /* 在静音期间更新噪声估计 */
            vad->noise_estimate = vad->noise_estimate * (1.0f - vad->adaptation_rate) +
                                  energy_db * vad->adaptation_rate;
        } else {
            /* 在语音期间更新信号估计 */
            vad->signal_estimate = vad->signal_estimate * (1.0f - vad->adaptation_rate) +
                                   energy_db * vad->adaptation_rate;
        }
        
        /* 计算自适应阈值 */
        vad->energy_threshold = vad->noise_estimate + 
            (vad->signal_estimate - vad->noise_estimate) * 0.3f;
        
        /* 确保阈值在合理范围内 */
        if (vad->energy_threshold < vad->config.energy_threshold_db) {
            vad->energy_threshold = vad->config.energy_threshold_db;
        }
    }
    
    /* 检测语音 */
    bool speech_detected = energy_db > vad->energy_threshold;
    
    /* Zero-crossing rate 辅助判断 */
    float zcr = compute_zero_crossing_rate(samples, num_samples);
    
    /* 语音通常 ZCR 在 0.1-0.5 之间 */
    if (zcr > 0.7f) {
        /* 高 ZCR 可能是噪声 */
        speech_detected = false;
    }
    
    /* 计算语音概率 */
    float snr = energy_db - vad->noise_estimate;
    result->speech_probability = 1.0f / (1.0f + expf(-(snr - 10.0f) / 5.0f));
    
    return speech_detected;
}

/* ============================================
 * VAD 处理
 * ============================================ */

voice_error_t voice_vad_process(
    voice_vad_t *vad,
    const int16_t *samples,
    size_t num_samples,
    voice_vad_result_t *result
) {
    if (!vad || !samples || !result) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    memset(result, 0, sizeof(voice_vad_result_t));
    
    bool speech_detected = false;
    
    switch (vad->mode) {
        case VOICE_VAD_ENERGY:
        default:
            speech_detected = process_energy_vad(vad, samples, num_samples, result);
            break;
            
        case VOICE_VAD_SPEEX:
#ifdef USE_SPEEXDSP
            if (vad->speex_preprocess) {
                /* 复制样本（SpeexDSP可能会修改） */
                int16_t *temp = (int16_t *)malloc(num_samples * sizeof(int16_t));
                if (temp) {
                    memcpy(temp, samples, num_samples * sizeof(int16_t));
                    speech_detected = speex_preprocess_run(vad->speex_preprocess, temp) != 0;
                    free(temp);
                }
                result->energy_db = compute_energy_db(samples, num_samples);
                result->speech_probability = speech_detected ? 0.8f : 0.2f;
            } else {
                speech_detected = process_energy_vad(vad, samples, num_samples, result);
            }
#else
            speech_detected = process_energy_vad(vad, samples, num_samples, result);
#endif
            break;
            
        case VOICE_VAD_RNNOISE:
            /* RNNoise VAD 需要集成到 denoiser 模块 */
            speech_detected = process_energy_vad(vad, samples, num_samples, result);
            break;
    }
    
    /* Hangover 逻辑 */
    if (speech_detected) {
        vad->hangover_counter = vad->hangover_frames;
        vad->speech_frames++;
        vad->silence_frames = 0;
    } else if (vad->hangover_counter > 0) {
        vad->hangover_counter--;
        speech_detected = true;  /* 延迟期间仍报告为语音 */
        vad->speech_frames++;
        vad->silence_frames = 0;
    } else {
        vad->silence_frames++;
        vad->speech_frames = 0;
    }
    
    /* 更新结果 */
    result->is_speech = speech_detected;
    result->speech_frames = vad->speech_frames;
    result->silence_frames = vad->silence_frames;
    
    /* 更新统计 */
    vad->stats.total_frames++;
    if (speech_detected) {
        vad->stats.speech_frames++;
    } else {
        vad->stats.silence_frames++;
    }
    vad->sum_probability += result->speech_probability;
    
    vad->last_speech = speech_detected;
    
    return VOICE_SUCCESS;
}

/* ============================================
 * 其他 API
 * ============================================ */

void voice_vad_reset(voice_vad_t *vad) {
    if (!vad) return;
    
    vad->last_speech = false;
    vad->speech_frames = 0;
    vad->silence_frames = 0;
    vad->hangover_counter = 0;
    
    vad->noise_estimate = -50.0f;
    vad->signal_estimate = -30.0f;
    vad->energy_threshold = vad->config.energy_threshold_db;
    
    memset(&vad->stats, 0, sizeof(vad->stats));
    vad->sum_probability = 0.0f;
}

voice_error_t voice_vad_set_aggressiveness(
    voice_vad_t *vad,
    voice_vad_aggressiveness_t aggressiveness
) {
    if (!vad) return VOICE_ERROR_INVALID_PARAM;
    
    vad->config.aggressiveness = aggressiveness;
    
    /* 根据激进程度调整参数 */
    switch (aggressiveness) {
        case VOICE_VAD_QUALITY:
            vad->config.energy_threshold_db = -35.0f;
            vad->config.speech_hangover_ms = 300.0f;
            break;
            
        case VOICE_VAD_LOW_BITRATE:
            vad->config.energy_threshold_db = -30.0f;
            vad->config.speech_hangover_ms = 200.0f;
            break;
            
        case VOICE_VAD_AGGRESSIVE:
            vad->config.energy_threshold_db = -25.0f;
            vad->config.speech_hangover_ms = 100.0f;
            break;
            
        case VOICE_VAD_VERY_AGGRESSIVE:
            vad->config.energy_threshold_db = -20.0f;
            vad->config.speech_hangover_ms = 50.0f;
            break;
    }
    
    /* 重新计算 hangover 帧数 */
    float frames_per_second = 1000.0f / vad->config.frame_size_ms;
    vad->hangover_frames = (uint32_t)(vad->config.speech_hangover_ms / 1000.0f * frames_per_second);
    
#ifdef USE_SPEEXDSP
    if (vad->speex_preprocess) {
        int prob = 50 + (int)aggressiveness * 15;
        speex_preprocess_ctl(vad->speex_preprocess, SPEEX_PREPROCESS_SET_PROB_START, &prob);
    }
#endif
    
    return VOICE_SUCCESS;
}

voice_error_t voice_vad_get_stats(
    voice_vad_t *vad,
    voice_vad_stats_t *stats
) {
    if (!vad || !stats) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    *stats = vad->stats;
    
    if (vad->stats.total_frames > 0) {
        stats->speech_ratio = (float)vad->stats.speech_frames / (float)vad->stats.total_frames;
        stats->average_speech_probability = vad->sum_probability / (float)vad->stats.total_frames;
    }
    
    return VOICE_SUCCESS;
}
