/**
 * @file audio_quality.h
 * @brief Audio quality metrics and MOS estimation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Audio quality assessment tools with MOS (Mean Opinion Score) estimation
 */

#ifndef VOICE_AUDIO_QUALITY_H
#define VOICE_AUDIO_QUALITY_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Type Definitions
 * ============================================ */

typedef struct voice_quality_analyzer_s voice_quality_analyzer_t;

/* ============================================
 * Quality Metrics
 * ============================================ */

typedef struct {
    /* MOS score (1.0-5.0) */
    float mos_lq;               /**< MOS-LQ (Listening Quality) */
    float mos_cq;               /**< MOS-CQ (Conversational Quality) */

    /* Network related */
    float packet_loss_rate;     /**< Packet loss rate (0-1) */
    uint32_t jitter_ms;         /**< Jitter (ms) */
    uint32_t rtt_ms;            /**< Round-trip time (ms) */
    uint32_t one_way_delay_ms;  /**< One-way delay (ms) */

    /* Audio related */
    float snr_db;               /**< Signal-to-noise ratio (dB) */
    float noise_level_db;       /**< Noise level (dB) */
    float signal_level_db;      /**< Signal level (dB) */
    float clipping_rate;        /**< Clipping rate */

    /* Speech related */
    float speech_ratio;         /**< Speech ratio (0-1) */
    uint32_t speech_duration_ms;/**< Speech duration (ms) */

    /* R-Factor (ITU-T G.107) */
    float r_factor;             /**< R value (0-100) */

    /* Problem flags */
    bool has_echo;              /**< Echo detected */
    bool has_noise;             /**< Excessive noise */
    bool has_clipping;          /**< Clipping present */
    bool low_volume;            /**< Volume too low */
} voice_quality_metrics_t;

/* ============================================
 * Configuration
 * ============================================ */

typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    uint32_t analysis_window_ms;    /**< Analysis window (ms) */

    /* Thresholds */
    float noise_threshold_db;       /**< Noise threshold */
    float snr_threshold_db;         /**< SNR threshold */
    float clipping_threshold;       /**< Clipping threshold */
} voice_quality_config_t;

/* ============================================
 * Quality Analyzer API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_quality_config_init(voice_quality_config_t *config);

/**
 * @brief Create quality analyzer
 */
VOICE_API voice_quality_analyzer_t *voice_quality_analyzer_create(
    const voice_quality_config_t *config
);

/**
 * @brief Destroy analyzer
 */
VOICE_API void voice_quality_analyzer_destroy(voice_quality_analyzer_t *analyzer);

/**
 * @brief Analyze audio frame
 */
VOICE_API voice_error_t voice_quality_analyze_frame(
    voice_quality_analyzer_t *analyzer,
    const int16_t *samples,
    size_t num_samples
);

/**
 * @brief Update network metrics
 */
VOICE_API void voice_quality_update_network(
    voice_quality_analyzer_t *analyzer,
    float packet_loss_rate,
    uint32_t jitter_ms,
    uint32_t rtt_ms
);

/**
 * @brief Get quality metrics
 */
VOICE_API voice_error_t voice_quality_get_metrics(
    voice_quality_analyzer_t *analyzer,
    voice_quality_metrics_t *metrics
);

/**
 * @brief Reset analyzer
 */
VOICE_API void voice_quality_reset(voice_quality_analyzer_t *analyzer);

/* ============================================
 * MOS Estimation Functions
 * ============================================ */

/**
 * @brief Calculate R-Factor based on E-Model (ITU-T G.107)
 * @param delay_ms One-way delay (ms)
 * @param packet_loss_pct Packet loss rate (percentage, 0-100)
 * @param codec_ie Codec impairment factor (Opus ~10, G.711 ~0)
 * @return R-Factor (0-100)
 */
VOICE_API float voice_calculate_r_factor(
    uint32_t delay_ms,
    float packet_loss_pct,
    float codec_ie
);

/**
 * @brief Convert R-Factor to MOS
 * @param r_factor R value (0-100)
 * @return MOS score (1.0-5.0)
 */
VOICE_API float voice_r_factor_to_mos(float r_factor);

/**
 * @brief Quick MOS estimation
 */
VOICE_API float voice_estimate_mos(
    uint32_t delay_ms,
    float packet_loss_pct,
    uint32_t jitter_ms
);

/**
 * @brief Get quality description
 */
VOICE_API const char *voice_mos_description(float mos);

/**
 * @brief Get R-Factor description
 */
VOICE_API const char *voice_r_factor_description(float r_factor);

/* ============================================
 * Codec Impairment Factors (Ie)
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
