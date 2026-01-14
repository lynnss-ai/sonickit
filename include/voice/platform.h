/**
 * @file platform.h
 * @brief Cross-platform audio abstraction layer
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Provides cross-platform audio session management and device control
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
 * Platform Types
 * ============================================ */

typedef enum {
    VOICE_PLATFORM_UNKNOWN = 0,
    VOICE_PLATFORM_WINDOWS,
    VOICE_PLATFORM_MACOS,
    VOICE_PLATFORM_LINUX,
    VOICE_PLATFORM_IOS,
    VOICE_PLATFORM_ANDROID,
    VOICE_PLATFORM_WASM,
} voice_platform_t;

/** Get current platform */
voice_platform_t voice_platform_get(void);

/** Get platform name */
const char *voice_platform_name(voice_platform_t platform);

/* ============================================
 * Audio Session (mainly for mobile platforms)
 * ============================================ */

/** Audio session category */
typedef enum {
    VOICE_SESSION_CATEGORY_AMBIENT,        /**< Can mix with other audio */
    VOICE_SESSION_CATEGORY_SOLO_AMBIENT,   /**< Exclusive playback */
    VOICE_SESSION_CATEGORY_PLAYBACK,       /**< Media playback */
    VOICE_SESSION_CATEGORY_RECORD,         /**< Recording */
    VOICE_SESSION_CATEGORY_PLAY_AND_RECORD,/**< Recording and playback (VoIP) */
    VOICE_SESSION_CATEGORY_MULTI_ROUTE,    /**< Multi-route */
} voice_session_category_t;

/** Audio session mode */
typedef enum {
    VOICE_SESSION_MODE_DEFAULT,
    VOICE_SESSION_MODE_VOICE_CHAT,     /**< VoIP optimized */
    VOICE_SESSION_MODE_VIDEO_CHAT,     /**< Video chat */
    VOICE_SESSION_MODE_GAME_CHAT,      /**< Game voice */
    VOICE_SESSION_MODE_VOICE_PROMPT,   /**< Voice prompt */
    VOICE_SESSION_MODE_MEASUREMENT,    /**< Audio measurement */
} voice_session_mode_t;

/** Audio session options */
typedef enum {
    VOICE_SESSION_OPTION_NONE                 = 0,
    VOICE_SESSION_OPTION_MIX_WITH_OTHERS      = 1 << 0,
    VOICE_SESSION_OPTION_DUCK_OTHERS          = 1 << 1,
    VOICE_SESSION_OPTION_ALLOW_BLUETOOTH      = 1 << 2,
    VOICE_SESSION_OPTION_DEFAULT_TO_SPEAKER   = 1 << 3,
    VOICE_SESSION_OPTION_INTERRUPT_SPOKEN     = 1 << 4,
} voice_session_options_t;

/** Audio route */
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
 * Audio Session Configuration
 * ============================================ */

typedef struct {
    voice_session_category_t category;
    voice_session_mode_t mode;
    uint32_t options;   /**< Combination of voice_session_options_t */
    uint32_t preferred_sample_rate;
    float preferred_io_buffer_duration; /**< Seconds */
} voice_session_config_t;

/** Initialize default configuration */
void voice_session_config_init(voice_session_config_t *config);

/* ============================================
 * Audio Session Callbacks
 * ============================================ */

/** Interrupt type */
typedef enum {
    VOICE_INTERRUPT_BEGAN,      /**< Interrupt began */
    VOICE_INTERRUPT_ENDED,      /**< Interrupt ended */
} voice_interrupt_type_t;

/** Interrupt reason */
typedef enum {
    VOICE_INTERRUPT_REASON_DEFAULT,
    VOICE_INTERRUPT_REASON_APP_SUSPENDED,
    VOICE_INTERRUPT_REASON_BUILT_IN_MIC_MUTED,
    VOICE_INTERRUPT_REASON_ROUTE_CHANGE,
} voice_interrupt_reason_t;

/** Route change reason */
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

/** Interrupt callback */
typedef void (*voice_interrupt_callback_t)(
    voice_interrupt_type_t type,
    voice_interrupt_reason_t reason,
    bool should_resume,
    void *user_data
);

/** Route change callback */
typedef void (*voice_route_change_callback_t)(
    voice_route_change_reason_t reason,
    voice_audio_route_t new_route,
    void *user_data
);

/* ============================================
 * Audio Session API
 * ============================================ */

/**
 * @brief Configure audio session
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @note Calls AVAudioSession on iOS, no-op on other platforms
 */
voice_error_t voice_session_configure(const voice_session_config_t *config);

/**
 * @brief Activate audio session
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_session_activate(void);

/**
 * @brief Deactivate audio session
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_session_deactivate(void);

/**
 * @brief Get current audio route
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_audio_route_t voice_session_get_current_route(void);

/**
 * @brief Override output port
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_session_override_output(voice_audio_route_t route);

/**
 * @brief Set interrupt callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_session_set_interrupt_callback(
    voice_interrupt_callback_t callback,
    void *user_data
);

/**
 * @brief Set route change callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_session_set_route_change_callback(
    voice_route_change_callback_t callback,
    void *user_data
);

/**
 * @brief Request microphone permission (async)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return true if already have permission, false means wait for callback
 */
bool voice_session_request_mic_permission(
    void (*callback)(bool granted, void *user_data),
    void *user_data
);

/**
 * @brief Check microphone permission status
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    VOICE_PERMISSION_UNKNOWN,
    VOICE_PERMISSION_GRANTED,
    VOICE_PERMISSION_DENIED,
    VOICE_PERMISSION_RESTRICTED,
} voice_permission_status_t;

voice_permission_status_t voice_session_get_mic_permission(void);

/* ============================================
 * Mobile Platform Low-Latency Optimization
 * ============================================ */

/**
 * @brief Enable low-latency mode (Android AAudio)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_platform_enable_low_latency(bool enable);

/**
 * @brief Get actual sample rate and buffer size
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frames_per_buffer
);

/**
 * @brief Set Bluetooth SCO mode
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_platform_set_bluetooth_sco(bool enable);

/* ============================================
 * CPU/Power Management
 * ============================================ */

/**
 * @brief Acquire/release audio processing wake lock
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_platform_acquire_wake_lock(void);
voice_error_t voice_platform_release_wake_lock(void);

/**
 * @brief Set real-time thread priority
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t voice_platform_set_realtime_priority(void);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_PLATFORM_H */
