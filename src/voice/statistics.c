/**
 * @file statistics.c
 * @brief Voice processing statistics collection implementation
 */

#include "voice/statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_stats_collector_s {
    voice_stats_config_t config;
    
    /* 当前统计数据 */
    voice_audio_stats_t audio_stats;
    voice_codec_stats_t codec_stats;
    voice_network_stats_t network_stats;
    voice_session_stats_t session_stats;
    
    /* 快照历史 */
    voice_stats_snapshot_t *snapshots;
    size_t snapshot_count;
    size_t snapshot_capacity;
    
    /* 时间 */
    uint64_t start_time;
    uint64_t last_update;
    
    /* 计算中间值 */
    float jitter_sum;
    float jitter_sq_sum;
    uint32_t jitter_count;
    
    int32_t rtt_sum;
    int32_t rtt_count;
    
    /* MOS 历史 */
    float mos_sum;
    float mos_count;
};

/* ============================================
 * 时间函数
 * ============================================ */

static uint64_t get_timestamp_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)(count.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/* ============================================
 * 配置
 * ============================================ */

void voice_stats_config_init(voice_stats_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_stats_config_t));
    
    config->snapshot_interval_ms = 1000;
    config->max_snapshots = 60;
    
    config->enable_audio_stats = true;
    config->enable_codec_stats = true;
    config->enable_network_stats = true;
    
    config->auto_calculate_quality = true;
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_stats_collector_t *voice_stats_collector_create(
    const voice_stats_config_t *config
) {
    voice_stats_collector_t *collector = (voice_stats_collector_t *)calloc(1, sizeof(voice_stats_collector_t));
    if (!collector) return NULL;
    
    if (config) {
        collector->config = *config;
    } else {
        voice_stats_config_init(&collector->config);
    }
    
    /* 分配快照缓冲区 */
    if (collector->config.max_snapshots > 0) {
        collector->snapshots = (voice_stats_snapshot_t *)calloc(
            collector->config.max_snapshots,
            sizeof(voice_stats_snapshot_t)
        );
        collector->snapshot_capacity = collector->config.max_snapshots;
    }
    
    collector->start_time = get_timestamp_ms();
    collector->last_update = collector->start_time;
    
    return collector;
}

void voice_stats_collector_destroy(voice_stats_collector_t *collector) {
    if (collector) {
        free(collector->snapshots);
        free(collector);
    }
}

/* ============================================
 * 统计更新
 * ============================================ */

void voice_stats_update_audio(
    voice_stats_collector_t *collector,
    const voice_audio_stats_t *stats
) {
    if (!collector || !stats) return;
    
    collector->audio_stats = *stats;
    collector->audio_stats.timestamp = get_timestamp_ms();
}

void voice_stats_update_codec(
    voice_stats_collector_t *collector,
    const voice_codec_stats_t *stats
) {
    if (!collector || !stats) return;
    
    collector->codec_stats = *stats;
    collector->codec_stats.timestamp = get_timestamp_ms();
}

void voice_stats_update_network(
    voice_stats_collector_t *collector,
    const voice_network_stats_t *stats
) {
    if (!collector || !stats) return;
    
    collector->network_stats = *stats;
    collector->network_stats.timestamp = get_timestamp_ms();
    
    /* 更新 jitter 统计 */
    collector->jitter_sum += stats->jitter_avg;
    collector->jitter_sq_sum += stats->jitter_avg * stats->jitter_avg;
    collector->jitter_count++;
}

void voice_stats_add_packet(
    voice_stats_collector_t *collector,
    size_t size,
    bool is_sent
) {
    if (!collector) return;
    
    if (is_sent) {
        collector->network_stats.packets_sent++;
        collector->network_stats.bytes_sent += size;
    } else {
        collector->network_stats.packets_received++;
        collector->network_stats.bytes_received += size;
    }
}

void voice_stats_add_loss(voice_stats_collector_t *collector, uint32_t count) {
    if (!collector) return;
    
    collector->network_stats.packets_lost += count;
    
    /* 更新丢包率 */
    uint32_t total = collector->network_stats.packets_received + 
                     collector->network_stats.packets_lost;
    if (total > 0) {
        collector->network_stats.loss_rate = 
            (float)collector->network_stats.packets_lost / (float)total * 100.0f;
    }
}

