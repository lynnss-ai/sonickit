/**
 * @file compressor.h
 * @brief Dynamic range compressor/expander/limiter
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 提供动态范围处理功能：压缩、扩展、限制
 */

#ifndef VOICE_COMPRESSOR_H
#define VOICE_COMPRESSOR_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 类型定义
 * ============================================ */

/**
 * @brief 处理器类型
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    VOICE_DRC_COMPRESSOR,       /**< 压缩器 (减少动态范围) */
    VOICE_DRC_EXPANDER,         /**< 扩展器 (增加动态范围) */
    VOICE_DRC_LIMITER,          /**< 限制器 (硬限制峰值) */
    VOICE_DRC_GATE              /**< 噪声门 (静音低电平) */
} voice_drc_type_t;

/**
 * @brief 检测模式
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    VOICE_DRC_PEAK,             /**< 峰值检测 */
    VOICE_DRC_RMS,              /**< RMS 检测 */
    VOICE_DRC_TRUE_PEAK         /**< 真峰值检测 (过采样) */
} voice_drc_detection_t;

/**
 * @brief 拐点模式
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    VOICE_DRC_HARD_KNEE,        /**< 硬拐点 */
    VOICE_DRC_SOFT_KNEE         /**< 软拐点 */
} voice_drc_knee_t;

/**
 * @brief 压缩器配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef struct {
    /* 基本参数 */
    voice_drc_type_t type;          /**< 处理器类型 */
    uint32_t sample_rate;           /**< 采样率 */
    
    /* 电平参数 */
    float threshold_db;             /**< 阈值 (dBFS) [-60, 0] */
    float ratio;                    /**< 压缩比 [1:1, ∞:1] (压缩器: >1, 扩展器: <1) */
    float knee_width_db;            /**< 软拐点宽度 (dB) [0, 24] */
    voice_drc_knee_t knee_type;     /**< 拐点类型 */
    
    /* 时间参数 */
    float attack_ms;                /**< 启动时间 (ms) [0.1, 100] */
    float release_ms;               /**< 释放时间 (ms) [10, 5000] */
    float hold_ms;                  /**< 保持时间 (ms) [0, 500] */
    
    /* 增益参数 */
    float makeup_gain_db;           /**< 补偿增益 (dB) */
    bool auto_makeup;               /**< 自动补偿增益 */
    
    /* 检测参数 */
    voice_drc_detection_t detection; /**< 检测模式 */
    float lookahead_ms;             /**< 前瞻时间 (ms) [0, 10] */
    
    /* 侧链 */
    bool enable_sidechain;          /**< 启用外部侧链 */
    float sidechain_hpf;            /**< 侧链高通滤波频率 (Hz), 0=禁用 */
} voice_compressor_config_t;

/**
 * @brief 压缩器状态
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef struct {
    float input_level_db;       /**< 输入电平 (dBFS) */
    float output_level_db;      /**< 输出电平 (dBFS) */
    float gain_reduction_db;    /**< 增益衰减量 (dB) */
    float current_ratio;        /**< 当前压缩比 */
    bool is_compressing;        /**< 是否正在压缩 */
} voice_compressor_state_t;

/* ============================================
 * 前向声明
 * ============================================ */

typedef struct voice_compressor_s voice_compressor_t;

/* ============================================
 * 配置初始化
 * ============================================ */

/**
 * @brief 初始化压缩器配置 (默认为语音压缩)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_compressor_config_init(voice_compressor_config_t *config);

/**
 * @brief 初始化限制器配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_limiter_config_init(voice_compressor_config_t *config);

/**
 * @brief 初始化噪声门配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_gate_config_init(voice_compressor_config_t *config);

/* ============================================
 * 生命周期
 * ============================================ */

/**
 * @brief 创建压缩器实例
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_compressor_t *voice_compressor_create(const voice_compressor_config_t *config);

/**
 * @brief 销毁压缩器实例
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_compressor_destroy(voice_compressor_t *comp);

/* ============================================
 * 处理接口
 * ============================================ */

/**
 * @brief 处理音频数据 (原地处理)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_process(
    voice_compressor_t *comp,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief 处理浮点音频数据
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_process_float(
    voice_compressor_t *comp,
    float *samples,
    size_t num_samples
);

/**
 * @brief 使用外部侧链信号处理
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_process_sidechain(
    voice_compressor_t *comp,
    int16_t *samples,
    const int16_t *sidechain,
    size_t num_samples
);

/* ============================================
 * 参数控制
 * ============================================ */

/**
 * @brief 设置阈值
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_set_threshold(
    voice_compressor_t *comp,
    float threshold_db
);

/**
 * @brief 设置压缩比
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_set_ratio(
    voice_compressor_t *comp,
    float ratio
);

/**
 * @brief 设置启动/释放时间
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_set_times(
    voice_compressor_t *comp,
    float attack_ms,
    float release_ms
);

/**
 * @brief 设置补偿增益
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_set_makeup_gain(
    voice_compressor_t *comp,
    float gain_db
);

/* ============================================
 * 状态查询
 * ============================================ */

/**
 * @brief 获取压缩器状态
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_compressor_get_state(
    voice_compressor_t *comp,
    voice_compressor_state_t *state
);

/**
 * @brief 重置压缩器状态
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_compressor_reset(voice_compressor_t *comp);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_COMPRESSOR_H */
