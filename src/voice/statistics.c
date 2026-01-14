/**
 * @file statistics.c
 * @brief Voice processing statistics collection implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
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

    voice_audio_stats_t audio_stats;
    voice_codec_stats_t codec_stats;
    voice_network_stats_t network_stats;
    voice_session_stats_t session_stats;

    voice_stats_snapshot_t *snapshots;
    size_t snapshot_count;
    size_t snapshot_capacity;

    uint64_t start_time;
    uint64_t last_update;

    float jitter_sum;
    uint32_t jitter_count;

    int32_t rtt_sum;
    int32_t rtt_count;

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
    config->history_size = 60;
    config->enable_detailed_timing = false;
    config->on_snapshot = NULL;
    config->on_quality_change = NULL;
    config->callback_user_data = NULL;
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

    if (collector->config.history_size > 0) {
        collector->snapshots = (voice_stats_snapshot_t *)calloc(
            collector->config.history_size,
            sizeof(voice_stats_snapshot_t)
        );
        collector->snapshot_capacity = collector->config.history_size;
    }

    collector->start_time = get_timestamp_ms();
    collector->last_update = collector->start_time;

    return collector;
}

void voice_stats_collector_destroy(voice_stats_collector_t *collector) {
    if (!collector) return;

    if (collector->snapshots) {
        free(collector->snapshots);
    }

    free(collector);
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
}

void voice_stats_update_codec(
    voice_stats_collector_t *collector,
    const voice_codec_stats_t *stats
) {
    if (!collector || !stats) return;
    collector->codec_stats = *stats;
}

void voice_stats_update_network(
    voice_stats_collector_t *collector,
    const voice_network_stats_t *stats
) {
    if (!collector || !stats) return;
    collector->network_stats = *stats;

    collector->jitter_sum += stats->jitter_ms;
    collector->jitter_count++;
}

/* ============================================
 * 事件记录
 * ============================================ */

void voice_stats_on_packet_sent(
    voice_stats_collector_t *collector,
    size_t bytes
) {
    if (!collector) return;
    collector->network_stats.packets_sent++;
    collector->network_stats.bytes_sent += bytes;
}

void voice_stats_on_packet_received(
    voice_stats_collector_t *collector,
    size_t size,
    uint32_t delay_ms
) {
    if (!collector) return;
    collector->network_stats.packets_received++;
    collector->network_stats.bytes_received += size;
    (void)delay_ms;
}

void voice_stats_on_packet_lost(
    voice_stats_collector_t *collector,
    uint32_t count
) {
    if (!collector) return;
    collector->network_stats.packets_lost += count;

    uint64_t total = collector->network_stats.packets_received + collector->network_stats.packets_lost;
    if (total > 0) {
        collector->network_stats.loss_rate = (float)collector->network_stats.packets_lost / (float)total;
    }
}

void voice_stats_add_rtt(
    voice_stats_collector_t *collector,
    uint32_t rtt_ms
) {
    if (!collector) return;

    collector->network_stats.rtt_ms = (float)rtt_ms;
    collector->rtt_sum += rtt_ms;
    collector->rtt_count++;
}

void voice_stats_add_jitter(
    voice_stats_collector_t *collector,
    float jitter_ms
) {
    if (!collector) return;

    collector->network_stats.jitter_ms = jitter_ms;
    collector->jitter_sum += jitter_ms;
    collector->jitter_count++;
}

/* ============================================
 * 获取统计
 * ============================================ */

void voice_stats_get_audio(
    voice_stats_collector_t *collector,
    voice_audio_stats_t *stats
) {
    if (!collector || !stats) return;
    *stats = collector->audio_stats;
}

void voice_stats_get_codec(
    voice_stats_collector_t *collector,
    voice_codec_stats_t *stats
) {
    if (!collector || !stats) return;
    *stats = collector->codec_stats;
}

void voice_stats_get_network(
    voice_stats_collector_t *collector,
    voice_network_stats_t *stats
) {
    if (!collector || !stats) return;
    *stats = collector->network_stats;
}

