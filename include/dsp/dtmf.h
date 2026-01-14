/**
 * @file dtmf.h
 * @brief DTMF (Dual-Tone Multi-Frequency) detection and generation
 * 
 * DTMF 信号检测和生成模块
 * 用于电话系统按键音的编解码
 */

#ifndef VOICE_DTMF_H
#define VOICE_DTMF_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * DTMF 字符定义
 * ============================================ */

/** 
 * DTMF 字符
 * 标准按键: 0-9, *, #
 * 扩展按键: A, B, C, D
 */
typedef enum {
    VOICE_DTMF_0 = '0',
    VOICE_DTMF_1 = '1',
    VOICE_DTMF_2 = '2',
    VOICE_DTMF_3 = '3',
    VOICE_DTMF_4 = '4',
    VOICE_DTMF_5 = '5',
    VOICE_DTMF_6 = '6',
    VOICE_DTMF_7 = '7',
    VOICE_DTMF_8 = '8',
    VOICE_DTMF_9 = '9',
    VOICE_DTMF_STAR = '*',
    VOICE_DTMF_HASH = '#',
    VOICE_DTMF_A = 'A',
    VOICE_DTMF_B = 'B',
    VOICE_DTMF_C = 'C',
    VOICE_DTMF_D = 'D',
    VOICE_DTMF_NONE = 0,
} voice_dtmf_digit_t;

/* ============================================
 * DTMF 检测器
 * ============================================ */

typedef struct voice_dtmf_detector_s voice_dtmf_detector_t;

/** 检测器配置 */
typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;
    
    /* Goertzel 参数 */
    float detection_threshold;      /**< 检测阈值 */
    float twist_threshold;          /**< 高低频能量比阈值 */
    float reverse_twist_threshold;  /**< 反向 twist 阈值 */
    
    /* 时间参数 */
    uint32_t min_on_time_ms;        /**< 最小按键持续时间 */
    uint32_t min_off_time_ms;       /**< 最小按键间隔时间 */
    
    /* 回调 */
    void (*on_digit)(voice_dtmf_digit_t digit, uint32_t duration_ms, void *user_data);
    void *callback_user_data;
} voice_dtmf_detector_config_t;

/** 检测结果 */
typedef struct {
    voice_dtmf_digit_t digit;       /**< 检测到的数字 */
    bool valid;                     /**< 是否有效 */
    float low_freq_energy;          /**< 低频能量 */
    float high_freq_energy;         /**< 高频能量 */
    float twist;                    /**< 能量比 */
    uint32_t duration_ms;           /**< 持续时间 */
} voice_dtmf_result_t;

/* ============================================
 * DTMF 生成器
 * ============================================ */

typedef struct voice_dtmf_generator_s voice_dtmf_generator_t;

/** 生成器配置 */
typedef struct {
    uint32_t sample_rate;
    float amplitude;                /**< 振幅 (0.0-1.0) */
    float low_freq_gain;            /**< 低频增益调整 */
    float high_freq_gain;           /**< 高频增益调整 */
    uint32_t tone_duration_ms;      /**< 音调持续时间 */
    uint32_t pause_duration_ms;     /**< 音调间隔时间 */
} voice_dtmf_generator_config_t;

/* ============================================
 * DTMF 检测器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_dtmf_detector_config_init(voice_dtmf_detector_config_t *config);

/**
 * @brief 创建检测器
 */
voice_dtmf_detector_t *voice_dtmf_detector_create(
    const voice_dtmf_detector_config_t *config
);

/**
 * @brief 销毁检测器
 */
void voice_dtmf_detector_destroy(voice_dtmf_detector_t *detector);

/**
 * @brief 处理音频帧
 * @param detector 检测器
 * @param samples 音频样本
 * @param num_samples 样本数
 * @param result 检测结果 (可选)
 * @return 检测到的数字，未检测到返回 VOICE_DTMF_NONE
 */
voice_dtmf_digit_t voice_dtmf_detector_process(
    voice_dtmf_detector_t *detector,
    const int16_t *samples,
    size_t num_samples,
    voice_dtmf_result_t *result
);

/**
 * @brief 重置检测器
 */
void voice_dtmf_detector_reset(voice_dtmf_detector_t *detector);

/**
 * @brief 获取检测到的数字序列
 * @param detector 检测器
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 数字个数
 */
size_t voice_dtmf_detector_get_digits(
    voice_dtmf_detector_t *detector,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief 清除数字缓冲区
 */
void voice_dtmf_detector_clear_digits(voice_dtmf_detector_t *detector);

/* ============================================
 * DTMF 生成器 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 */
void voice_dtmf_generator_config_init(voice_dtmf_generator_config_t *config);

/**
 * @brief 创建生成器
 */
voice_dtmf_generator_t *voice_dtmf_generator_create(
    const voice_dtmf_generator_config_t *config
);

/**
 * @brief 销毁生成器
 */
void voice_dtmf_generator_destroy(voice_dtmf_generator_t *generator);

/**
 * @brief 生成单个 DTMF 音调
 * @param generator 生成器
 * @param digit 数字
 * @param output 输出缓冲区
 * @param num_samples 样本数
 * @return 生成的样本数
 */
size_t voice_dtmf_generator_generate(
    voice_dtmf_generator_t *generator,
    voice_dtmf_digit_t digit,
    int16_t *output,
    size_t num_samples
);

/**
 * @brief 生成 DTMF 序列
 * @param generator 生成器
 * @param digits 数字序列 (以 '\0' 结尾)
 * @param output 输出缓冲区
 * @param max_samples 缓冲区大小
 * @return 生成的样本数
 */
size_t voice_dtmf_generator_generate_sequence(
    voice_dtmf_generator_t *generator,
    const char *digits,
    int16_t *output,
    size_t max_samples
);

/**
 * @brief 重置生成器
 */
void voice_dtmf_generator_reset(voice_dtmf_generator_t *generator);

/* ============================================
 * 辅助函数
 * ============================================ */

/**
 * @brief 验证 DTMF 字符是否有效
 */
bool voice_dtmf_is_valid_digit(char c);

/**
 * @brief 获取 DTMF 频率
 * @param digit 数字
 * @param low_freq 输出低频率
 * @param high_freq 输出高频率
 */
void voice_dtmf_get_frequencies(
    voice_dtmf_digit_t digit,
    float *low_freq,
    float *high_freq
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_DTMF_H */
