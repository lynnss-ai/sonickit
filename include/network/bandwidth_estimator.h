/**
 * @file bandwidth_estimator.h
 * @brief Network bandwidth estimation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 网络带宽估计器，用于自适应比特率控制
 */

#ifndef VOICE_BANDWIDTH_ESTIMATOR_H
#define VOICE_BANDWIDTH_ESTIMATOR_H

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

typedef struct voice_bwe_s voice_bwe_t;

/** 网络状况 */
typedef enum {
    VOICE_NETWORK_EXCELLENT,   /**< 优秀 (< 1% 丢包, < 50ms 延迟) */
    VOICE_NETWORK_GOOD,        /**< 良好 (< 3% 丢包, < 100ms 延迟) */
    VOICE_NETWORK_FAIR,        /**< 一般 (< 10% 丢包, < 200ms 延迟) */
    VOICE_NETWORK_POOR,        /**< 较差 (< 20% 丢包, < 400ms 延迟) */
    VOICE_NETWORK_BAD,         /**< 很差 (>= 20% 丢包 或 >= 400ms 延迟) */
} voice_network_quality_t;

/* ============================================
 * 配置
 * ============================================ */

typedef struct {
    uint32_t initial_bitrate;       /**< 初始比特率 */
    uint32_t min_bitrate;           /**< 最小比特率 */
    uint32_t max_bitrate;           /**< 最大比特率 */
    
    /* 估计窗口 */
    uint32_t window_size_ms;        /**< 统计窗口大小 */
    
    /* AIMD 参数 */
    float increase_rate;            /**< 增加速率 (加法增加) */
    float decrease_factor;          /**< 减少因子 (乘法减少) */
    
    /* 阈值 */
    float loss_threshold_increase;  /**< 允许增加的丢包阈值 */
    float loss_threshold_decrease;  /**< 触发减少的丢包阈值 */
    uint32_t rtt_threshold_ms;      /**< RTT 阈值 */
    
    /* 稳定期 */
    uint32_t hold_time_ms;          /**< 调整后的稳定期 */
} voice_bwe_config_t;

/* ============================================
 * 反馈数据
 * ============================================ */

typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_lost;
    uint32_t bytes_sent;
    uint32_t rtt_ms;
    uint32_t jitter_ms;
    uint32_t timestamp;             /**< 反馈时间戳 */
} voice_bwe_feedback_t;

/* ============================================
 * 估计结果
 * ============================================ */

typedef struct {
    uint32_t estimated_bitrate;     /**< 估计的可用带宽 */
    uint32_t target_bitrate;        /**< 建议的目标比特率 */
    float packet_loss_rate;         /**< 丢包率 */
    uint32_t rtt_ms;                /**< 往返延迟 */
    uint32_t jitter_ms;             /**< 抖动 */
    voice_network_quality_t quality;/**< 网络质量 */
    bool should_adjust;             /**< 是否需要调整比特率 */
} voice_bwe_estimate_t;

/* ============================================
 * BWE API
 * ============================================ */

/**
 * @brief 初始化默认配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_bwe_config_init(voice_bwe_config_t *config);

/**
 * @brief 创建带宽估计器
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_bwe_t *voice_bwe_create(const voice_bwe_config_t *config);

/**
 * @brief 销毁带宽估计器
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_bwe_destroy(voice_bwe_t *bwe);

/**
 * @brief 更新发送统计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_bwe_on_packet_sent(
    voice_bwe_t *bwe,
    uint16_t sequence,
    size_t size,
    uint32_t timestamp
);

/**
 * @brief 处理接收反馈 (RTCP RR)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_bwe_on_feedback(
    voice_bwe_t *bwe,
    const voice_bwe_feedback_t *feedback
);

/**
 * @brief 获取带宽估计
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_bwe_get_estimate(
    voice_bwe_t *bwe,
    voice_bwe_estimate_t *estimate
);

/**
 * @brief 获取建议比特率
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint32_t voice_bwe_get_target_bitrate(voice_bwe_t *bwe);

/**
 * @brief 获取网络质量
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_network_quality_t voice_bwe_get_network_quality(voice_bwe_t *bwe);

/**
 * @brief 重置估计器
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_bwe_reset(voice_bwe_t *bwe);

/**
 * @brief 带宽变化回调
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*voice_bwe_callback_t)(
    voice_bwe_t *bwe,
    uint32_t old_bitrate,
    uint32_t new_bitrate,
    voice_network_quality_t quality,
    void *user_data
);

void voice_bwe_set_callback(
    voice_bwe_t *bwe,
    voice_bwe_callback_t callback,
    void *user_data
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_BANDWIDTH_ESTIMATOR_H */