voice_error_t voice_stats_get_session(
    voice_stats_collector_t *collector,
    voice_session_stats_t *stats
) {
    if (!collector || !stats) return VOICE_ERROR_NULL_POINTER;

    uint64_t now = get_timestamp_ms();

    stats->session_duration_ms = now - collector->start_time;
    stats->start_timestamp = collector->start_time;

    /* 计算 MOS (简化版) */
    float R = 93.2f;
    R -= collector->network_stats.loss_rate * 100.0f * 2.5f;
    R -= collector->network_stats.jitter_ms * 0.1f;

    if (R < 0) R = 0;
    if (R > 100) R = 100;

    stats->r_factor = R;
    stats->mos_lq = 1.0f + 0.035f * R + R * (R - 60.0f) * (100.0f - R) * 7.0e-6f;
    stats->mos_cq = stats->mos_lq;

    if (stats->mos_lq < 1.0f) stats->mos_lq = 1.0f;
    if (stats->mos_lq > 5.0f) stats->mos_lq = 5.0f;
    if (stats->mos_cq < 1.0f) stats->mos_cq = 1.0f;
    if (stats->mos_cq > 5.0f) stats->mos_cq = 5.0f;

    stats->audio = collector->audio_stats;
    stats->codec = collector->codec_stats;
    stats->network = collector->network_stats;

    return VOICE_OK;
}

/* ============================================
 * 快照管理
 * ============================================ */

void voice_stats_take_snapshot(voice_stats_collector_t *collector) {
    if (!collector || !collector->snapshots) return;
    if (collector->snapshot_count >= collector->snapshot_capacity) return;

    voice_stats_snapshot_t *snap = &collector->snapshots[collector->snapshot_count];

    snap->timestamp = get_timestamp_ms();
    snap->interval_ms = collector->config.snapshot_interval_ms;
    snap->packets_sent = (uint32_t)collector->network_stats.packets_sent;
    snap->packets_received = (uint32_t)collector->network_stats.packets_received;
    snap->packets_lost = (uint32_t)collector->network_stats.packets_lost;
    snap->bytes_sent = (uint32_t)collector->network_stats.bytes_sent;
    snap->bytes_received = (uint32_t)collector->network_stats.bytes_received;
    snap->avg_rtt_ms = collector->network_stats.rtt_ms;
    snap->avg_jitter_ms = collector->network_stats.jitter_ms;

    voice_session_stats_t session;
    voice_stats_get_session(collector, &session);
    snap->avg_mos = session.mos_lq;

    collector->snapshot_count++;

    if (collector->config.on_snapshot) {
        collector->config.on_snapshot(snap, collector->config.callback_user_data);
    }
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

void voice_stats_clear_snapshots(voice_stats_collector_t *collector) {
    if (!collector) return;
    collector->snapshot_count = 0;
}

/* ============================================
 * 重置
 * ============================================ */

void voice_stats_reset(voice_stats_collector_t *collector) {
    if (!collector) return;

    memset(&collector->audio_stats, 0, sizeof(voice_audio_stats_t));
    memset(&collector->codec_stats, 0, sizeof(voice_codec_stats_t));
    memset(&collector->network_stats, 0, sizeof(voice_network_stats_t));
    memset(&collector->session_stats, 0, sizeof(voice_session_stats_t));

    collector->jitter_sum = 0;
    collector->jitter_count = 0;
    collector->rtt_sum = 0;
    collector->rtt_count = 0;
    collector->mos_sum = 0;
    collector->mos_count = 0;

    collector->start_time = get_timestamp_ms();
    collector->last_update = collector->start_time;
    collector->snapshot_count = 0;
}

/* ============================================
 * 报告生成
 * ============================================ */

void voice_stats_generate_report(
    voice_stats_collector_t *collector,
    char *buffer,
    size_t buffer_size
) {
    if (!collector || !buffer || buffer_size == 0) return;

    voice_session_stats_t session;
    voice_stats_get_session(collector, &session);

    snprintf(buffer, buffer_size,
        "=== Voice Statistics Report ===\n"
        "Duration: %llu ms\n"
        "Packets: sent=%llu, received=%llu, lost=%llu\n"
        "Bytes: sent=%llu, received=%llu\n"
        "Loss Rate: %.2f%%\n"
        "RTT: %.1f ms\n"
        "Jitter: %.1f ms\n"
        "MOS: %.2f\n"
        "R-Factor: %.1f\n",
        (unsigned long long)session.session_duration_ms,
        (unsigned long long)collector->network_stats.packets_sent,
        (unsigned long long)collector->network_stats.packets_received,
        (unsigned long long)collector->network_stats.packets_lost,
        (unsigned long long)collector->network_stats.bytes_sent,
        (unsigned long long)collector->network_stats.bytes_received,
        collector->network_stats.loss_rate * 100.0f,
        collector->network_stats.rtt_ms,
        collector->network_stats.jitter_ms,
        session.mos_lq,
        session.r_factor
    );
}
