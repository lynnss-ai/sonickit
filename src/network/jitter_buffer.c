/**
 * @file jitter_buffer.c
 * @brief Jitter Buffer and PLC implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1 Enhancement:
 * - Jitter histogram for statistical delay estimation
 * - WSOLA time stretch integration
 * - Improved adaptive algorithm
 */

#include "network/jitter_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/* ============================================
 * 常量定义
 * ============================================ */

#define JITTER_SMOOTH_FACTOR        0.1f    /* 抖动平滑因子 */
#define DELAY_ADJUST_THRESHOLD_MS   10      /* 延迟调整阈值 */
#define STRETCH_RATE_MIN            0.85f   /* 最小拉伸率(减速) */
#define STRETCH_RATE_MAX            1.15f   /* 最大拉伸率(加速) */
#define STRETCH_RATE_STEP           0.02f   /* 拉伸率调整步长 */
#define BUFFER_LEVEL_LOW            1.0f    /* 低水位(帧) */
#define BUFFER_LEVEL_HIGH           4.0f    /* 高水位(帧) */

/* ============================================
 * 缓冲区包结构
 * ============================================ */

typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t timestamp;
    uint16_t sequence;
    bool marker;
    bool used;
    uint32_t arrival_time_ms;   /* 包到达时间 */
} jitter_packet_t;

/* ============================================
 * 抖动统计结构
 * ============================================ */

typedef struct {
    /* 直方图 */
    uint32_t histogram[JITTER_HISTOGRAM_BINS];
    float    bin_width_ms;
    uint64_t total_samples;

    /* 延迟历史 (用于百分位计算) */
    float    delay_history[JITTER_HISTORY_SIZE];
    size_t   history_index;
    size_t   history_count;

    /* 运行统计 */
    float    jitter_avg;        /* 平均抖动 */
    float    jitter_max;        /* 最大抖动 */
    float    last_arrival_delta;/* 上次到达间隔 */
    uint32_t last_arrival_time; /* 上次到达时间 */
    uint32_t last_rtp_timestamp;/* 上次RTP时间戳 */
    bool     first_packet;
} jitter_stats_t;

/* ============================================
 * Jitter Buffer 结构
 * ============================================ */

struct jitter_buffer_s {
    jitter_buffer_config_t config;

    /* 包存储 */
    jitter_packet_t *packets;
    size_t capacity;
    size_t count;

    /* 状态 */
    bool initialized;
    uint32_t next_timestamp;    /* 下一个期望的时间戳 */
    uint16_t next_sequence;     /* 下一个期望的序列号 */
    bool first_packet;

    /* 延迟控制 */
    uint32_t target_delay_ms;
    uint32_t current_delay_ms;
    uint32_t optimal_delay_ms;  /* 统计计算的最优延迟 */

    /* 时间拉伸控制 */
    bool     time_stretch_enabled;
    float    current_stretch_rate;
    float    target_buffer_level;

    /* 时间跟踪 */
    uint32_t last_output_time;
    uint32_t samples_per_frame;

    /* 抖动统计 */
    jitter_stats_t jitter_stats;

    /* PLC 实例 */
    voice_plc_t *plc;

    /* 统计 */
    jitter_buffer_stats_t stats;
};

/* ============================================
 * 时间工具
 * ============================================ */

