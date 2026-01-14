/**
 * @file audio_quality.h
 * @brief Audio quality metrics and MOS estimation
 * 
 * 音频质量评估工具，包含 MOS (Mean Opinion Score) 估计
 */

#ifndef VOICE_AUDIO_QUALITY_H
#define VOICE_AUDIO_QUALITY_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 类型定义
 * ============================================ */

typedef struct voice_quality_analyzer_s voice_quality_analyzer_t;

/* ============================================
 * 质量指标
 * ============================================ */

typedef struct {
    /* MOS 分数 (1.0-5.0) */
    float mos_lq;               /**< MOS-LQ (Listening Quality) */
    float mos_cq;               /**< MOS-CQ (Conversational Quality) */
    
    /* 网络相关 */
    float packet_loss_rate;     /**< 丢包率 (0-1) */
    uint32_t jitter_ms;         /**< 抖动 (ms) */
    uint32_t rtt_ms;            /**< 往返延迟 (ms) */
    uint32_t one_way_delay_ms;  /**< 单向延迟 (ms) */
    
    /* 音频相关 */
    float snr_db;               /**< 信噪比 (dB) */
    float noise_level_db;       /**< 噪声电平 (dB) */
    float signal_level_db;      /**< 信号电平 (dB) */
    float clipping_rate;        /**< 削波率 */
    
    /* 语音相关 */
    float speech_ratio;         /**< 语音占比 (0-1) */
    uint32_t speech_duration_ms;/**< 语音时长 (ms) */
    
    /* R-Factor (ITU-T G.107) */
    float r_factor;             /**< R值 (0-100) */
    
    /* 问题标志 */
    bool has_echo;              /**< 检测到回声 */
    bool has_noise;             /**< 噪声过大 */
    bool has_clipping;          /**< 有削波 */
    bool low_volume;            /**< 音量过低 */
} voice_quality_metrics_t;

/* ============================================
 * 配置
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    uint32_t analysis_window_ms;    /**< 分析窗口 (ms) */
    
    /* 阈值 */
    float noise_threshold_db;       /**< 噪声阈值 */
    float snr_threshold_db;         /**< 信噪比阈值 */
    float clipping_threshold;       /**< 削波阈值 */
} voice_quality_config_t;

/* ============================================
 * 质量分析器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_quality_config_init(voice_quality_config_t *config);

/**
 * @brief 创建质量分析器
 */
voice_quality_analyzer_t *voice_quality_analyzer_create(
    const voice_quality_config_t *config
);

/**
 * @brief 销毁分析器
 */
void voice_quality_analyzer_destroy(voice_quality_analyzer_t *analyzer);

/**
 * @brief 分析音频帧
 */
voice_error_t voice_quality_analyze_frame(
    voice_quality_analyzer_t *analyzer,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief 更新网络指标
 */
void voice_quality_update_network(
    voice_quality_analyzer_t *analyzer,
    float packet_loss_rate,
    uint32_t jitter_ms,
    uint32_t rtt_ms
);

/**
 * @brief 获取质量指标
 */
voice_error_t voice_quality_get_metrics(
    voice_quality_analyzer_t *analyzer,
    voice_quality_metrics_t *metrics
);

/**
 * @brief 重置分析器
 */
void voice_quality_reset(voice_quality_analyzer_t *analyzer);

/* ============================================
 * MOS 估计函数
 * ============================================ */

/**
 * @brief 基于 E-Model (ITU-T G.107) 计算 R-Factor
 * @param delay_ms 单向延迟 (ms)
 * @param packet_loss_pct 丢包率 (百分比, 0-100)
 * @param codec_ie 编解码器损伤因子 (Opus约10, G.711约0)
 * @return R-Factor (0-100)
 */
float voice_calculate_r_factor(
    uint32_t delay_ms,
    float packet_loss_pct,
    float codec_ie
);

/**
 * @brief R-Factor 转 MOS
 * @param r_factor R值 (0-100)
 * @return MOS 分数 (1.0-5.0)
 */
float voice_r_factor_to_mos(float r_factor);

/**
 * @brief 快速 MOS 估计
 */
float voice_estimate_mos(
    uint32_t delay_ms,
    float packet_loss_pct,
    uint32_t jitter_ms
);

/**
 * @brief 获取质量描述
 */
const char *voice_mos_description(float mos);

/**
 * @brief 获取 R-Factor 描述
 */
const char *voice_r_factor_description(float r_factor);

/* ============================================
 * 编解码器损伤因子 (Ie)
 * ITU-T G.113 Appendix I
 * ============================================ */

#define VOICE_IE_G711       0.0f    /**< G.711 A/μ-law */
#define VOICE_IE_G722       7.0f    /**< G.722 */
#define VOICE_IE_G729       10.0f   /**< G.729 */
#define VOICE_IE_OPUS       10.0f   /**< Opus (高质量模式) */
#define VOICE_IE_OPUS_VBR   12.0f   /**< Opus VBR */
#define VOICE_IE_AMR_WB     7.0f    /**< AMR-WB */
#define VOICE_IE_SPEEX      11.0f   /**< Speex */

#ifdef __cplusplus
}
#endif

#endif /* VOICE_AUDIO_QUALITY_H */
