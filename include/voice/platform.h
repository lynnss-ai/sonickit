/**
 * @file platform.h
 * @brief Cross-platform audio abstraction layer
 * 
 * 提供跨平台音频会话管理和设备控制
 */

#ifndef VOICE_PLATFORM_H
#define VOICE_PLATFORM_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 平台类型
 * ============================================ */

typedef enum {
    VOICE_PLATFORM_UNKNOWN = 0,
    VOICE_PLATFORM_WINDOWS,
    VOICE_PLATFORM_MACOS,
    VOICE_PLATFORM_LINUX,
    VOICE_PLATFORM_IOS,
    VOICE_PLATFORM_ANDROID,
} voice_platform_t;

/** 获取当前平台 */
voice_platform_t voice_platform_get(void);

/** 获取平台名称 */
const char *voice_platform_name(voice_platform_t platform);

/* ============================================
 * 音频会话 (主要用于移动平台)
 * ============================================ */

/** 音频会话类别 */
typedef enum {
    VOICE_SESSION_CATEGORY_AMBIENT,        /**< 可与其他音频混合 */
    VOICE_SESSION_CATEGORY_SOLO_AMBIENT,   /**< 独占播放 */
    VOICE_SESSION_CATEGORY_PLAYBACK,       /**< 媒体播放 */
    VOICE_SESSION_CATEGORY_RECORD,         /**< 录音 */
    VOICE_SESSION_CATEGORY_PLAY_AND_RECORD,/**< 录制和播放 (VoIP) */
    VOICE_SESSION_CATEGORY_MULTI_ROUTE,    /**< 多路由 */
} voice_session_category_t;

/** 音频会话模式 */
typedef enum {
    VOICE_SESSION_MODE_DEFAULT,
    VOICE_SESSION_MODE_VOICE_CHAT,     /**< VoIP优化 */
    VOICE_SESSION_MODE_VIDEO_CHAT,     /**< 视频通话 */
    VOICE_SESSION_MODE_GAME_CHAT,      /**< 游戏语音 */
    VOICE_SESSION_MODE_VOICE_PROMPT,   /**< 语音提示 */
    VOICE_SESSION_MODE_MEASUREMENT,    /**< 音频测量 */
} voice_session_mode_t;

/** 音频会话选项 */
typedef enum {
    VOICE_SESSION_OPTION_NONE                 = 0,
    VOICE_SESSION_OPTION_MIX_WITH_OTHERS      = 1 << 0,
    VOICE_SESSION_OPTION_DUCK_OTHERS          = 1 << 1,
    VOICE_SESSION_OPTION_ALLOW_BLUETOOTH      = 1 << 2,
    VOICE_SESSION_OPTION_DEFAULT_TO_SPEAKER   = 1 << 3,
    VOICE_SESSION_OPTION_INTERRUPT_SPOKEN     = 1 << 4,
} voice_session_options_t;

/** 音频路由 */
typedef enum {
    VOICE_AUDIO_ROUTE_UNKNOWN = 0,
    VOICE_AUDIO_ROUTE_BUILTIN_SPEAKER,
    VOICE_AUDIO_ROUTE_BUILTIN_RECEIVER,
    VOICE_AUDIO_ROUTE_HEADPHONES,
    VOICE_AUDIO_ROUTE_BLUETOOTH_A2DP,
    VOICE_AUDIO_ROUTE_BLUETOOTH_HFP,
    VOICE_AUDIO_ROUTE_BLUETOOTH_LE,
    VOICE_AUDIO_ROUTE_USB,
    VOICE_AUDIO_ROUTE_HDMI,
    VOICE_AUDIO_ROUTE_LINE_OUT,
    VOICE_AUDIO_ROUTE_CAR_PLAY,
    VOICE_AUDIO_ROUTE_AIR_PLAY,
} voice_audio_route_t;

/* ============================================
 * 音频会话配置
 * ============================================ */

typedef struct {
    voice_session_category_t category;
    voice_session_mode_t mode;
    uint32_t options;   /**< voice_session_options_t 组合 */
    uint32_t preferred_sample_rate;
    float preferred_io_buffer_duration; /**< 秒 */
} voice_session_config_t;

/** 初始化默认配置 */
void voice_session_config_init(voice_session_config_t *config);