static uint32_t get_time_ms(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

/* ============================================
 * 抖动统计函数
 * ============================================ */

static void jitter_stats_init(jitter_stats_t *stats, uint32_t max_delay_ms)
{
    memset(stats, 0, sizeof(jitter_stats_t));
    stats->bin_width_ms = (float)max_delay_ms / JITTER_HISTOGRAM_BINS;
    stats->first_packet = true;
}

static void jitter_stats_update(jitter_stats_t *stats, uint32_t arrival_time,
                                 uint32_t rtp_timestamp, uint32_t clock_rate)
{
    if (stats->first_packet) {
        stats->first_packet = false;
        stats->last_arrival_time = arrival_time;
        stats->last_rtp_timestamp = rtp_timestamp;
        return;
    }

    /* 计算到达间隔偏差 (RFC 3550 jitter calculation) */
    int32_t arrival_delta = (int32_t)(arrival_time - stats->last_arrival_time);
    int32_t rtp_delta = (int32_t)(rtp_timestamp - stats->last_rtp_timestamp);
    int32_t rtp_delta_ms = (rtp_delta * 1000) / (int32_t)clock_rate;

    int32_t jitter = arrival_delta - rtp_delta_ms;
    if (jitter < 0) jitter = -jitter;

    float jitter_f = (float)jitter;

    /* 指数加权移动平均 */
    stats->jitter_avg = stats->jitter_avg * (1.0f - JITTER_SMOOTH_FACTOR)
                      + jitter_f * JITTER_SMOOTH_FACTOR;

    if (jitter_f > stats->jitter_max) {
        stats->jitter_max = jitter_f;
    }

    /* 更新直方图 */
    int bin = (int)(jitter_f / stats->bin_width_ms);
    if (bin < 0) bin = 0;
    if (bin >= JITTER_HISTOGRAM_BINS) bin = JITTER_HISTOGRAM_BINS - 1;
    stats->histogram[bin]++;
    stats->total_samples++;

    /* 更新延迟历史 */
    stats->delay_history[stats->history_index] = jitter_f;
    stats->history_index = (stats->history_index + 1) % JITTER_HISTORY_SIZE;
    if (stats->history_count < JITTER_HISTORY_SIZE) {
        stats->history_count++;
    }

    stats->last_arrival_time = arrival_time;
    stats->last_rtp_timestamp = rtp_timestamp;
    stats->last_arrival_delta = jitter_f;
}

/* 计算百分位延迟 */
static float jitter_stats_get_percentile(jitter_stats_t *stats, float percentile)
{
    if (stats->history_count == 0) return 0.0f;

    /* 复制并排序历史记录 */
    float sorted[JITTER_HISTORY_SIZE];
    size_t n = stats->history_count;
    memcpy(sorted, stats->delay_history, n * sizeof(float));

    /* 简单插入排序 (数据量小) */
    for (size_t i = 1; i < n; i++) {
        float key = sorted[i];
        size_t j = i;
        while (j > 0 && sorted[j - 1] > key) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = key;
    }

    /* 获取百分位值 */
    size_t idx = (size_t)(n * percentile);
    if (idx >= n) idx = n - 1;

    return sorted[idx];
}

/* 计算最优延迟 */
static uint32_t calculate_optimal_delay(jitter_buffer_t *jb)
{
    jitter_stats_t *stats = &jb->jitter_stats;

    /* 基于百分位抖动计算 */
    float p_jitter = jitter_stats_get_percentile(stats, jb->config.jitter_percentile);

    /* 最优延迟 = 百分位抖动 + 1帧时长安全边际 */
    uint32_t optimal = (uint32_t)(p_jitter + jb->config.frame_duration_ms);

    /* 限制范围 */
    if (optimal < jb->config.min_delay_ms) {
        optimal = jb->config.min_delay_ms;
    }
    if (optimal > jb->config.max_delay_ms) {
        optimal = jb->config.max_delay_ms;
    }

    return optimal;
}

/* 计算时间拉伸率 */
static float calculate_stretch_rate(jitter_buffer_t *jb)
{
    float buffer_level = (float)jb->count;
    float target = jb->target_buffer_level;
    float rate = 1.0f;

    if (buffer_level < BUFFER_LEVEL_LOW) {
        /* 缓冲区过空 - 减速播放 */
        rate = STRETCH_RATE_MIN + (buffer_level / BUFFER_LEVEL_LOW) * (1.0f - STRETCH_RATE_MIN);
    } else if (buffer_level > BUFFER_LEVEL_HIGH) {
        /* 缓冲区过满 - 加速播放 */
        float excess = (buffer_level - BUFFER_LEVEL_HIGH) / BUFFER_LEVEL_HIGH;
        rate = 1.0f + excess * (STRETCH_RATE_MAX - 1.0f);
        if (rate > STRETCH_RATE_MAX) rate = STRETCH_RATE_MAX;
    } else if (buffer_level > target + 1.0f) {
        /* 略高于目标 - 轻微加速 */
        rate = 1.0f + STRETCH_RATE_STEP;
    } else if (buffer_level < target - 1.0f && buffer_level >= BUFFER_LEVEL_LOW) {
        /* 略低于目标 - 轻微减速 */
        rate = 1.0f - STRETCH_RATE_STEP;
    }

    return rate;
}

/* ============================================
 * Jitter Buffer 实现
 * ============================================ */

void jitter_buffer_config_init(jitter_buffer_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(jitter_buffer_config_t));
    config->clock_rate = 48000;
    config->frame_duration_ms = 20;
    config->mode = JITTER_MODE_ADAPTIVE;
    config->min_delay_ms = JITTER_BUFFER_DEFAULT_MIN_DELAY;
    config->max_delay_ms = JITTER_BUFFER_DEFAULT_MAX_DELAY;
    config->initial_delay_ms = 60;
    config->capacity = JITTER_BUFFER_DEFAULT_CAPACITY;
    config->enable_plc = true;
    config->enable_time_stretch = true;
    config->target_buffer_level = 2.0f;
    config->jitter_percentile = 0.95f;
}

