/**
 * @file bandwidth_estimator.c
 * @brief Network bandwidth estimation implementation
 * 
 * 基于 AIMD (Additive Increase Multiplicative Decrease) 的带宽估计
 */

#include "network/bandwidth_estimator.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    static uint32_t get_time_ms(void) {
        return (uint32_t)GetTickCount64();
    }
#else
    #include <time.h>
    static uint32_t get_time_ms(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
    }
#endif

/* ============================================
 * 内部结构
 * ============================================ */

#define BWE_HISTORY_SIZE 32

typedef struct {
    uint16_t sequence;
    size_t size;
    uint32_t send_time;
    bool acked;
} bwe_packet_record_t;

struct voice_bwe_s {
    voice_bwe_config_t config;
    
    /* 当前估计 */
    uint32_t current_bitrate;
    uint32_t target_bitrate;
    voice_network_quality_t quality;
    
    /* 统计 */
    float packet_loss_rate;
    uint32_t rtt_ms;
    uint32_t jitter_ms;
    
    /* 发送记录 */
    bwe_packet_record_t send_history[BWE_HISTORY_SIZE];
    size_t history_pos;
    uint32_t total_sent;
    uint32_t total_lost;
    size_t bytes_sent_window;
    
    /* AIMD 状态 */
    uint32_t last_adjust_time;
    bool in_decrease_phase;
    
    /* 回调 */
    voice_bwe_callback_t callback;
    void *callback_user_data;
};

/* ============================================
 * 配置
 * ============================================ */

void voice_bwe_config_init(voice_bwe_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_bwe_config_t));
    
    config->initial_bitrate = 32000;    /* 32 kbps */
    config->min_bitrate = 8000;         /* 8 kbps */
    config->max_bitrate = 128000;       /* 128 kbps */
    
    config->window_size_ms = 2000;      /* 2 秒窗口 */
    
    /* AIMD 参数 */
    config->increase_rate = 4000.0f;    /* 每秒增加 4 kbps */
    config->decrease_factor = 0.8f;     /* 减少 20% */
    
    config->loss_threshold_increase = 0.02f;  /* 2% 以下丢包允许增加 */
    config->loss_threshold_decrease = 0.10f;  /* 10% 以上触发减少 */
    config->rtt_threshold_ms = 400;
    
    config->hold_time_ms = 1000;        /* 调整后等待 1 秒 */
}

/* ============================================
 * 创建/销毁
 * ============================================ */

voice_bwe_t *voice_bwe_create(const voice_bwe_config_t *config) {
    voice_bwe_t *bwe = (voice_bwe_t *)calloc(1, sizeof(voice_bwe_t));
    if (!bwe) return NULL;
    
    if (config) {
        bwe->config = *config;
    } else {
        voice_bwe_config_init(&bwe->config);
    }
    
    bwe->current_bitrate = bwe->config.initial_bitrate;
    bwe->target_bitrate = bwe->config.initial_bitrate;
    bwe->quality = VOICE_NETWORK_GOOD;
    
    return bwe;
}

void voice_bwe_destroy(voice_bwe_t *bwe) {
    if (bwe) {
        free(bwe);
    }
}

/* ============================================
 * 网络质量评估
 * ============================================ */

static voice_network_quality_t evaluate_quality(
    float packet_loss_rate,
    uint32_t rtt_ms,
    uint32_t jitter_ms
) {
    float loss_pct = packet_loss_rate * 100.0f;
    
    if (loss_pct < 1.0f && rtt_ms < 50 && jitter_ms < 20) {
        return VOICE_NETWORK_EXCELLENT;
    }
    if (loss_pct < 3.0f && rtt_ms < 100 && jitter_ms < 40) {
        return VOICE_NETWORK_GOOD;
    }
    if (loss_pct < 10.0f && rtt_ms < 200 && jitter_ms < 80) {
        return VOICE_NETWORK_FAIR;
    }
    if (loss_pct < 20.0f && rtt_ms < 400) {
        return VOICE_NETWORK_POOR;
    }
    return VOICE_NETWORK_BAD;
}

/* ============================================
 * 带宽调整
 * ============================================ */

