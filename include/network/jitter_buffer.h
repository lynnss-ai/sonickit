/**
 * @file jitter_buffer.h
 * @brief Jitter Buffer and Packet Loss Concealment interface
 */

#ifndef NETWORK_JITTER_BUFFER_H
#define NETWORK_JITTER_BUFFER_H

#include "voice/types.h"
#include "voice/error.h"
#include "network/rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Jitter Buffer 常量
 * ============================================ */

#define JITTER_BUFFER_DEFAULT_CAPACITY  100   /**< 默认容量(包数) */
#define JITTER_BUFFER_DEFAULT_MIN_DELAY 20    /**< 默认最小延迟(ms) */
#define JITTER_BUFFER_DEFAULT_MAX_DELAY 200   /**< 默认最大延迟(ms) */

/* ============================================
 * Jitter Buffer 类型
 * ============================================ */

typedef struct jitter_buffer_s jitter_buffer_t;

/** 抖动缓冲模式 */
typedef enum {
    JITTER_MODE_FIXED,      /**< 固定延迟 */
    JITTER_MODE_ADAPTIVE,   /**< 自适应延迟 */
} jitter_mode_t;

/** 抖动缓冲配置 */
typedef struct {
    uint32_t clock_rate;        /**< 时钟频率 */
    uint32_t frame_duration_ms; /**< 帧时长(ms) */
    jitter_mode_t mode;         /**< 缓冲模式 */
    uint32_t min_delay_ms;      /**< 最小延迟(ms) */
    uint32_t max_delay_ms;      /**< 最大延迟(ms) */
    uint32_t initial_delay_ms;  /**< 初始延迟(ms) */
    uint32_t capacity;          /**< 容量(包数) */
    bool enable_plc;            /**< 启用PLC */
} jitter_buffer_config_t;

/** 包状态 */
typedef enum {
    JITTER_PACKET_OK,           /**< 正常 */
    JITTER_PACKET_LOST,         /**< 丢失 */
    JITTER_PACKET_LATE,         /**< 太迟 */
    JITTER_PACKET_EARLY,        /**< 太早 */
    JITTER_PACKET_DUPLICATE,    /**< 重复 */
    JITTER_PACKET_INTERPOLATED, /**< 插值生成 */
} jitter_packet_status_t;

/** 抖动缓冲统计 */
typedef struct {
    uint64_t packets_received;
    uint64_t packets_output;
    uint64_t packets_lost;
    uint64_t packets_late;
    uint64_t packets_early;
    uint64_t packets_duplicate;
    uint64_t packets_interpolated;
    uint32_t current_delay_ms;
    uint32_t min_delay_observed_ms;
    uint32_t max_delay_observed_ms;
    float loss_rate;
} jitter_buffer_stats_t;

/* ============================================
 * Jitter Buffer API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void jitter_buffer_config_init(jitter_buffer_config_t *config);

/**
 * @brief 创建抖动缓冲
 */
jitter_buffer_t *jitter_buffer_create(const jitter_buffer_config_t *config);

/**
 * @brief 销毁抖动缓冲
 */
void jitter_buffer_destroy(jitter_buffer_t *jb);

/**
 * @brief 添加包到缓冲区
 * 
 * @param jb 抖动缓冲句柄
 * @param data 包数据
 * @param size 包大小
 * @param timestamp RTP时间戳
 * @param sequence 序列号
 * @param marker 标记位
 * @return 错误码
 */
voice_error_t jitter_buffer_put(
    jitter_buffer_t *jb,
    const uint8_t *data,
    size_t size,
    uint32_t timestamp,
    uint16_t sequence,
    bool marker
);

/**
 * @brief 从缓冲区获取包
 * 
 * 此函数应定期调用，每次调用间隔等于 frame_duration_ms
 * 
 * @param jb 抖动缓冲句柄
 * @param output 输出缓冲区
 * @param max_size 缓冲区大小
 * @param out_size 实际输出大小
 * @param status 包状态
 * @return 错误码
 */
voice_error_t jitter_buffer_get(
    jitter_buffer_t *jb,
    uint8_t *output,
    size_t max_size,
    size_t *out_size,
    jitter_packet_status_t *status
);

/**
 * @brief 设置目标延迟
 */
voice_error_t jitter_buffer_set_delay(
    jitter_buffer_t *jb,
    uint32_t delay_ms
);

/**
 * @brief 获取当前延迟
 */
uint32_t jitter_buffer_get_delay(jitter_buffer_t *jb);

/**
 * @brief 获取统计信息
 */
voice_error_t jitter_buffer_get_stats(
    jitter_buffer_t *jb,
    jitter_buffer_stats_t *stats
);

/**
 * @brief 重置统计信息
 */
void jitter_buffer_reset_stats(jitter_buffer_t *jb);

/**
 * @brief 重置缓冲区
 */
void jitter_buffer_reset(jitter_buffer_t *jb);

/**
 * @brief 获取缓冲区中的包数
 */
size_t jitter_buffer_get_count(jitter_buffer_t *jb);

/* ============================================
 * PLC (Packet Loss Concealment) API
 * ============================================ */

typedef struct voice_plc_s voice_plc_t;

/** PLC 算法类型 */
typedef enum {
    PLC_ALG_ZERO,           /**< 静音填充 */
    PLC_ALG_REPEAT,         /**< 重复上一帧 */
    PLC_ALG_FADE,           /**< 渐变衰减 */
    PLC_ALG_INTERPOLATE,    /**< 波形插值 */
} plc_algorithm_t;

/** PLC 配置 */
typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    plc_algorithm_t algorithm;
    uint32_t max_consecutive_loss;  /**< 最大连续丢包数 */
} voice_plc_config_t;

/**
 * @brief 初始化PLC默认配置
 */
void voice_plc_config_init(voice_plc_config_t *config);

/**
 * @brief 创建PLC处理器
 */
voice_plc_t *voice_plc_create(const voice_plc_config_t *config);

/**
 * @brief 销毁PLC处理器
 */
void voice_plc_destroy(voice_plc_t *plc);

/**
 * @brief 更新好帧 (用于PLC参考)
 */
void voice_plc_update_good_frame(
    voice_plc_t *plc,
    const int16_t *pcm,
    size_t samples
);

/**
 * @brief 生成丢包补偿帧
 */
voice_error_t voice_plc_generate(
    voice_plc_t *plc,
    int16_t *output,
    size_t samples
);

/**
 * @brief 重置PLC状态
 */
void voice_plc_reset(voice_plc_t *plc);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_JITTER_BUFFER_H */