void voice_stats_add_rtt(voice_stats_collector_t *collector, uint32_t rtt_ms) {
    if (!collector) return;
    
    collector->network_stats.rtt = rtt_ms;
    
    /* 更新 min/max */
    if (rtt_ms < collector->network_stats.rtt_min || 
        collector->network_stats.rtt_min == 0) {
        collector->network_stats.rtt_min = rtt_ms;
    }
    if (rtt_ms > collector->network_stats.rtt_max) {
        collector->network_stats.rtt_max = rtt_ms;
    }
    
    /* 更新平均值 */
    collector->rtt_sum += rtt_ms;
    collector->rtt_count++;
}

/* ============================================
 * 统计获取
 * ============================================ */

voice_error_t voice_stats_get_audio(
    voice_stats_collector_t *collector,
    voice_audio_stats_t *stats
) {
    if (!collector || !stats) return VOICE_ERROR_INVALID_PARAM;
    
    *stats = collector->audio_stats;
    return VOICE_SUCCESS;
}

voice_error_t voice_stats_get_codec(
    voice_stats_collector_t *collector,
    voice_codec_stats_t *stats
) {
    if (!collector || !stats) return VOICE_ERROR_INVALID_PARAM;
    
    *stats = collector->codec_stats;
    return VOICE_SUCCESS;
}

voice_error_t voice_stats_get_network(
    voice_stats_collector_t *collector,
    voice_network_stats_t *stats
) {
    if (!collector || !stats) return VOICE_ERROR_INVALID_PARAM;
    
    *stats = collector->network_stats;
    return VOICE_SUCCESS;
}

voice_error_t voice_stats_get_session(
    voice_stats_collector_t *collector,
    voice_session_stats_t *stats
) {
    if (!collector || !stats) return VOICE_ERROR_INVALID_PARAM;
    
    uint64_t now = get_timestamp_ms();
    
    collector->session_stats.duration_ms = (uint32_t)(now - collector->start_time);
    collector->session_stats.total_packets_sent = collector->network_stats.packets_sent;
    collector->session_stats.total_packets_received = collector->network_stats.packets_received;
    collector->session_stats.total_packets_lost = collector->network_stats.packets_lost;
    
    /* 计算平均 MOS */
    if (collector->mos_count > 0) {
        collector->session_stats.avg_mos = collector->mos_sum / collector->mos_count;
    }
    
    /* 计算平均丢包率 */
    uint32_t total = collector->session_stats.total_packets_received + 
                     collector->session_stats.total_packets_lost;
    if (total > 0) {
        collector->session_stats.avg_loss_rate = 
            (float)collector->session_stats.total_packets_lost / (float)total * 100.0f;
    }
    
    /* 计算平均 RTT */
    if (collector->rtt_count > 0) {
        collector->session_stats.avg_rtt = collector->rtt_sum / collector->rtt_count;
    }
    
    /* 计算平均 jitter */
    if (collector->jitter_count > 0) {
        collector->session_stats.avg_jitter = 
            collector->jitter_sum / collector->jitter_count;
    }
    
    /* 计算质量评分 */
    if (collector->config.auto_calculate_quality) {
        /* 基于 E-Model 的简化计算 */
        float R = 93.2f;
        R -= collector->session_stats.avg_loss_rate * 2.5f;      /* 丢包影响 */
        R -= collector->session_stats.avg_jitter * 0.2f;          /* 抖动影响 */
        R -= collector->session_stats.avg_rtt * 0.01f;            /* 延迟影响 */
        
        if (R < 0) R = 0;
        if (R > 100) R = 100;
        
        collector->session_stats.r_factor = R;
        
        /* MOS 转换 */
        if (R < 0) {
            collector->session_stats.mos = 1.0f;
        } else if (R > 100) {
            collector->session_stats.mos = 4.5f;
        } else {
            collector->session_stats.mos = 1.0f + 0.035f * R + 
                R * (R - 60.0f) * (100.0f - R) * 7.0e-6f;
        }
        
        /* 确定质量等级 */
        if (collector->session_stats.mos >= 4.3f) {
            collector->session_stats.quality = VOICE_QUALITY_EXCELLENT;
        } else if (collector->session_stats.mos >= 4.0f) {
            collector->session_stats.quality = VOICE_QUALITY_GOOD;
        } else if (collector->session_stats.mos >= 3.6f) {
            collector->session_stats.quality = VOICE_QUALITY_FAIR;
        } else if (collector->session_stats.mos >= 2.6f) {
            collector->session_stats.quality = VOICE_QUALITY_POOR;
        } else {
            collector->session_stats.quality = VOICE_QUALITY_BAD;
        }
    }
    
    *stats = collector->session_stats;
    return VOICE_SUCCESS;
}

/* ============================================
 * 快照
 * ============================================ */

