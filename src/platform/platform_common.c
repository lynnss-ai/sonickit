/**
 * @file platform_common.c
 * @brief Platform-independent implementations
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "voice/platform.h"
#include <string.h>

void voice_session_config_init(voice_session_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(voice_session_config_t));
    config->category = VOICE_SESSION_CATEGORY_PLAY_AND_RECORD;
    config->mode = VOICE_SESSION_MODE_VOICE_CHAT;
    config->options = VOICE_SESSION_OPTION_ALLOW_BLUETOOTH | 
                      VOICE_SESSION_OPTION_DEFAULT_TO_SPEAKER;
    config->preferred_sample_rate = 48000;
    config->preferred_io_buffer_duration = 0.02f; /* 20ms */
}

voice_platform_t voice_platform_get(void)
{
#if defined(_WIN32) || defined(_WIN64)
    return VOICE_PLATFORM_WINDOWS;
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE
        return VOICE_PLATFORM_IOS;
    #else
        return VOICE_PLATFORM_MACOS;
    #endif
#elif defined(__ANDROID__)
    return VOICE_PLATFORM_ANDROID;
#elif defined(__linux__)
    return VOICE_PLATFORM_LINUX;
#else
    return VOICE_PLATFORM_UNKNOWN;
#endif
}

const char *voice_platform_name(voice_platform_t platform)
{
    switch (platform) {
    case VOICE_PLATFORM_WINDOWS: return "Windows";
    case VOICE_PLATFORM_MACOS:   return "macOS";
    case VOICE_PLATFORM_LINUX:   return "Linux";
    case VOICE_PLATFORM_IOS:     return "iOS";
    case VOICE_PLATFORM_ANDROID: return "Android";
    default:                     return "Unknown";
    }
}

/* ============================================
 * Default implementations (non-mobile platforms)
 * ============================================ */

#if !defined(__APPLE__) || !(TARGET_OS_IPHONE)
#if !defined(__ANDROID__)

voice_error_t voice_session_configure(const voice_session_config_t *config)
{
    (void)config;
    return VOICE_OK;
}

voice_error_t voice_session_activate(void)
{
    return VOICE_OK;
}

voice_error_t voice_session_deactivate(void)
{
    return VOICE_OK;
}

voice_audio_route_t voice_session_get_current_route(void)
{
    return VOICE_AUDIO_ROUTE_UNKNOWN;
}

voice_error_t voice_session_override_output(voice_audio_route_t route)
{
    (void)route;
    return VOICE_ERROR_NOT_SUPPORTED;
}

static voice_interrupt_callback_t g_interrupt_cb = NULL;
static void *g_interrupt_cb_data = NULL;
static voice_route_change_callback_t g_route_change_cb = NULL;
static void *g_route_change_cb_data = NULL;

void voice_session_set_interrupt_callback(
    voice_interrupt_callback_t callback,
    void *user_data)
{
    g_interrupt_cb = callback;
    g_interrupt_cb_data = user_data;
}

void voice_session_set_route_change_callback(
    voice_route_change_callback_t callback,
    void *user_data)
{
    g_route_change_cb = callback;
    g_route_change_cb_data = user_data;
}

bool voice_session_request_mic_permission(
    void (*callback)(bool granted, void *user_data),
    void *user_data)
{
    /* Desktop platforms don't typically require explicit permission */
    if (callback) {
        callback(true, user_data);
    }
    return true;
}

voice_permission_status_t voice_session_get_mic_permission(void)
{
    return VOICE_PERMISSION_GRANTED;
}

voice_error_t voice_platform_enable_low_latency(bool enable)
{
    (void)enable;
    return VOICE_OK;
}

voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frames_per_buffer)
{
    if (sample_rate) *sample_rate = 48000;
    if (frames_per_buffer) *frames_per_buffer = 960; /* 20ms at 48kHz */
    return VOICE_OK;
}

voice_error_t voice_platform_set_bluetooth_sco(bool enable)
{
    (void)enable;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_platform_acquire_wake_lock(void)
{
    return VOICE_OK;
}

voice_error_t voice_platform_release_wake_lock(void)
{
    return VOICE_OK;
}

#ifdef _WIN32
#include <windows.h>
#include <avrt.h>
#pragma comment(lib, "avrt.lib")

voice_error_t voice_platform_set_realtime_priority(void)
{
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    return hTask ? VOICE_OK : VOICE_ERROR_SYSTEM;
}
#else
#include <pthread.h>
#include <sched.h>

voice_error_t voice_platform_set_realtime_priority(void)
{
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    
    int ret = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    if (ret != 0) {
        /* Fall back to high priority if FIFO not allowed */
        param.sched_priority = sched_get_priority_max(SCHED_RR);
        ret = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }
    
    return ret == 0 ? VOICE_OK : VOICE_ERROR_SYSTEM;
}
#endif /* _WIN32 */

#endif /* !__ANDROID__ */
#endif /* !iOS */
