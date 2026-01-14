/**
 * @file statistics.h
 * @brief Comprehensive statistics collection
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Comprehensive statistics collection module, collects and reports various audio/network metrics
 */

#ifndef VOICE_STATISTICS_H
#define VOICE_STATISTICS_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Statistics Collector
 * ============================================ */

typedef struct voice_stats_collector_s voice_stats_collector_t;

/* ============================================
 * Audio Statistics
 * ============================================ */

typedef struct {
    /* Level */
    float input_level_db;           /**< Input level (dBFS) */
    float output_level_db;          /**< Output level (dBFS) */
    float noise_level_db;           /**< Noise level (dBFS) */
    
    /* Voice activity */
    float speech_ratio;             /**< Speech ratio (0-1) */
    uint64_t speech_duration_ms;    /**< Total speech duration (ms) */
    
    /* Processing */
    float agc_gain_db;              /**< AGC gain (dB) */
    bool echo_detected;             /**< Echo detected */
    float echo_return_loss_db;      /**< Echo return loss (dB) */
    
    /* Quality */
    float snr_db;                   /**< Signal-to-noise ratio (dB) */
    uint32_t clipping_count;        /**< Clipping count */
    uint32_t underrun_count;        /**< Buffer underrun count */
    uint32_t overrun_count;         /**< Buffer overrun count */
} voice_audio_stats_t;

/* ============================================
 * Codec Statistics
 * ============================================ */

typedef struct {
    char codec_name[32];            /**< Codec name */
    uint32_t sample_rate;           /**< Sample rate */
    uint8_t channels;               /**< Number of channels */
    uint32_t bitrate;               /**< Current bitrate (bps) */
    
    /* Encoding */
    uint64_t frames_encoded;        /**< Encoded frames */
    uint64_t bytes_encoded;         /**< Encoded bytes */
    float avg_encode_time_us;       /**< Average encode time (μs) */
    
    /* Decoding */
    uint64_t frames_decoded;        /**< Decoded frames */
    uint64_t bytes_decoded;         /**< Decoded bytes */
    float avg_decode_time_us;       /**< Average decode time (μs) */
    uint64_t fec_recovered;         /**< FEC recovered frames */
    
    /* DTX */
    bool dtx_enabled;               /**< DTX enabled */
    uint64_t dtx_frames;            /**< DTX frames */
} voice_codec_stats_t;

/* ============================================
 * Network Statistics
 * ============================================ */

typedef struct {
    /* Sending */
    uint64_t packets_sent;          /**< Packets sent */
    uint64_t bytes_sent;            /**< Bytes sent */
    uint32_t send_bitrate;          /**< Send bitrate (bps) */
    
    /* Receiving */
    uint64_t packets_received;      /**< Packets received */
    uint64_t bytes_received;        /**< Bytes received */
    uint32_t recv_bitrate;          /**< Receive bitrate (bps) */
    
    /* Packet loss */
    uint64_t packets_lost;          /**< Lost packets */
    float packet_loss_rate;         /**< Packet loss rate (0-1) */
    uint64_t packets_late;          /**< Late packets */
    uint64_t packets_discarded;     /**< Discarded packets */
    
    /* 延迟 */
    uint32_t rtt_ms;                /**< 往返延迟 (ms) */
    uint32_t rtt_min_ms;            /**< 最小 RTT */
    uint32_t rtt_max_ms;            /**< 最大 RTT */
    uint32_t jitter_ms;             /**< 抖动 (ms) */
    
    /* 抖动缓冲 */
    uint32_t jitter_buffer_ms;      /**< 抖动缓冲大小 (ms) */
    uint32_t jitter_buffer_target_ms;/**< 目标大小 */
    uint64_t plc_count;             /**< PLC 触发次数 */
    
    /* 带宽估计 */
    uint32_t estimated_bandwidth;   /**< 估计带宽 (bps) */
    uint32_t target_bitrate;        /**< 目标比特率 (bps) */
} voice_network_stats_t;

/* ============================================
 * 会话统计
 * ============================================ */

typedef struct {
    /* 时间 */
    uint64_t session_duration_ms;   /**< 会话时长 (ms) */
    uint64_t start_timestamp;       /**< 开始时间戳 */
    
    /* 质量 */
    float mos_lq;                   /**< MOS-LQ (1.0-5.0) */
    float mos_cq;                   /**< MOS-CQ (1.0-5.0) */
    float r_factor;                 /**< R-Factor (0-100) */
    
    /* 聚合统计 */
    voice_audio_stats_t audio;
    voice_codec_stats_t codec;
    voice_network_stats_t network;
} voice_session_stats_t;

/* ============================================
 * 统计快照 (用于周期性报告)
 * ============================================ */

typedef struct {
    uint64_t timestamp;             /**< 快照时间戳 */
    uint32_t interval_ms;           /**< 统计间隔 */
    
    /* 间隔内的统计 */
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    float avg_rtt_ms;
    float avg_jitter_ms;
    float avg_mos;
} voice_stats_snapshot_t;

/* ============================================
 * 统计收集器配置
 * ============================================ */

typedef struct {
    uint32_t snapshot_interval_ms;  /**< 快照间隔 (ms) */
    size_t history_size;            /**< 历史记录大小 */
    bool enable_detailed_timing;    /**< 启用详细时间统计 */
    
    /* 回调 */
    void (*on_snapshot)(const voice_stats_snapshot_t *snapshot, void *user_data);
    void (*on_quality_change)(float old_mos, float new_mos, void *user_data);
    void *callback_user_data;
} voice_stats_config_t;

/* ============================================
 * 统计收集器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_config_init(voice_stats_config_t *config);

/**
 * @brief 创建统计收集器
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_stats_collector_t *voice_stats_collector_create(
    const voice_stats_config_t *config
);

/**
 * @brief 销毁统计收集器
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_collector_destroy(voice_stats_collector_t *collector);

/**
 * @brief 更新音频统计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_update_audio(
    voice_stats_collector_t *collector,
    const voice_audio_stats_t *stats
);

/**
 * @brief 更新编解码器统计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_update_codec(
    voice_stats_collector_t *collector,
    const voice_codec_stats_t *stats
);

/**
 * @brief 更新网络统计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_update_network(
    voice_stats_collector_t *collector,
    const voice_network_stats_t *stats
);

/**
 * @brief 记录发送的包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_on_packet_sent(
    voice_stats_collector_t *collector,
    size_t size
);

/**
 * @brief 记录接收的包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_on_packet_received(
    voice_stats_collector_t *collector,
    size_t size,
    uint32_t delay_ms
);

/**
 * @brief 记录丢失的包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_on_packet_lost(
    voice_stats_collector_t *collector,
    uint32_t count
);

/**
 * @brief 获取会话统计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_stats_get_session(
    voice_stats_collector_t *collector,
    voice_session_stats_t *stats
);

/**
 * @brief 获取最新快照
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_stats_get_snapshot(
    voice_stats_collector_t *collector,
    voice_stats_snapshot_t *snapshot
);

/**
 * @brief 获取历史快照
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_stats_get_history(
    voice_stats_collector_t *collector,
    voice_stats_snapshot_t *snapshots,
    size_t *count
);

/**
 * @brief 重置统计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_reset(voice_stats_collector_t *collector);

/**
 * @brief 导出统计为 JSON
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
size_t voice_stats_export_json(
    voice_stats_collector_t *collector,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief 打印统计摘要
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_stats_print_summary(voice_stats_collector_t *collector);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_STATISTICS_H */
