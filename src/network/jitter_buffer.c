/**
 * @file jitter_buffer.c
 * @brief Jitter Buffer and PLC implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "network/jitter_buffer.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

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
} jitter_packet_t;

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
    
    /* 时间跟踪 */
    uint32_t last_output_time;
    uint32_t samples_per_frame;
    
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
    jb->samples_per_frame = config->clock_rate * config->frame_duration_ms / 1000;
    jb->initialized = true;
    
    VOICE_LOG_I("Jitter buffer created: capacity=%u, delay=%ums",
        (unsigned)config->capacity, config->initial_delay_ms);
    
    return jb;
}

void jitter_buffer_destroy(jitter_buffer_t *jb)
{
    if (!jb) return;
    
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
    
    jb->stats.packets_received++;
    
    /* 首包初始化 */
    if (jb->first_packet) {
        jb->first_packet = false;
        jb->next_timestamp = timestamp;
        jb->next_sequence = sequence;
        jb->last_output_time = get_time_ms();
    }
    
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
    memcpy(pkt->data, data, size);
    pkt->size = size;
    pkt->timestamp = timestamp;
    pkt->sequence = sequence;
    pkt->marker = marker;
    pkt->used = true;
    
    if (!jb->packets[slot].used) {
        jb->count++;
    }
    
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
    }
    
    /* 更新期望值 */
    jb->next_sequence++;
    jb->next_timestamp += jb->samples_per_frame;
    
    /* 自适应延迟调整 */
    if (jb->config.mode == JITTER_MODE_ADAPTIVE) {
        uint32_t now = get_time_ms();
        uint32_t elapsed = now - jb->last_output_time;
        jb->last_output_time = now;
        
        /* 简单的自适应算法 */
        if (*status == JITTER_PACKET_LOST) {
            /* 丢包时增加延迟 */
            if (jb->current_delay_ms < jb->config.max_delay_ms) {
                jb->current_delay_ms += 10;
            }
        } else if (jb->count > jb->capacity / 2) {
            /* 缓冲区过满时减少延迟 */
            if (jb->current_delay_ms > jb->config.min_delay_ms) {
                jb->current_delay_ms -= 5;
            }
        }
        
        jb->target_delay_ms = jb->current_delay_ms;
    }
    
    /* 更新统计 */
    jb->stats.current_delay_ms = jb->current_delay_ms;
    
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
    
    jitter_buffer_reset_stats(jb);
}

size_t jitter_buffer_get_count(jitter_buffer_t *jb)
{
    return jb ? jb->count : 0;
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
