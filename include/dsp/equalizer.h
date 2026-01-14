/**
 * @file equalizer.h
 * @brief Multi-band parametric equalizer
 * 
 * 提供多频段参数均衡器，用于调整音频频谱响应
 */

#ifndef VOICE_EQUALIZER_H
#define VOICE_EQUALIZER_H

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
 * @brief 滤波器类型
 */
typedef enum {
    VOICE_EQ_LOWPASS,       /**< 低通滤波器 */
    VOICE_EQ_HIGHPASS,      /**< 高通滤波器 */
    VOICE_EQ_BANDPASS,      /**< 带通滤波器 */
    VOICE_EQ_NOTCH,         /**< 陷波滤波器 */
    VOICE_EQ_PEAK,          /**< 峰值滤波器 (参数均衡) */
    VOICE_EQ_LOWSHELF,      /**< 低架滤波器 */
    VOICE_EQ_HIGHSHELF      /**< 高架滤波器 */
} voice_eq_filter_type_t;

/**
 * @brief 均衡器频段配置
 */
typedef struct {
    bool enabled;                   /**< 频段是否启用 */
    voice_eq_filter_type_t type;    /**< 滤波器类型 */
    float frequency;                /**< 中心/截止频率 (Hz) */
    float gain_db;                  /**< 增益 (dB) [-24, +24] */
    float q;                        /**< Q值 (品质因数) [0.1, 10] */
} voice_eq_band_t;

/**
 * @brief 预设类型
 */
typedef enum {
    VOICE_EQ_PRESET_FLAT,           /**< 平坦响应 */
    VOICE_EQ_PRESET_VOICE_ENHANCE,  /**< 语音增强 */
    VOICE_EQ_PRESET_TELEPHONE,      /**< 电话音质 (300-3400Hz) */
    VOICE_EQ_PRESET_BASS_BOOST,     /**< 低音增强 */
    VOICE_EQ_PRESET_TREBLE_BOOST,   /**< 高音增强 */
    VOICE_EQ_PRESET_REDUCE_NOISE,   /**< 降噪倾向 (衰减高频) */
    VOICE_EQ_PRESET_CLARITY,        /**< 清晰度增强 */
    VOICE_EQ_PRESET_CUSTOM          /**< 自定义 */
} voice_eq_preset_t;

/**
 * @brief 均衡器配置
 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 */
    uint32_t num_bands;         /**< 频段数量 [1, 10] */
    voice_eq_band_t *bands;     /**< 频段配置数组 */
    float master_gain_db;       /**< 主增益 (dB) */
} voice_eq_config_t;

/* ============================================
 * 前向声明
 * ============================================ */

typedef struct voice_eq_s voice_eq_t;

/* ============================================
 * 配置初始化
 * ============================================ */

/**
 * @brief 初始化均衡器配置为默认值
 */
void voice_eq_config_init(voice_eq_config_t *config);

/**
 * @brief 从预设加载配置
 */
voice_error_t voice_eq_config_from_preset(
    voice_eq_config_t *config,
    voice_eq_preset_t preset,
    uint32_t sample_rate
);

/* ============================================
 * 均衡器生命周期
 * ============================================ */

/**
 * @brief 创建均衡器实例
 */
voice_eq_t *voice_eq_create(const voice_eq_config_t *config);

/**
 * @brief 销毁均衡器实例
 */
void voice_eq_destroy(voice_eq_t *eq);

/* ============================================
 * 处理接口
 * ============================================ */

/**
 * @brief 处理音频数据 (原地处理)
 */
voice_error_t voice_eq_process(
    voice_eq_t *eq,
    int16_t *samples,
    size_t num_samples
);

/**
 * @brief 处理浮点音频数据
 */
voice_error_t voice_eq_process_float(
    voice_eq_t *eq,
    float *samples,
    size_t num_samples
);

/* ============================================
 * 频段控制
 * ============================================ */

/**
 * @brief 设置频段参数
 */
voice_error_t voice_eq_set_band(
    voice_eq_t *eq,
    uint32_t band_index,
    const voice_eq_band_t *band
);

/**
 * @brief 获取频段参数
 */
voice_error_t voice_eq_get_band(
    voice_eq_t *eq,
    uint32_t band_index,
    voice_eq_band_t *band
);

/**
 * @brief 启用/禁用频段
 */
voice_error_t voice_eq_enable_band(
    voice_eq_t *eq,
    uint32_t band_index,
    bool enable
);

/**
 * @brief 设置主增益
 */
voice_error_t voice_eq_set_master_gain(voice_eq_t *eq, float gain_db);

/**
 * @brief 应用预设
 */
voice_error_t voice_eq_apply_preset(
    voice_eq_t *eq,
    voice_eq_preset_t preset
);

/* ============================================
 * 状态查询
 * ============================================ */

/**
 * @brief 获取频率响应
 * @param frequencies 频率点数组 (Hz)
 * @param responses 响应数组 (dB)，输出参数
 * @param count 数组大小
 */
voice_error_t voice_eq_get_frequency_response(
    voice_eq_t *eq,
    const float *frequencies,
    float *responses,
    size_t count
);

/**
 * @brief 重置均衡器状态
 */
void voice_eq_reset(voice_eq_t *eq);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_EQUALIZER_H */