jitter_buffer_t *jitter_buffer_create(const jitter_buffer_config_t *config)
{
    if (!config || config->capacity == 0) {
        return NULL;
    }

    jitter_buffer_t *jb = (jitter_buffer_t *)calloc(1, sizeof(jitter_buffer_t));
    if (!jb) {
        return NULL;
    }

    jb->config = *config;
    jb->capacity = config->capacity;

    /* 分配包存储 */
    jb->packets = (jitter_packet_t *)calloc(config->capacity, sizeof(jitter_packet_t));
    if (!jb->packets) {
        free(jb);
        return NULL;
    }

    /* 为每个槽分配数据缓冲区 */
    size_t max_packet_size = 1500;
    for (size_t i = 0; i < config->capacity; i++) {
        jb->packets[i].data = (uint8_t *)malloc(max_packet_size);
        if (!jb->packets[i].data) {
            for (size_t j = 0; j < i; j++) {
                free(jb->packets[j].data);
            }
            free(jb->packets);
            free(jb);
            return NULL;
        }
    }

    jb->first_packet = true;
    jb->target_delay_ms = config->initial_delay_ms;
    jb->current_delay_ms = config->initial_delay_ms;
    jb->optimal_delay_ms = config->initial_delay_ms;
    jb->samples_per_frame = config->clock_rate * config->frame_duration_ms / 1000;
    jb->time_stretch_enabled = config->enable_time_stretch;
    jb->current_stretch_rate = 1.0f;
    jb->target_buffer_level = config->target_buffer_level;

    /* 初始化抖动统计 */
    jitter_stats_init(&jb->jitter_stats, config->max_delay_ms);

    /* 初始化PLC */
    if (config->enable_plc) {
        voice_plc_config_t plc_config;
        voice_plc_config_init(&plc_config);
        plc_config.sample_rate = config->clock_rate;
        plc_config.frame_size = jb->samples_per_frame;
        plc_config.algorithm = PLC_ALG_FADE;
        jb->plc = voice_plc_create(&plc_config);
    }

    jb->initialized = true;

    VOICE_LOG_I("Jitter buffer created: capacity=%u, delay=%ums, time_stretch=%s",
        (unsigned)config->capacity, config->initial_delay_ms,
        config->enable_time_stretch ? "enabled" : "disabled");

    return jb;
}

void jitter_buffer_destroy(jitter_buffer_t *jb)
{
    if (!jb) return;

    if (jb->plc) {
        voice_plc_destroy(jb->plc);
    }

    if (jb->packets) {
        for (size_t i = 0; i < jb->capacity; i++) {
            if (jb->packets[i].data) {
                free(jb->packets[i].data);
            }
        }
        free(jb->packets);
    }

    free(jb);
}

