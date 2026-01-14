/**
 * @file wasm_platform.c
 * @brief WebAssembly 平台实现
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "voice/platform.h"
#include "voice/error.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

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
 * 平台初始化
 * ============================================ */

voice_error_t voice_platform_init(void) {
    return VOICE_SUCCESS;  /* 不需要初始化 */
}

void voice_platform_cleanup(void) {
    /* 不需要清理 */
}

/* ============================================
 * 音频会话（桩实现）
 * ============================================ */

voice_error_t voice_platform_audio_session_set_category(voice_session_category_t category) {
    (void)category;
    return VOICE_SUCCESS;  /* WebAssembly 不需要音频会话管理 */
}

voice_error_t voice_platform_audio_session_set_mode(voice_session_mode_t mode) {
    (void)mode;
    return VOICE_SUCCESS;
}

voice_error_t voice_platform_audio_session_activate(bool active) {
    (void)active;
    return VOICE_SUCCESS;
}

/* ============================================
 * 最优参数查询
 * ============================================ */

voice_error_t voice_platform_get_optimal_parameters(
    uint32_t *sample_rate,
    uint32_t *frame_size)
{
    if (!sample_rate || !frame_size) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* WebAssembly 推荐参数 */
    *sample_rate = 48000;
    *frame_size = 480;  /* 10ms @ 48kHz */

    return VOICE_SUCCESS;
}

/* ============================================
 * 中断处理（桩实现）
 * ============================================ */

voice_error_t voice_platform_set_interruption_handler(
    voice_interrupt_callback_t callback,
    void *user_data)
{
    (void)callback;
    (void)user_data;
    /* WebAssembly 不需要中断处理 */
    return VOICE_SUCCESS;
}

/* ============================================
 * 路由变更处理（桩实现）
 * ============================================ */

voice_error_t voice_platform_set_route_change_handler(
    voice_route_change_callback_t callback,
    void *user_data)
{
    (void)callback;
    (void)user_data;
    /* WebAssembly 不需要路由变更处理 */
    return VOICE_SUCCESS;
}

/* ============================================
 * 日志输出
 * ============================================ */

static voice_log_level_t g_log_level = VOICE_LOG_INFO;

void voice_set_log_level(voice_log_level_t level) {
    g_log_level = level;
}

voice_log_level_t voice_get_log_level(void) {
    return g_log_level;
}

void voice_log(voice_log_level_t level, const char *fmt, ...) {
    if (level < g_log_level) {
        return;
    }

    const char *level_str;
    switch (level) {
        case VOICE_LOG_DEBUG: level_str = "DEBUG"; break;
        case VOICE_LOG_INFO: level_str = "INFO"; break;
        case VOICE_LOG_WARN: level_str = "WARN"; break;
        case VOICE_LOG_ERROR: level_str = "ERROR"; break;
        default: level_str = "UNKNOWN"; break;
    }

    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    /* 使用 Emscripten 的 console 输出 */
    EM_ASM({
        var level = UTF8ToString($0);
        var msg = UTF8ToString($1);
        if (level === "ERROR") {
            console.error("[SonicKit]", msg);
        } else if (level === "WARN") {
            console.warn("[SonicKit]", msg);
        } else if (level === "DEBUG") {
            console.debug("[SonicKit]", msg);
        } else {
            console.log("[SonicKit]", msg);
        }
    }, level_str, buffer);
}

#endif /* __EMSCRIPTEN__ */
