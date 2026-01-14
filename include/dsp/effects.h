/**
 * @file effects.h
 * @brief Audio effects processing (reverb, delay, pitch shift, chorus, flanger)
 */

#ifndef DSP_EFFECTS_H
#define DSP_EFFECTS_H

#include "voice/types.h"
#include "voice/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 混响效果 (Reverb)
 * ============================================ */

/** 混响类型 */
typedef enum {
    VOICE_REVERB_ROOM,          /**< 小房间 */
    VOICE_REVERB_HALL,          /**< 音乐厅 */
    VOICE_REVERB_PLATE,         /**< 板式混响 */
    VOICE_REVERB_CHAMBER,       /**< 混响室 */
    VOICE_REVERB_CATHEDRAL      /**< 大教堂 */
} voice_reverb_type_t;

/** 混响配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 */
    voice_reverb_type_t type;   /**< 混响类型 */
    float room_size;            /**< 房间大小 (0.0-1.0) */
    float damping;              /**< 阻尼 (0.0-1.0) */
    float wet_level;            /**< 湿声比例 (0.0-1.0) */
    float dry_level;            /**< 干声比例 (0.0-1.0) */
    float width;                /**< 立体声宽度 (0.0-1.0) */
    float pre_delay_ms;         /**< 预延迟 (毫秒) */
} voice_reverb_config_t;

/** 混响句柄 */
typedef struct voice_reverb_s voice_reverb_t;

/**
 * @brief 初始化默认混响配置
 */
void voice_reverb_config_init(voice_reverb_config_t *config);

/**
 * @brief 创建混响效果器
 */
voice_reverb_t *voice_reverb_create(const voice_reverb_config_t *config);

/**
 * @brief 销毁混响效果器
 */
void voice_reverb_destroy(voice_reverb_t *reverb);

/**
 * @brief 处理音频 (原地)
 */
voice_error_t voice_reverb_process(
    voice_reverb_t *reverb,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频 (int16)
 */
voice_error_t voice_reverb_process_int16(
    voice_reverb_t *reverb,
    int16_t *samples,
    size_t count
);

/**
 * @brief 设置混响参数
 */
void voice_reverb_set_room_size(voice_reverb_t *reverb, float size);
void voice_reverb_set_damping(voice_reverb_t *reverb, float damping);
void voice_reverb_set_wet_level(voice_reverb_t *reverb, float level);
void voice_reverb_set_dry_level(voice_reverb_t *reverb, float level);

/**
 * @brief 重置混响状态
 */
void voice_reverb_reset(voice_reverb_t *reverb);

/* ============================================
 * 延迟效果 (Delay)
 * ============================================ */

/** 延迟类型 */
typedef enum {
    VOICE_DELAY_SIMPLE,         /**< 简单延迟 */
    VOICE_DELAY_PINGPONG,       /**< 乒乓延迟 (立体声) */
    VOICE_DELAY_TAPE,           /**< 磁带延迟 (带调制) */
    VOICE_DELAY_MULTI_TAP       /**< 多抽头延迟 */
} voice_delay_type_t;

/** 延迟配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 */
    voice_delay_type_t type;    /**< 延迟类型 */
    float delay_ms;             /**< 延迟时间 (毫秒) */
    float feedback;             /**< 反馈量 (0.0-1.0) */
    float mix;                  /**< 混合比例 (0.0-1.0) */
    float modulation_rate;      /**< 调制速率 (Hz, 仅磁带延迟) */
    float modulation_depth;     /**< 调制深度 (0.0-1.0) */
    uint32_t max_delay_ms;      /**< 最大延迟 (毫秒) */
} voice_delay_config_t;

/** 延迟句柄 */
typedef struct voice_delay_s voice_delay_t;

/**
 * @brief 初始化默认延迟配置
 */
void voice_delay_config_init(voice_delay_config_t *config);

/**
 * @brief 创建延迟效果器
 */
voice_delay_t *voice_delay_create(const voice_delay_config_t *config);

/**
 * @brief 销毁延迟效果器
 */
void voice_delay_destroy(voice_delay_t *delay);

/**
 * @brief 处理音频 (原地)
 */
voice_error_t voice_delay_process(
    voice_delay_t *delay,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频 (int16)
 */
voice_error_t voice_delay_process_int16(
    voice_delay_t *delay,
    int16_t *samples,
    size_t count
);

/**
 * @brief 设置延迟时间
 */
void voice_delay_set_time(voice_delay_t *delay, float delay_ms);

/**
 * @brief 设置反馈量
 */
void voice_delay_set_feedback(voice_delay_t *delay, float feedback);

/**
 * @brief 重置延迟状态
 */
void voice_delay_reset(voice_delay_t *delay);

/* ============================================
 * 音高变换 (Pitch Shift)
 * ============================================ */

/** 音高变换配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 */
    float semitones;            /**< 半音偏移 (-12 到 +12) */
    float cents;                /**< 音分微调 (-100 到 +100) */
    uint32_t frame_size;        /**< 帧大小 (样本数) */
    uint32_t overlap;           /**< 重叠因子 (通常 4 或 8) */
    bool preserve_formants;     /**< 保持共振峰 (用于语音) */
} voice_pitch_shift_config_t;

/** 音高变换句柄 */
typedef struct voice_pitch_shift_s voice_pitch_shift_t;

/**
 * @brief 初始化默认音高变换配置
 */
void voice_pitch_shift_config_init(voice_pitch_shift_config_t *config);

/**
 * @brief 创建音高变换器
 */
voice_pitch_shift_t *voice_pitch_shift_create(const voice_pitch_shift_config_t *config);