static void adjust_bitrate(voice_bwe_t *bwe) {
    uint32_t now = get_time_ms();
    
    /* 检查是否在稳定期 */
    if (now - bwe->last_adjust_time < bwe->config.hold_time_ms) {
        return;
    }
    
    uint32_t old_bitrate = bwe->target_bitrate;
    uint32_t new_bitrate = old_bitrate;
    
    /* AIMD 算法 */
    if (bwe->packet_loss_rate > bwe->config.loss_threshold_decrease ||
        bwe->rtt_ms > bwe->config.rtt_threshold_ms) {
        /* 乘法减少 */
        new_bitrate = (uint32_t)(old_bitrate * bwe->config.decrease_factor);
        bwe->in_decrease_phase = true;
    } else if (bwe->packet_loss_rate < bwe->config.loss_threshold_increase) {
        /* 加法增加 */
        float elapsed_sec = (float)(now - bwe->last_adjust_time) / 1000.0f;
        uint32_t increase = (uint32_t)(bwe->config.increase_rate * elapsed_sec);
        new_bitrate = old_bitrate + increase;
        bwe->in_decrease_phase = false;
    }
    
    /* 限制范围 */
    if (new_bitrate < bwe->config.min_bitrate) {
        new_bitrate = bwe->config.min_bitrate;
    }
    if (new_bitrate > bwe->config.max_bitrate) {
        new_bitrate = bwe->config.max_bitrate;
    }
    
    /* 更新目标比特率 */
    if (new_bitrate != old_bitrate) {
        bwe->target_bitrate = new_bitrate;
        bwe->last_adjust_time = now;
        
        /* 触发回调 */
        if (bwe->callback) {
            bwe->callback(bwe, old_bitrate, new_bitrate, bwe->quality, 
                         bwe->callback_user_data);
        }
    }
    
    /* 平滑更新当前比特率 */
    float alpha = 0.2f;
    bwe->current_bitrate = (uint32_t)(
        bwe->current_bitrate * (1.0f - alpha) + 
        bwe->target_bitrate * alpha
    );
}

/* ============================================
 * API 实现
 * ============================================ */

void voice_bwe_on_packet_sent(
    voice_bwe_t *bwe,
    uint16_t sequence,
    size_t size,
    uint32_t timestamp
) {
    if (!bwe) return;
    
    /* 记录发送的包 */
    size_t pos = bwe->history_pos % BWE_HISTORY_SIZE;
    bwe->send_history[pos].sequence = sequence;
    bwe->send_history[pos].size = size;
    bwe->send_history[pos].send_time = timestamp ? timestamp : get_time_ms();
    bwe->send_history[pos].acked = false;
    bwe->history_pos++;
    
    bwe->bytes_sent_window += size;
    bwe->total_sent++;
}

voice_error_t voice_bwe_on_feedback(
    voice_bwe_t *bwe,
    const voice_bwe_feedback_t *feedback
) {
    if (!bwe || !feedback) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    /* 更新统计 */
    if (feedback->packets_sent > 0) {
        bwe->packet_loss_rate = (float)feedback->packets_lost / 
                                (float)feedback->packets_sent;
    }
    
    bwe->rtt_ms = feedback->rtt_ms;
    bwe->jitter_ms = feedback->jitter_ms;
    
    /* 累积丢包统计 */
    bwe->total_lost += feedback->packets_lost;
    
    /* 评估网络质量 */
    bwe->quality = evaluate_quality(
        bwe->packet_loss_rate,
        bwe->rtt_ms,
        bwe->jitter_ms
    );
    
    /* 调整比特率 */
    adjust_bitrate(bwe);
    
    return VOICE_SUCCESS;
}

voice_error_t voice_bwe_get_estimate(
    voice_bwe_t *bwe,
    voice_bwe_estimate_t *estimate
) {
    if (!bwe || !estimate) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    memset(estimate, 0, sizeof(voice_bwe_estimate_t));
    
    estimate->estimated_bitrate = bwe->current_bitrate;
    estimate->target_bitrate = bwe->target_bitrate;
    estimate->packet_loss_rate = bwe->packet_loss_rate;
    estimate->rtt_ms = bwe->rtt_ms;
    estimate->jitter_ms = bwe->jitter_ms;
    estimate->quality = bwe->quality;
    estimate->should_adjust = (bwe->current_bitrate != bwe->target_bitrate);
    
    return VOICE_SUCCESS;
}

uint32_t voice_bwe_get_target_bitrate(voice_bwe_t *bwe) {
    return bwe ? bwe->target_bitrate : 0;
}

voice_network_quality_t voice_bwe_get_network_quality(voice_bwe_t *bwe) {
    return bwe ? bwe->quality : VOICE_NETWORK_BAD;
}

void voice_bwe_reset(voice_bwe_t *bwe) {
    if (!bwe) return;
    
    bwe->current_bitrate = bwe->config.initial_bitrate;
    bwe->target_bitrate = bwe->config.initial_bitrate;
    bwe->quality = VOICE_NETWORK_GOOD;
    
    bwe->packet_loss_rate = 0.0f;
    bwe->rtt_ms = 0;
    bwe->jitter_ms = 0;
    
    memset(bwe->send_history, 0, sizeof(bwe->send_history));
    bwe->history_pos = 0;
    bwe->total_sent = 0;
    bwe->total_lost = 0;
    bwe->bytes_sent_window = 0;
    
    bwe->last_adjust_time = 0;
    bwe->in_decrease_phase = false;
}

void voice_bwe_set_callback(
    voice_bwe_t *bwe,
    voice_bwe_callback_t callback,
    void *user_data
) {
    if (!bwe) return;
    
    bwe->callback = callback;
    bwe->callback_user_data = user_data;
}
