/**
 * @file audio_quality.c
 * @brief Audio quality metrics and MOS estimation implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/audio_quality.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_quality_analyzer_s {
    voice_quality_config_t config;
    
    /* 累积统计 */
    uint64_t total_frames;
    uint64_t speech_frames;
    uint64_t clip_frames;
    
    /* 能量统计 */
    float sum_signal_energy;
    float sum_noise_energy;
    float peak_level;
    
    /* 网络指标 (外部更新) */
    float packet_loss_rate;
    uint32_t jitter_ms;
    uint32_t rtt_ms;
    
    /* 滑动窗口 */
    float recent_snr_db;
    float recent_noise_db;
    float recent_signal_db;
};

/* ============================================
 * 配置
 * ============================================ */

void voice_quality_config_init(voice_quality_config_t *config) {
    if (!config) return;
    
    config->sample_rate = 16000;
    config->frame_size = 320;
    config->analysis_window_ms = 5000;
    config->noise_threshold_db = -50.0f;
    config->snr_threshold_db = 15.0f;
    config->clipping_threshold = 0.99f;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_quality_analyzer_t *voice_quality_analyzer_create(
    const voice_quality_config_t *config
) {
    voice_quality_analyzer_t *analyzer = 
        (voice_quality_analyzer_t *)calloc(1, sizeof(voice_quality_analyzer_t));
    
    if (!analyzer) return NULL;
    
    if (config) {
        analyzer->config = *config;
    } else {
        voice_quality_config_init(&analyzer->config);
    }
    
    return analyzer;
}

void voice_quality_analyzer_destroy(voice_quality_analyzer_t *analyzer) {
    if (analyzer) {
        free(analyzer);
    }
}

/* ============================================
 * 辅助函数
 * ============================================ */

static float linear_to_db(float linear) {
    if (linear <= 0.0f) return -96.0f;
    float db = 20.0f * log10f(linear);
    return db < -96.0f ? -96.0f : db;
}

static float compute_rms(const int16_t *samples, size_t count) {
    if (count == 0) return 0.0f;
    
    int64_t sum_sq = 0;
    for (size_t i = 0; i < count; i++) {
        sum_sq += (int32_t)samples[i] * (int32_t)samples[i];
    }
    
    return sqrtf((float)sum_sq / (float)count) / 32768.0f;
}

static float compute_peak(const int16_t *samples, size_t count) {
    int16_t peak = 0;
    for (size_t i = 0; i < count; i++) {
        int16_t abs_val = samples[i] < 0 ? -samples[i] : samples[i];
        if (abs_val > peak) peak = abs_val;
    }
    return (float)peak / 32768.0f;
}

/* ============================================
 * 分析
 * ============================================ */

voice_error_t voice_quality_analyze_frame(
    voice_quality_analyzer_t *analyzer,
    const int16_t *samples,
    size_t num_samples
) {
    if (!analyzer || !samples || num_samples == 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    float rms = compute_rms(samples, num_samples);
    float peak = compute_peak(samples, num_samples);
    float rms_db = linear_to_db(rms);
    
    analyzer->total_frames++;
    
    /* 更新峰值 */
    if (peak > analyzer->peak_level) {
        analyzer->peak_level = peak;
    }
    
    /* 检测削波 */
    if (peak > analyzer->config.clipping_threshold) {
        analyzer->clip_frames++;
    }
    
    /* 简单 VAD: RMS > -40dB 认为是语音 */
    if (rms_db > -40.0f) {
        analyzer->speech_frames++;
        analyzer->sum_signal_energy += rms * rms;
    } else {
        /* 噪声帧 */
        analyzer->sum_noise_energy += rms * rms;
    }
    
    /* 更新滑动统计 */
    analyzer->recent_signal_db = rms_db;
    
    return VOICE_SUCCESS;
}

void voice_quality_update_network(
    voice_quality_analyzer_t *analyzer,
    float packet_loss_rate,
    uint32_t jitter_ms,
    uint32_t rtt_ms
) {
    if (!analyzer) return;
    
    analyzer->packet_loss_rate = packet_loss_rate;
    analyzer->jitter_ms = jitter_ms;
    analyzer->rtt_ms = rtt_ms;
}

voice_error_t voice_quality_get_metrics(
    voice_quality_analyzer_t *analyzer,
    voice_quality_metrics_t *metrics
) {
    if (!analyzer || !metrics) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    memset(metrics, 0, sizeof(voice_quality_metrics_t));
    
    /* 网络指标 */
    metrics->packet_loss_rate = analyzer->packet_loss_rate;
    metrics->jitter_ms = analyzer->jitter_ms;
    metrics->rtt_ms = analyzer->rtt_ms;
    metrics->one_way_delay_ms = analyzer->rtt_ms / 2;
    
    /* 音频指标 */
    if (analyzer->total_frames > 0) {
        metrics->speech_ratio = (float)analyzer->speech_frames / (float)analyzer->total_frames;
        metrics->clipping_rate = (float)analyzer->clip_frames / (float)analyzer->total_frames;
        
        /* 计算 SNR */
        float avg_signal = analyzer->speech_frames > 0 ?
            sqrtf(analyzer->sum_signal_energy / analyzer->speech_frames) : 0.0f;
        float avg_noise = (analyzer->total_frames - analyzer->speech_frames) > 0 ?
            sqrtf(analyzer->sum_noise_energy / (analyzer->total_frames - analyzer->speech_frames)) : 0.0001f;
        
        metrics->signal_level_db = linear_to_db(avg_signal);
        metrics->noise_level_db = linear_to_db(avg_noise);
        metrics->snr_db = metrics->signal_level_db - metrics->noise_level_db;
    }
    
    /* 计算 R-Factor 和 MOS */
    metrics->r_factor = voice_calculate_r_factor(
        metrics->one_way_delay_ms,
        metrics->packet_loss_rate * 100.0f,
        VOICE_IE_OPUS
    );
    
    metrics->mos_lq = voice_r_factor_to_mos(metrics->r_factor);
    
    /* MOS-CQ 考虑往返延迟的额外影响 */
    float cq_penalty = 0.0f;
    if (metrics->rtt_ms > 400) {
        cq_penalty = (metrics->rtt_ms - 400) * 0.003f;
    }
    metrics->mos_cq = metrics->mos_lq - cq_penalty;
    if (metrics->mos_cq < 1.0f) metrics->mos_cq = 1.0f;
    
    /* 语音时长 */
    uint32_t ms_per_frame = (analyzer->config.frame_size * 1000) / 
                            analyzer->config.sample_rate;
    metrics->speech_duration_ms = (uint32_t)(analyzer->speech_frames * ms_per_frame);
    
    /* 问题检测 */
    metrics->has_noise = metrics->snr_db < analyzer->config.snr_threshold_db;
    metrics->has_clipping = metrics->clipping_rate > 0.01f;
    metrics->low_volume = metrics->signal_level_db < -30.0f;
    
    return VOICE_SUCCESS;
}

void voice_quality_reset(voice_quality_analyzer_t *analyzer) {
    if (!analyzer) return;
    
    analyzer->total_frames = 0;
    analyzer->speech_frames = 0;
    analyzer->clip_frames = 0;
    analyzer->sum_signal_energy = 0.0f;
    analyzer->sum_noise_energy = 0.0f;
    analyzer->peak_level = 0.0f;
}

/* ============================================
 * E-Model (ITU-T G.107) 实现
 * ============================================ */

float voice_calculate_r_factor(
    uint32_t delay_ms,
    float packet_loss_pct,
    float codec_ie
) {
    /* R = R0 - Is - Id - Ie + A
     * 
     * R0 = 基础信号与噪声比 (通常约 93.2)
     * Is = 同时损伤因子 (通常较小)
     * Id = 延迟损伤因子
     * Ie = 设备损伤因子 (包含编解码器和丢包)
     * A = 优势因子
     */
    
    const float R0 = 93.2f;
    const float Is = 1.41f;  /* 典型值 */
    const float A = 0.0f;    /* 普通 VoIP */
    
    /* 延迟损伤 (Id) */
    float Id = 0.0f;
    if (delay_ms > 100) {
        float Ta = (float)delay_ms;
        /* 简化的 Id 计算 */
        if (Ta < 177.3f) {
            Id = 0.024f * Ta + 0.11f * (Ta - 177.3f) * 
                 ((Ta > 177.3f) ? 1.0f : 0.0f);
        } else {
            Id = 0.024f * Ta + 0.11f * (Ta - 177.3f);
        }
    }
    
    /* 设备损伤 (Ie-eff) - 包含编解码器和丢包效应 */
    float Ie_eff = codec_ie;
    
    /* 丢包对 Ie 的影响 (简化模型) */
    if (packet_loss_pct > 0) {
        float Bpl = 10.0f;  /* 丢包鲁棒性因子, Opus 约 10 */
        float Ppl = packet_loss_pct;
        Ie_eff = codec_ie + (95.0f - codec_ie) * 
                 (Ppl / (Ppl + Bpl));
    }
    
    /* 计算 R */
    float R = R0 - Is - Id - Ie_eff + A;
    
    /* 限制范围 */
    if (R < 0.0f) R = 0.0f;
    if (R > 100.0f) R = 100.0f;
    
    return R;
}

float voice_r_factor_to_mos(float r_factor) {
    float R = r_factor;
    float MOS;
    
    if (R < 0.0f) {
        MOS = 1.0f;
    } else if (R > 100.0f) {
        MOS = 4.5f;
    } else {
        MOS = 1.0f + 0.035f * R + R * (R - 60.0f) * 
              (100.0f - R) * 7.0e-6f;
    }
    
    if (MOS < 1.0f) MOS = 1.0f;
    if (MOS > 5.0f) MOS = 5.0f;
    
    return MOS;
}

float voice_estimate_mos(
    uint32_t delay_ms,
    float packet_loss_pct,
    uint32_t jitter_ms
) {
    /* 考虑抖动对有效丢包的影响 */
    float effective_loss = packet_loss_pct;
    if (jitter_ms > 50) {
        effective_loss += (jitter_ms - 50) * 0.05f;
    }
    
    float r = voice_calculate_r_factor(delay_ms, effective_loss, VOICE_IE_OPUS);
    return voice_r_factor_to_mos(r);
}

const char *voice_mos_description(float mos) {
    if (mos >= 4.3f) return "Excellent";
    if (mos >= 4.0f) return "Good";
    if (mos >= 3.6f) return "Fair";
    if (mos >= 3.1f) return "Poor";
    if (mos >= 2.6f) return "Bad";
    return "Very Bad";
}

const char *voice_r_factor_description(float r_factor) {
    if (r_factor >= 90.0f) return "Excellent";
    if (r_factor >= 80.0f) return "High";
    if (r_factor >= 70.0f) return "Medium";
    if (r_factor >= 60.0f) return "Low";
    if (r_factor >= 50.0f) return "Poor";
    return "Very Poor";
}