/**
 * @brief 销毁音高变换器
 */
void voice_pitch_shift_destroy(voice_pitch_shift_t *pitch);

/**
 * @brief 处理音频 (原地)
 */
voice_error_t voice_pitch_shift_process(
    voice_pitch_shift_t *pitch,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频 (int16)
 */
voice_error_t voice_pitch_shift_process_int16(
    voice_pitch_shift_t *pitch,
    int16_t *samples,
    size_t count
);

/**
 * @brief 设置音高偏移
 * @param semitones 半音 (-12 到 +12)
 * @param cents 音分 (-100 到 +100)
 */
void voice_pitch_shift_set_shift(
    voice_pitch_shift_t *pitch,
    float semitones,
    float cents
);

/**
 * @brief 重置音高变换器状态
 */
void voice_pitch_shift_reset(voice_pitch_shift_t *pitch);

/* ============================================
 * 合唱效果 (Chorus)
 * ============================================ */

/** 合唱配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 */
    uint8_t voices;             /**< 声部数量 (1-8) */
    float rate;                 /**< LFO 频率 (Hz) */
    float depth;                /**< 调制深度 (毫秒) */
    float mix;                  /**< 混合比例 (0.0-1.0) */
    float feedback;             /**< 反馈量 (0.0-1.0) */
    float delay_ms;             /**< 基础延迟 (毫秒) */
} voice_chorus_config_t;

/** 合唱句柄 */
typedef struct voice_chorus_s voice_chorus_t;

/**
 * @brief 初始化默认合唱配置
 */
void voice_chorus_config_init(voice_chorus_config_t *config);

/**
 * @brief 创建合唱效果器
 */
voice_chorus_t *voice_chorus_create(const voice_chorus_config_t *config);

/**
 * @brief 销毁合唱效果器
 */
void voice_chorus_destroy(voice_chorus_t *chorus);

/**
 * @brief 处理音频 (原地)
 */
voice_error_t voice_chorus_process(
    voice_chorus_t *chorus,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频 (int16)
 */
voice_error_t voice_chorus_process_int16(
    voice_chorus_t *chorus,
    int16_t *samples,
    size_t count
);

/**
 * @brief 重置合唱状态
 */
void voice_chorus_reset(voice_chorus_t *chorus);

/* ============================================
 * 镶边效果 (Flanger)
 * ============================================ */

/** 镶边配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 */
    float rate;                 /**< LFO 频率 (Hz) */
    float depth;                /**< 调制深度 (0.0-1.0) */
    float mix;                  /**< 混合比例 (0.0-1.0) */
    float feedback;             /**< 反馈量 (-1.0 到 1.0) */
    float delay_ms;             /**< 基础延迟 (毫秒, 通常 1-10) */
} voice_flanger_config_t;

/** 镶边句柄 */
typedef struct voice_flanger_s voice_flanger_t;

/**
 * @brief 初始化默认镶边配置
 */
void voice_flanger_config_init(voice_flanger_config_t *config);

/**
 * @brief 创建镶边效果器
 */
voice_flanger_t *voice_flanger_create(const voice_flanger_config_t *config);

/**
 * @brief 销毁镶边效果器
 */
void voice_flanger_destroy(voice_flanger_t *flanger);

/**
 * @brief 处理音频 (原地)
 */
voice_error_t voice_flanger_process(
    voice_flanger_t *flanger,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频 (int16)
 */
voice_error_t voice_flanger_process_int16(
    voice_flanger_t *flanger,
    int16_t *samples,
    size_t count
);

/**
 * @brief 重置镶边状态
 */
void voice_flanger_reset(voice_flanger_t *flanger);

/* ============================================
 * 效果链 (Effects Chain)
 * ============================================ */

/** 效果类型 */
typedef enum {
    VOICE_EFFECT_REVERB,
    VOICE_EFFECT_DELAY,
    VOICE_EFFECT_PITCH_SHIFT,
    VOICE_EFFECT_CHORUS,
    VOICE_EFFECT_FLANGER
} voice_effect_type_t;

/** 效果链句柄 */
typedef struct voice_effect_chain_s voice_effect_chain_t;

/**
 * @brief 创建效果链
 */
voice_effect_chain_t *voice_effect_chain_create(uint32_t sample_rate);

/**
 * @brief 销毁效果链
 */
void voice_effect_chain_destroy(voice_effect_chain_t *chain);

/**
 * @brief 添加效果到链
 */
voice_error_t voice_effect_chain_add(
    voice_effect_chain_t *chain,
    voice_effect_type_t type,
    void *config
);

/**
 * @brief 移除效果
 */
voice_error_t voice_effect_chain_remove(
    voice_effect_chain_t *chain,
    size_t index
);

/**
 * @brief 清空效果链
 */
void voice_effect_chain_clear(voice_effect_chain_t *chain);

/**
 * @brief 处理音频通过效果链
 */
voice_error_t voice_effect_chain_process(
    voice_effect_chain_t *chain,
    float *samples,
    size_t count
);

/**
 * @brief 处理音频 (int16)
 */
voice_error_t voice_effect_chain_process_int16(
    voice_effect_chain_t *chain,
    int16_t *samples,
    size_t count
);

/**
 * @brief 绕过效果链
 */
void voice_effect_chain_set_bypass(voice_effect_chain_t *chain, bool bypass);

/**
 * @brief 重置效果链
 */
void voice_effect_chain_reset(voice_effect_chain_t *chain);

#ifdef __cplusplus
}
#endif

#endif /* DSP_EFFECTS_H */