voice_error_t jitter_buffer_put(
    jitter_buffer_t *jb,
    const uint8_t *data,
    size_t size,
    uint32_t timestamp,
    uint16_t sequence,
    bool marker)
{
    if (!jb || !jb->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!data || size == 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    uint32_t arrival_time = get_time_ms();
    jb->stats.packets_received++;

    /* 首包初始化 */
    if (jb->first_packet) {
        jb->first_packet = false;
        jb->next_timestamp = timestamp;
        jb->next_sequence = sequence;
        jb->last_output_time = arrival_time;
    }

    /* 更新抖动统计 */
    jitter_stats_update(&jb->jitter_stats, arrival_time, timestamp, jb->config.clock_rate);

    /* 检查是否太迟 */
    int32_t ts_diff = (int32_t)(timestamp - jb->next_timestamp);
    if (ts_diff < -(int32_t)(jb->samples_per_frame * 2)) {
        jb->stats.packets_late++;
        return VOICE_OK;  /* 丢弃太迟的包 */
    }

    /* 查找槽位 */
    size_t slot = sequence % jb->capacity;
    jitter_packet_t *pkt = &jb->packets[slot];

    /* 检查重复 */
    if (pkt->used && pkt->sequence == sequence) {
        jb->stats.packets_duplicate++;
        return VOICE_OK;
    }

    /* 存储包 */
    if (!pkt->used) {
        jb->count++;
    }

    memcpy(pkt->data, data, size);
    pkt->size = size;
    pkt->timestamp = timestamp;
    pkt->sequence = sequence;
    pkt->marker = marker;
    pkt->arrival_time_ms = arrival_time;
    pkt->used = true;

    return VOICE_OK;
}

voice_error_t jitter_buffer_get(
    jitter_buffer_t *jb,
    uint8_t *output,
    size_t max_size,
    size_t *out_size,
    jitter_packet_status_t *status)
{
    if (!jb || !jb->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!output || !out_size || !status) {
        return VOICE_ERROR_NULL_POINTER;
    }

    *out_size = 0;
    *status = JITTER_PACKET_LOST;

    /* 查找目标包 */
    size_t slot = jb->next_sequence % jb->capacity;
    jitter_packet_t *pkt = &jb->packets[slot];

    if (pkt->used && pkt->sequence == jb->next_sequence) {
        /* 找到包 */
        if (pkt->size > max_size) {
            return VOICE_ERROR_BUFFER_TOO_SMALL;
        }

        memcpy(output, pkt->data, pkt->size);
        *out_size = pkt->size;
        *status = JITTER_PACKET_OK;

        /* 标记为已使用 */
        pkt->used = false;
        jb->count--;
        jb->stats.packets_output++;

    } else {
        /* 包丢失 */
        jb->stats.packets_lost++;
        *status = JITTER_PACKET_LOST;
        jb->stats.packets_interpolated++;
    }

    /* 更新期望值 */
    jb->next_sequence++;
    jb->next_timestamp += jb->samples_per_frame;

    /* 自适应延迟调整 - 增强版 */
    if (jb->config.mode == JITTER_MODE_ADAPTIVE) {
        uint32_t now = get_time_ms();
        jb->last_output_time = now;

        /* 计算最优延迟（基于抖动统计） */
        jb->optimal_delay_ms = calculate_optimal_delay(jb);

        /* 平滑调整当前延迟向最优延迟靠拢 */
        if (jb->current_delay_ms < jb->optimal_delay_ms) {
            uint32_t diff = jb->optimal_delay_ms - jb->current_delay_ms;
            jb->current_delay_ms += (diff > 10) ? 10 : diff;
        } else if (jb->current_delay_ms > jb->optimal_delay_ms + DELAY_ADJUST_THRESHOLD_MS) {
            uint32_t diff = jb->current_delay_ms - jb->optimal_delay_ms;
            jb->current_delay_ms -= (diff > 5) ? 5 : diff;
        }

        /* 丢包时额外增加延迟 */
        if (*status == JITTER_PACKET_LOST) {
            if (jb->current_delay_ms < jb->config.max_delay_ms) {
                jb->current_delay_ms += 10;
            }
        }

        /* 计算时间拉伸率 */
        if (jb->time_stretch_enabled) {
            float new_rate = calculate_stretch_rate(jb);
            /* 平滑过渡 */
            jb->current_stretch_rate = jb->current_stretch_rate * 0.9f + new_rate * 0.1f;

            if (new_rate > 1.0f) {
                jb->stats.accelerate_count++;
            } else if (new_rate < 1.0f) {
                jb->stats.decelerate_count++;
            }
        }

        jb->target_delay_ms = jb->optimal_delay_ms;
    }

    /* 更新统计 */
    jb->stats.current_delay_ms = jb->current_delay_ms;
    jb->stats.target_delay_ms = jb->optimal_delay_ms;
    jb->stats.jitter_ms = jb->jitter_stats.jitter_avg;
    jb->stats.jitter_max_ms = jb->jitter_stats.jitter_max;
    jb->stats.jitter_percentile_ms = jitter_stats_get_percentile(
        &jb->jitter_stats, jb->config.jitter_percentile);
    jb->stats.current_stretch_rate = jb->current_stretch_rate;
    jb->stats.buffer_level = (float)jb->count;
    jb->stats.buffer_health = (jb->count >= 2 && jb->count <= 6) ? 1.0f :
                              (jb->count == 0) ? 0.0f : 0.5f;

    return VOICE_OK;
}

voice_error_t jitter_buffer_set_delay(jitter_buffer_t *jb, uint32_t delay_ms)
{
    if (!jb) {
        return VOICE_ERROR_NULL_POINTER;
    }

    if (delay_ms < jb->config.min_delay_ms) {
        delay_ms = jb->config.min_delay_ms;
    }
    if (delay_ms > jb->config.max_delay_ms) {
        delay_ms = jb->config.max_delay_ms;
    }

    jb->target_delay_ms = delay_ms;
    jb->current_delay_ms = delay_ms;

    return VOICE_OK;
}

uint32_t jitter_buffer_get_delay(jitter_buffer_t *jb)
{
    return jb ? jb->current_delay_ms : 0;
}

voice_error_t jitter_buffer_get_stats(
    jitter_buffer_t *jb,
    jitter_buffer_stats_t *stats)
{
    if (!jb || !stats) {
        return VOICE_ERROR_NULL_POINTER;
    }

    *stats = jb->stats;

    /* 计算丢包率 */
    uint64_t total = stats->packets_output + stats->packets_lost;
    if (total > 0) {
        stats->loss_rate = (float)stats->packets_lost / total;
    }

    return VOICE_OK;
}

void jitter_buffer_reset_stats(jitter_buffer_t *jb)
{
    if (jb) {
        memset(&jb->stats, 0, sizeof(jitter_buffer_stats_t));
    }
}

void jitter_buffer_reset(jitter_buffer_t *jb)
{
    if (!jb) return;

    /* 清空所有包 */
    for (size_t i = 0; i < jb->capacity; i++) {
        jb->packets[i].used = false;
    }

    jb->count = 0;
    jb->first_packet = true;
    jb->current_delay_ms = jb->config.initial_delay_ms;
    jb->target_delay_ms = jb->config.initial_delay_ms;
    jb->optimal_delay_ms = jb->config.initial_delay_ms;
    jb->current_stretch_rate = 1.0f;

    /* 重置抖动统计 */
    jitter_stats_init(&jb->jitter_stats, jb->config.max_delay_ms);

    /* 重置PLC */
    if (jb->plc) {
        voice_plc_reset(jb->plc);
    }

    jitter_buffer_reset_stats(jb);
}

size_t jitter_buffer_get_count(jitter_buffer_t *jb)
{
    return jb ? jb->count : 0;
}

voice_error_t jitter_buffer_get_histogram(
    jitter_buffer_t *jb,
    uint32_t *histogram,
    float *bin_width_ms)
{
    if (!jb || !histogram || !bin_width_ms) {
        return VOICE_ERROR_NULL_POINTER;
    }

    memcpy(histogram, jb->jitter_stats.histogram,
           JITTER_HISTOGRAM_BINS * sizeof(uint32_t));
    *bin_width_ms = jb->jitter_stats.bin_width_ms;

    return VOICE_OK;
}

voice_error_t jitter_buffer_enable_time_stretch(
    jitter_buffer_t *jb,
    bool enable)
{
    if (!jb) {
        return VOICE_ERROR_NULL_POINTER;
    }

    jb->time_stretch_enabled = enable;
    if (!enable) {
        jb->current_stretch_rate = 1.0f;
    }

    return VOICE_OK;
}

float jitter_buffer_get_playout_rate(jitter_buffer_t *jb)
{
    if (!jb) return 1.0f;
    return jb->current_stretch_rate;
}

/* ============================================
 * PLC 结构
 * ============================================ */

struct voice_plc_s {
    voice_plc_config_t config;
    int16_t *last_frame;
    size_t last_frame_size;
    uint32_t consecutive_loss;
    float fade_factor;
    bool initialized;
};

/* ============================================
 * PLC 实现
 * ============================================ */

void voice_plc_config_init(voice_plc_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(voice_plc_config_t));
    config->sample_rate = 48000;
    config->frame_size = 960;  /* 20ms @ 48kHz */
    config->algorithm = PLC_ALG_FADE;
    config->max_consecutive_loss = 10;
}