/* ============================================
 * 音频会话回调
 * ============================================ */

/** 中断类型 */
typedef enum {
    VOICE_INTERRUPT_BEGAN,      /**< 中断开始 */
    VOICE_INTERRUPT_ENDED,      /**< 中断结束 */
} voice_interrupt_type_t;

/** 中断原因 */
typedef enum {
    VOICE_INTERRUPT_REASON_DEFAULT,
    VOICE_INTERRUPT_REASON_APP_SUSPENDED,
    VOICE_INTERRUPT_REASON_BUILT_IN_MIC_MUTED,
    VOICE_INTERRUPT_REASON_ROUTE_CHANGE,
} voice_interrupt_reason_t;

/** 路由变化原因 */
typedef enum {
    VOICE_ROUTE_CHANGE_UNKNOWN,
    VOICE_ROUTE_CHANGE_NEW_DEVICE,
    VOICE_ROUTE_CHANGE_OLD_DEVICE_UNAVAILABLE,
    VOICE_ROUTE_CHANGE_CATEGORY_CHANGE,
    VOICE_ROUTE_CHANGE_OVERRIDE,
    VOICE_ROUTE_CHANGE_WAKE_FROM_SLEEP,
    VOICE_ROUTE_CHANGE_NO_SUITABLE_ROUTE,
    VOICE_ROUTE_CHANGE_CONFIG_CHANGE,
} voice_route_change_reason_t;

/** 中断回调 */
typedef void (*voice_interrupt_callback_t)(
    voice_interrupt_type_t type,
    voice_interrupt_reason_t reason,
    bool should_resume,
    void *user_data
);

/** 路由变化回调 */
typedef void (*voice_route_change_callback_t)(
    voice_route_change_reason_t reason,
    voice_audio_route_t new_route,
    void *user_data
);

/* ============================================
 * 音频会话 API
 * ============================================ */

/**
 * @brief 配置音频会话
 * @note 在 iOS 上调用 AVAudioSession，其他平台为空操作
 */
voice_error_t voice_session_configure(const voice_session_config_t *config);

/**
 * @brief 激活音频会话
 */
voice_error_t voice_session_activate(void);

/**
 * @brief 停用音频会话
 */
voice_error_t voice_session_deactivate(void);

/**
 * @brief 获取当前音频路由
 */
voice_audio_route_t voice_session_get_current_route(void);

/**
 * @brief 覆盖输出端口
 */
voice_error_t voice_session_override_output(voice_audio_route_t route);

/**
 * @brief 设置中断回调
 */
void voice_session_set_interrupt_callback(
    voice_interrupt_callback_t callback,
    void *user_data
);

/**
 * @brief 设置路由变化回调
 */
void voice_session_set_route_change_callback(
    voice_route_change_callback_t callback,
    void *user_data
);

/**
 * @brief 请求麦克风权限 (异步)
 * @return true 如果已有权限，false 表示需要等待回调
 */
bool voice_session_request_mic_permission(
    void (*callback)(bool granted, void *user_data),
    void *user_data
);

/**
 * @brief 检查麦克风权限状态
 */
typedef enum {
    VOICE_PERMISSION_UNKNOWN,
    VOICE_PERMISSION_GRANTED,
    VOICE_PERMISSION_DENIED,
    VOICE_PERMISSION_RESTRICTED,
} voice_permission_status_t;

voice_permission_status_t voice_session_get_mic_permission(void);

/* ============================================
 * 移动平台低延迟优化
 * ============================================ */

/**
 * @brief 启用低延迟模式 (Android AAudio)
 */
voice_error_t voice_platform_enable_low_latency(bool enable);

/**
 * @brief 获取实际的采样率和缓冲区大小
 */
voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frames_per_buffer
);

/**
 * @brief 设置蓝牙SCO模式
 */
voice_error_t voice_platform_set_bluetooth_sco(bool enable);

/* ============================================
 * CPU/电源管理
 * ============================================ */

/**
 * @brief 获取/释放音频处理唤醒锁
 */
voice_error_t voice_platform_acquire_wake_lock(void);
voice_error_t voice_platform_release_wake_lock(void);

/**
 * @brief 设置实时线程优先级
 */
voice_error_t voice_platform_set_realtime_priority(void);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_PLATFORM_H */
