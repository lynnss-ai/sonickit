/**
 * @file wasm_platform.c
 * @brief WebAssembly 平台实现
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "voice/platform.h"
#include <string.h>

#ifdef __EMSCRIPTEN__

#include <emscripten.h>

/* ============================================
 * 平台检测
 * ============================================ */

voice_platform_t voice_platform_get(void) {
    return VOICE_PLATFORM_WASM;
}

const char *voice_platform_name(voice_platform_t platform) {
    switch (platform) {
        case VOICE_PLATFORM_WASM: return "WebAssembly";
        case VOICE_PLATFORM_WINDOWS: return "Windows";
        case VOICE_PLATFORM_MACOS: return "macOS";
        case VOICE_PLATFORM_LINUX: return "Linux";
        case VOICE_PLATFORM_IOS: return "iOS";
        case VOICE_PLATFORM_ANDROID: return "Android";
        default: return "Unknown";
    }
}

/* ============================================
 * 系统信息
 * ============================================ */

bool voice_platform_is_mobile(void) {
    return false;  /* WebAssembly 运行在浏览器中 */
}

bool voice_platform_is_desktop(void) {
    return true;  /* 将 WASM 视为桌面平台 */
}

uint32_t voice_platform_get_processor_count(void) {
    /* WebAssembly 通常是单线程，除非启用 SharedArrayBuffer */
    return 1;
}

/* ============================================
 * 音频会话管理（WASM 不需要）
 * ============================================ */

voice_error_t voice_platform_audio_session_init(void) {
    return VOICE_ERROR_NONE;  /* 不需要初始化 */
}

void voice_platform_audio_session_cleanup(void) {
    /* 不需要清理 */
}

voice_error_t voice_platform_audio_session_set_category(voice_audio_category_t category) {
    (void)category;
    return VOICE_ERROR_NONE;  /* WebAssembly 不需要音频会话管理 */
}

voice_error_t voice_platform_audio_session_set_mode(voice_audio_mode_t mode) {
    (void)mode;
    return VOICE_ERROR_NONE;
}

bool voice_platform_audio_session_is_active(void) {
    return true;  /* 总是激活 */
}

/* ============================================
 * 权限管理（WASM 通过浏览器权限 API）
 * ============================================ */

voice_error_t voice_platform_request_audio_permission(void) {
    /* 在 JavaScript 层通过 getUserMedia 请求权限 */
    return VOICE_ERROR_NONE;
}

bool voice_platform_has_audio_permission(void) {
    /* 权限由浏览器管理 */
    return true;
}

/* ============================================
 * 后台运行（WASM 不支持）
 * ============================================ */

voice_error_t voice_platform_enable_background_audio(bool enable) {
    (void)enable;
    return VOICE_ERROR_NOT_SUPPORTED;
}

bool voice_platform_is_background_audio_enabled(void) {
    return false;
}

/* ============================================
 * 电源管理（WASM 不支持直接控制）
 * ============================================ */

voice_error_t voice_platform_prevent_sleep(bool prevent) {
    (void)prevent;
    /* 可以通过 JavaScript Wake Lock API 实现 */
    return VOICE_ERROR_NOT_SUPPORTED;
}

bool voice_platform_is_sleep_prevented(void) {
    return false;
}

/* ============================================
 * 低延迟模式（WASM 依赖 AudioWorklet）
 * ============================================ */

voice_error_t voice_platform_enable_low_latency(bool enable) {
    (void)enable;
    /* 低延迟由 AudioWorklet 配置决定 */
    return VOICE_ERROR_NONE;
}

/* ============================================
 * 最优参数（推荐用于实时音频处理）
 * ============================================ */

voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frame_size,
    uint32_t *num_channels
) {
    if (!sample_rate || !frame_size || !num_channels) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* WebAssembly/Web Audio API 推荐参数 */
    *sample_rate = 48000;     /* Web Audio 标准采样率 */
    *frame_size = 128;        /* AudioWorklet 默认缓冲大小 */
    *num_channels = 1;        /* 单声道语音 */
    
    return VOICE_ERROR_NONE;
}

/* ============================================
 * 音频中断处理（WASM 不适用）
 * ============================================ */

void voice_platform_set_interruption_callback(
    voice_platform_interruption_callback_t callback,
    void *user_data
) {
    (void)callback;
    (void)user_data;
    /* WebAssembly 不需要处理音频中断 */
}

/* ============================================
 * 路由变化（WASM 由浏览器处理）
 * ============================================ */

void voice_platform_set_route_change_callback(
    voice_platform_route_change_callback_t callback,
    void *user_data
) {
    (void)callback;
    (void)user_data;
    /* 音频路由由浏览器管理 */
}

/* ============================================
 * 系统音量（WASM 无法直接访问）
 * ============================================ */

float voice_platform_get_system_volume(void) {
    return 1.0f;  /* 无法获取系统音量，返回默认值 */
}

voice_error_t voice_platform_set_system_volume(float volume) {
    (void)volume;
    return VOICE_ERROR_NOT_SUPPORTED;
}

/* ============================================
 * 平台特定信息
 * ============================================ */

const char *voice_platform_get_os_version(void) {
    return "Browser";  /* 返回浏览器环境 */
}

const char *voice_platform_get_device_model(void) {
    return "WebAssembly";
}

/* ============================================
 * 调试支持
 * ============================================ */

void voice_platform_log(voice_log_level_t level, const char *tag, const char *message) {
    /* 使用 Emscripten 的控制台输出 */
    const char *level_str = "INFO";
    switch (level) {
        case VOICE_LOG_LEVEL_VERBOSE: level_str = "VERBOSE"; break;
        case VOICE_LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case VOICE_LOG_LEVEL_INFO: level_str = "INFO"; break;
        case VOICE_LOG_LEVEL_WARN: level_str = "WARN"; break;
        case VOICE_LOG_LEVEL_ERROR: level_str = "ERROR"; break;
        default: break;
    }
    
    EM_ASM({
        var level = UTF8ToString($0);
        var tag = UTF8ToString($1);
        var msg = UTF8ToString($2);
        var prefix = '[' + level + '][' + tag + '] ';
        
        if (level === 'ERROR') {
            console.error(prefix + msg);
        } else if (level === 'WARN') {
            console.warn(prefix + msg);
        } else {
            console.log(prefix + msg);
        }
    }, level_str, tag, message);
}

#endif /* __EMSCRIPTEN__ */