voice_error_t voice_stats_take_snapshot(voice_stats_collector_t *collector) {
    if (!collector || !collector->snapshots) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    voice_stats_snapshot_t snapshot;
    snapshot.timestamp = get_timestamp_ms();
    snapshot.audio = collector->audio_stats;
    snapshot.codec = collector->codec_stats;
    snapshot.network = collector->network_stats;
    
    /* 添加到环形缓冲区 */
    if (collector->snapshot_count < collector->snapshot_capacity) {
        collector->snapshots[collector->snapshot_count++] = snapshot;
    } else {
        /* 移动并覆盖最旧的 */
        memmove(collector->snapshots, 
                collector->snapshots + 1,
                (collector->snapshot_capacity - 1) * sizeof(voice_stats_snapshot_t));
        collector->snapshots[collector->snapshot_capacity - 1] = snapshot;
    }
    
    return VOICE_SUCCESS;
}

size_t voice_stats_get_snapshots(
    voice_stats_collector_t *collector,
    voice_stats_snapshot_t *snapshots,
    size_t max_count
) {
    if (!collector || !snapshots) return 0;
    
    size_t count = collector->snapshot_count;
    if (count > max_count) count = max_count;
    
    memcpy(snapshots, collector->snapshots, count * sizeof(voice_stats_snapshot_t));
    return count;
}

/* ============================================
 * 导出
 * ============================================ */

voice_error_t voice_stats_export_json(
    voice_stats_collector_t *collector,
    char *buffer,
    size_t buffer_size
) {
    if (!collector || !buffer || buffer_size < 256) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    voice_session_stats_t session;
    voice_stats_get_session(collector, &session);
    
    int written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"session\": {\n"
        "    \"duration_ms\": %u,\n"
        "    \"mos\": %.2f,\n"
        "    \"r_factor\": %.2f,\n"
        "    \"quality\": %d\n"
        "  },\n"
        "  \"audio\": {\n"
        "    \"input_level_db\": %.2f,\n"
        "    \"output_level_db\": %.2f,\n"
        "    \"frames_processed\": %u,\n"
        "    \"frames_dropped\": %u\n"
        "  },\n"
        "  \"network\": {\n"
        "    \"packets_sent\": %u,\n"
        "    \"packets_received\": %u,\n"
        "    \"packets_lost\": %u,\n"
        "    \"loss_rate\": %.2f,\n"
        "    \"jitter_avg\": %.2f,\n"
        "    \"rtt\": %u\n"
        "  },\n"
        "  \"codec\": {\n"
        "    \"codec_name\": \"%s\",\n"
        "    \"frames_encoded\": %u,\n"
        "    \"frames_decoded\": %u,\n"
        "    \"bitrate\": %u\n"
        "  }\n"
        "}\n",
        session.duration_ms,
        session.mos,
        session.r_factor,
        (int)session.quality,
        collector->audio_stats.input_level_db,
        collector->audio_stats.output_level_db,
        collector->audio_stats.frames_processed,
        collector->audio_stats.frames_dropped,
        collector->network_stats.packets_sent,
        collector->network_stats.packets_received,
        collector->network_stats.packets_lost,
        collector->network_stats.loss_rate,
        collector->network_stats.jitter_avg,
        collector->network_stats.rtt,
        collector->codec_stats.codec_name[0] ? collector->codec_stats.codec_name : "unknown",
        collector->codec_stats.frames_encoded,
        collector->codec_stats.frames_decoded,
        collector->codec_stats.bitrate
    );
    
    if (written < 0 || (size_t)written >= buffer_size) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }
    
    return VOICE_SUCCESS;
}

/* ============================================
 * 控制
 * ============================================ */

void voice_stats_reset(voice_stats_collector_t *collector) {
    if (!collector) return;
    
    memset(&collector->audio_stats, 0, sizeof(voice_audio_stats_t));
    memset(&collector->codec_stats, 0, sizeof(voice_codec_stats_t));
    memset(&collector->network_stats, 0, sizeof(voice_network_stats_t));
    memset(&collector->session_stats, 0, sizeof(voice_session_stats_t));
    
    collector->snapshot_count = 0;
    
    collector->jitter_sum = 0;
    collector->jitter_sq_sum = 0;
    collector->jitter_count = 0;
    
    collector->rtt_sum = 0;
    collector->rtt_count = 0;
    
    collector->mos_sum = 0;
    collector->mos_count = 0;
    
    collector->start_time = get_timestamp_ms();
    collector->last_update = collector->start_time;
}
