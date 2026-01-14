/**
 * @file statistics.h
 * @brief Comprehensive statistics collection
 * 
 * 综合统计收集模块，收集和报告各种音频/网络指标
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
 * 统计收集器
 * ============================================ */

typedef struct voice_stats_collector_s voice_stats_collector_t;

/* ============================================
 * 音频统计
 * ============================================ */

typedef struct {
    /* 电平 */
    float input_level_db;           /**< 输入电平 (dBFS) */
    float output_level_db;          /**< 输出电平 (dBFS) */
    float noise_level_db;           /**< 噪声电平 (dBFS) */
    
    /* 语音活动 */
    float speech_ratio;             /**< 语音占比 (0-1) */
    uint64_t speech_duration_ms;    /**< 总语音时长 (ms) */
    
    /* 处理 */
    float agc_gain_db;              /**< AGC 增益 (dB) */
    bool echo_detected;             /**< 检测到回声 */
    float echo_return_loss_db;      /**< 回声返回损耗 (dB) */
    
    /* 质量 */
    float snr_db;                   /**< 信噪比 (dB) */
    uint32_t clipping_count;        /**< 削波次数 */
    uint32_t underrun_count;        /**< 缓冲区欠载次数 */
    uint32_t overrun_count;         /**< 缓冲区溢出次数 */
} voice_audio_stats_t;

/* ============================================
 * 编解码器统计
 * ============================================ */

typedef struct {
    char codec_name[32];            /**< 编解码器名称 */
    uint32_t sample_rate;           /**< 采样率 */
    uint8_t channels;               /**< 通道数 */
    uint32_t bitrate;               /**< 当前比特率 (bps) */
    
    /* 编码 */
    uint64_t frames_encoded;        /**< 编码帧数 */
    uint64_t bytes_encoded;         /**< 编码字节数 */
    float avg_encode_time_us;       /**< 平均编码时间 (μs) */
    
    /* 解码 */
    uint64_t frames_decoded;        /**< 解码帧数 */
    uint64_t bytes_decoded;         /**< 解码字节数 */
    float avg_decode_time_us;       /**< 平均解码时间 (μs) */
    uint64_t fec_recovered;         /**< FEC 恢复帧数 */
    
    /* DTX */
    bool dtx_enabled;               /**< DTX 是否启用 */
    uint64_t dtx_frames;            /**< DTX 帧数 */
} voice_codec_stats_t;

/* ============================================
 * 网络统计
 * ============================================ */

typedef struct {
    /* 发送 */
    uint64_t packets_sent;          /**< 发送包数 */
    uint64_t bytes_sent;            /**< 发送字节数 */
    uint32_t send_bitrate;          /**< 发送比特率 (bps) */
    
    /* 接收 */
    uint64_t packets_received;      /**< 接收包数 */
    uint64_t bytes_received;        /**< 接收字节数 */
    uint32_t recv_bitrate;          /**< 接收比特率 (bps) */
    
    /* 丢包 */
    uint64_t packets_lost;          /**< 丢失包数 */
    float packet_loss_rate;         /**< 丢包率 (0-1) */
    uint64_t packets_late;          /**< 迟到包数 */
    uint64_t packets_discarded;     /**< 丢弃包数 */
    
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
 */
void voice_stats_config_init(voice_stats_config_t *config);

/**
 * @brief 创建统计收集器
 */
voice_stats_collector_t *voice_stats_collector_create(
    const voice_stats_config_t *config
);

/**
 * @brief 销毁统计收集器
 */
void voice_stats_collector_destroy(voice_stats_collector_t *collector);

/**
 * @brief 更新音频统计
 */
void voice_stats_update_audio(
    voice_stats_collector_t *collector,
    const voice_audio_stats_t *stats
);

/**
 * @brief 更新编解码器统计
 */
void voice_stats_update_codec(
    voice_stats_collector_t *collector,
    const voice_codec_stats_t *stats
);

/**
 * @brief 更新网络统计
 */
void voice_stats_update_network(
    voice_stats_collector_t *collector,
    const voice_network_stats_t *stats
);

/**
 * @brief 记录发送的包
 */
void voice_stats_on_packet_sent(
    voice_stats_collector_t *collector,
    size_t size
);

/**
 * @brief 记录接收的包
 */
void voice_stats_on_packet_received(
    voice_stats_collector_t *collector,
    size_t size,
    uint32_t delay_ms
);

/**
 * @brief 记录丢失的包
 */
void voice_stats_on_packet_lost(
    voice_stats_collector_t *collector,
    uint32_t count
);

/**
 * @brief 获取会话统计
 */
voice_error_t voice_stats_get_session(
    voice_stats_collector_t *collector,
    voice_session_stats_t *stats
);

/**
 * @brief 获取最新快照
 */
voice_error_t voice_stats_get_snapshot(
    voice_stats_collector_t *collector,
    voice_stats_snapshot_t *snapshot
);

/**
 * @brief 获取历史快照
 */
voice_error_t voice_stats_get_history(
    voice_stats_collector_t *collector,
    voice_stats_snapshot_t *snapshots,
    size_t *count
);

/**
 * @brief 重置统计
 */
void voice_stats_reset(voice_stats_collector_t *collector);

/**
 * @brief 导出统计为 JSON
 */
size_t voice_stats_export_json(
    voice_stats_collector_t *collector,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief 打印统计摘要
 */
void voice_stats_print_summary(voice_stats_collector_t *collector);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_STATISTICS_H */
