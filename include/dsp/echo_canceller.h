/**
 * @file echo_canceller.h
 * @brief Acoustic Echo Cancellation (AEC) interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1 Enhancement:
 * - Frequency-domain NLMS adaptive filter
 * - Double-talk detection (DTD)
 * - Integrated delay estimator (GCC-PHAT)
 */

#ifndef DSP_ECHO_CANCELLER_H
#define DSP_ECHO_CANCELLER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 回声消除器
 * ============================================ */

/** 回声消除器句柄 */
typedef struct voice_aec_s voice_aec_t;

/** AEC 算法类型 */
typedef enum {
    VOICE_AEC_ALG_NLMS,         /**< 时域 NLMS */
    VOICE_AEC_ALG_FDAF,         /**< 频域自适应滤波 (推荐) */
    VOICE_AEC_ALG_SPEEX,        /**< SpeexDSP 后端 (如可用) */
} voice_aec_algorithm_t;

/** 双讲检测状态 */
typedef enum {
    VOICE_DTD_IDLE,             /**< 无活动 */
    VOICE_DTD_FAR_END,          /**< 仅远端说话 */
    VOICE_DTD_NEAR_END,         /**< 仅近端说话 */
    VOICE_DTD_DOUBLE_TALK,      /**< 双讲 */
} voice_dtd_state_t;

/** 回声消除器扩展配置 (包含额外选项) */
typedef struct {
    uint32_t sample_rate;               /**< 采样率 */
    uint32_t frame_size;                /**< 帧大小(样本数) */
    uint32_t filter_length;             /**< 滤波器长度(样本数) */
    int echo_suppress_db;               /**< 回声抑制量 (负dB) */
    int echo_suppress_active_db;        /**< 近端活跃时抑制量 */
    bool enable_residual_echo_suppress; /**< 启用残余回声抑制 */
    bool enable_comfort_noise;          /**< 启用舒适噪声 */

    /* Phase 1 新增配置 */
    voice_aec_algorithm_t algorithm;    /**< 算法类型 */
    bool enable_delay_estimation;       /**< 启用自动延迟估算 */
    bool enable_dtd;                    /**< 启用双讲检测 */
    uint32_t tail_length_ms;            /**< 回声尾长(ms), 默认200 */
    float nlms_step_size;               /**< NLMS 步长 (0.1-1.0) */
    float dtd_threshold;                /**< DTD 阈值 (0.5-0.9) */
} voice_aec_ext_config_t;

/** AEC 状态信息 */
typedef struct {
    voice_dtd_state_t dtd_state;        /**< 当前双讲状态 */
    int32_t estimated_delay_samples;    /**< 估计的延迟(样本数) */
    float   estimated_delay_ms;         /**< 估计的延迟(ms) */
    float   erle_db;                    /**< 回声返回损耗增强(dB) */
    float   convergence;                /**< 滤波器收敛度(0-1) */
    bool    delay_stable;               /**< 延迟估计是否稳定 */
    uint64_t frames_processed;          /**< 已处理帧数 */
} voice_aec_state_t;

/**
 * @brief 初始化默认配置
 * @param config 配置结构指针
 */
void voice_aec_ext_config_init(voice_aec_ext_config_t *config);

/**
 * @brief 创建回声消除器
 * @param config 配置
 * @return 回声消除器句柄
 */
voice_aec_t *voice_aec_create(const voice_aec_ext_config_t *config);

/**
 * @brief 销毁回声消除器
 * @param aec 回声消除器句柄
 */
void voice_aec_destroy(voice_aec_t *aec);

/**
 * @brief 处理回声消除 (同步模式)
 *
 * 要求播放信号和麦克风信号时间对齐
 *
 * @param aec 回声消除器句柄
 * @param mic_input 麦克风输入 (含回声)
 * @param speaker_ref 扬声器参考信号
 * @param output 输出 (回声消除后)
 * @param frame_count 帧数(样本数)
 * @return 错误码
 */
voice_error_t voice_aec_process(
    voice_aec_t *aec,
    const int16_t *mic_input,
    const int16_t *speaker_ref,
    int16_t *output,
    size_t frame_count
);

/**
 * @brief 播放端回调 (异步模式)
 *
 * 在播放线程中调用，缓存播放数据用于AEC
 *
 * @param aec 回声消除器句柄
 * @param speaker_data 扬声器数据
 * @param frame_count 帧数
 * @return 错误码
 */
voice_error_t voice_aec_playback(
    voice_aec_t *aec,
    const int16_t *speaker_data,
    size_t frame_count
);

/**
 * @brief 采集端回调 (异步模式)
 *
 * 在采集线程中调用，处理回声消除
 *
 * @param aec 回声消除器句柄
 * @param mic_input 麦克风输入
 * @param output 输出
 * @param frame_count 帧数
 * @return 错误码
 */
voice_error_t voice_aec_capture(
    voice_aec_t *aec,
    const int16_t *mic_input,
    int16_t *output,
    size_t frame_count
);

/**
 * @brief 设置回声抑制量
 * @param aec 回声消除器句柄
 * @param suppress_db 抑制量 (负dB)
 * @param suppress_active_db 近端活跃时抑制量
 * @return 错误码
 */
voice_error_t voice_aec_set_suppress(
    voice_aec_t *aec,
    int suppress_db,
    int suppress_active_db
);

/**
 * @brief 启用/禁用回声消除
 * @param aec 回声消除器句柄
 * @param enabled 是否启用
 * @return 错误码
 */
voice_error_t voice_aec_set_enabled(voice_aec_t *aec, bool enabled);

/**
 * @brief 检查是否启用
 * @param aec 回声消除器句柄
 * @return true 已启用
 */
bool voice_aec_is_enabled(voice_aec_t *aec);

/**
 * @brief 重置回声消除器状态
 * @param aec 回声消除器句柄
 */
void voice_aec_reset(voice_aec_t *aec);

/**
 * @brief 获取滤波器延迟估计
 * @param aec 回声消除器句柄
 * @return 延迟(样本数)
 */
int voice_aec_get_delay(voice_aec_t *aec);

/**
 * @brief 设置已知的系统延迟
 *
 * 当从硬件API获得精确延迟时使用
 *
 * @param aec 回声消除器句柄
 * @param delay_samples 延迟(样本数)
 * @return 错误码
 */
voice_error_t voice_aec_set_delay(voice_aec_t *aec, int delay_samples);

/**
 * @brief 获取当前AEC状态
 *
 * @param aec 回声消除器句柄
 * @param state 状态输出
 * @return 错误码
 */
voice_error_t voice_aec_get_state(voice_aec_t *aec, voice_aec_state_t *state);

/**
 * @brief 获取当前双讲检测状态
 *
 * @param aec 回声消除器句柄
 * @return DTD 状态
 */
voice_dtd_state_t voice_aec_get_dtd_state(voice_aec_t *aec);

/**
 * @brief 启用/禁用延迟估算
 *
 * @param aec 回声消除器句柄
 * @param enabled 是否启用
 * @return 错误码
 */
voice_error_t voice_aec_enable_delay_estimation(voice_aec_t *aec, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* DSP_ECHO_CANCELLER_H */