voice_plc_t *voice_plc_create(const voice_plc_config_t *config)
{
    if (!config || config->frame_size == 0) {
        return NULL;
    }

    voice_plc_t *plc = (voice_plc_t *)calloc(1, sizeof(voice_plc_t));
    if (!plc) {
        return NULL;
    }

    plc->config = *config;

    plc->last_frame = (int16_t *)calloc(config->frame_size, sizeof(int16_t));
    if (!plc->last_frame) {
        free(plc);
        return NULL;
    }

    plc->fade_factor = 1.0f;
    plc->initialized = true;

    return plc;
}

void voice_plc_destroy(voice_plc_t *plc)
{
    if (!plc) return;

    if (plc->last_frame) {
        free(plc->last_frame);
    }

    free(plc);
}

void voice_plc_update_good_frame(
    voice_plc_t *plc,
    const int16_t *pcm,
    size_t samples)
{
    if (!plc || !plc->initialized || !pcm) {
        return;
    }

    size_t copy_size = samples < plc->config.frame_size ? samples : plc->config.frame_size;
    memcpy(plc->last_frame, pcm, copy_size * sizeof(int16_t));
    plc->last_frame_size = copy_size;

    /* 重置丢包计数和衰减因子 */
    plc->consecutive_loss = 0;
    plc->fade_factor = 1.0f;
}

voice_error_t voice_plc_generate(
    voice_plc_t *plc,
    int16_t *output,
    size_t samples)
{
    if (!plc || !plc->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!output) {
        return VOICE_ERROR_NULL_POINTER;
    }

    plc->consecutive_loss++;

    /* 检查是否超过最大连续丢包 */
    if (plc->consecutive_loss > plc->config.max_consecutive_loss) {
        memset(output, 0, samples * sizeof(int16_t));
        return VOICE_OK;
    }

    switch (plc->config.algorithm) {
    case PLC_ALG_ZERO:
        memset(output, 0, samples * sizeof(int16_t));
        break;

    case PLC_ALG_REPEAT:
        {
            size_t copy_size = samples < plc->last_frame_size ? samples : plc->last_frame_size;
            memcpy(output, plc->last_frame, copy_size * sizeof(int16_t));
            if (copy_size < samples) {
                memset(output + copy_size, 0, (samples - copy_size) * sizeof(int16_t));
            }
        }
        break;

    case PLC_ALG_FADE:
        {
            /* 渐变衰减 */
            plc->fade_factor *= 0.8f;  /* 每帧衰减20% */
            if (plc->fade_factor < 0.1f) {
                plc->fade_factor = 0.0f;
            }

            size_t copy_size = samples < plc->last_frame_size ? samples : plc->last_frame_size;
            for (size_t i = 0; i < copy_size; i++) {
                output[i] = (int16_t)(plc->last_frame[i] * plc->fade_factor);
            }
            if (copy_size < samples) {
                memset(output + copy_size, 0, (samples - copy_size) * sizeof(int16_t));
            }
        }
        break;

    case PLC_ALG_INTERPOLATE:
        {
            /* 简单的波形重复+抖动 */
            plc->fade_factor *= 0.85f;

            size_t copy_size = samples < plc->last_frame_size ? samples : plc->last_frame_size;
            for (size_t i = 0; i < copy_size; i++) {
                /* 添加轻微相位偏移模拟 */
                size_t src_idx = (i + plc->consecutive_loss * 3) % plc->last_frame_size;
                output[i] = (int16_t)(plc->last_frame[src_idx] * plc->fade_factor);
            }
            if (copy_size < samples) {
                memset(output + copy_size, 0, (samples - copy_size) * sizeof(int16_t));
            }
        }
        break;

    default:
        memset(output, 0, samples * sizeof(int16_t));
        break;
    }

    return VOICE_OK;
}

void voice_plc_reset(voice_plc_t *plc)
{
    if (!plc) return;

    if (plc->last_frame) {
        memset(plc->last_frame, 0, plc->config.frame_size * sizeof(int16_t));
    }

    plc->last_frame_size = 0;
    plc->consecutive_loss = 0;
    plc->fade_factor = 1.0f;
}
