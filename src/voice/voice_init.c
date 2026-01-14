/**
 * @file voice_init.c
 * @brief Voice library initialization and cleanup
 */

#include "voice/voice.h"
#include "voice/platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* ============================================
 * 全局状态
 * ============================================ */

static struct {
    bool initialized;
    void (*log_callback)(voice_log_level_t, const char*, void*);
    void *log_user_data;
    voice_log_level_t log_level;
    voice_error_t last_error;
} g_voice_state = {0};

/* ============================================
 * 版本信息
 * ============================================ */

const char *voice_version(void)
{
    return VOICE_VERSION_STRING;
}

void voice_version_get(int *major, int *minor, int *patch)
{
    if (major) *major = VOICE_VERSION_MAJOR;
    if (minor) *minor = VOICE_VERSION_MINOR;
    if (patch) *patch = VOICE_VERSION_PATCH;
}

/* ============================================
 * 日志系统
 * ============================================ */

static const char *log_level_names[] = {
    "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

void voice_set_log_callback(
    void (*callback)(voice_log_level_t level, const char *msg, void *user_data),
    void *user_data)
{
    g_voice_state.log_callback = callback;
    g_voice_state.log_user_data = user_data;
}

void voice_set_log_level(voice_log_level_t level)
{
    g_voice_state.log_level = level;
}

voice_log_level_t voice_get_log_level(void)
{
    return g_voice_state.log_level;
}

void voice_log(voice_log_level_t level, const char *fmt, ...)
{
    if (level < g_voice_state.log_level) {
        return;
    }
    
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    
    if (g_voice_state.log_callback) {
        g_voice_state.log_callback(level, message, g_voice_state.log_user_data);
    } else {
        /* 默认输出到 stderr */
        fprintf(stderr, "[%s] %s\n", log_level_names[level], message);
    }
}

/* ============================================
 * 错误处理
 * ============================================ */

voice_error_t voice_get_last_error(void)
{
    return g_voice_state.last_error;
}

void voice_set_last_error(voice_error_t error)
{
    g_voice_state.last_error = error;
}

void voice_clear_error(void)
{
    g_voice_state.last_error = VOICE_OK;
}

/* ============================================
 * 库初始化
 * ============================================ */

voice_error_t voice_init(const voice_global_config_t *config)
{
    if (g_voice_state.initialized) {
        return VOICE_OK;
    }
    
    /* 默认日志级别 */
    g_voice_state.log_level = VOICE_LOG_INFO;
    
    if (config) {
        voice_set_log_level(config->log_level);
    }
    
    /* 平台特定初始化 */
#ifdef _WIN32
    /* COM 初始化 (用于 WASAPI) */
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        VOICE_LOG_E("Failed to initialize COM: 0x%08lX", hr);
        return VOICE_ERROR_NOT_INITIALIZED;
    }
#endif
    
    g_voice_state.initialized = true;
    VOICE_LOG_I("Voice library initialized (version %s)", VOICE_VERSION_STRING);
    VOICE_LOG_I("Platform: %s", voice_platform_name(voice_platform_get()));
    
    return VOICE_OK;
}

void voice_deinit(void)
{
    if (!g_voice_state.initialized) {
        return;
    }
    
#ifdef _WIN32
    CoUninitialize();
#endif
    
    VOICE_LOG_I("Voice library deinitialized");
    g_voice_state.initialized = false;
}

bool voice_is_initialized(void)
{
    return g_voice_state.initialized;
}

/* ============================================
 * 错误信息
 * ============================================ */

const char *voice_error_string(voice_error_t error)
{
    switch (error) {
    case VOICE_OK:                          return "OK";
    case VOICE_ERROR:                       return "Unknown error";
    case VOICE_ERROR_INVALID_PARAM:         return "Invalid parameter";
    case VOICE_ERROR_NULL_POINTER:          return "Null pointer";
    case VOICE_ERROR_OUT_OF_MEMORY:         return "Out of memory";
    case VOICE_ERROR_NOT_INITIALIZED:       return "Not initialized";
    case VOICE_ERROR_ALREADY_INITIALIZED:   return "Already initialized";
    case VOICE_ERROR_NOT_SUPPORTED:         return "Not supported";
    case VOICE_ERROR_TIMEOUT:               return "Timeout";
    case VOICE_ERROR_BUSY:                  return "Busy";
    case VOICE_ERROR_OVERFLOW:              return "Buffer overflow";
    case VOICE_ERROR_UNDERFLOW:             return "Buffer underflow";
    case VOICE_ERROR_DEVICE_NOT_FOUND:      return "Device not found";
    case VOICE_ERROR_DEVICE_OPEN_FAILED:    return "Failed to open device";
    case VOICE_ERROR_DEVICE_START_FAILED:   return "Failed to start device";
    case VOICE_ERROR_DEVICE_STOP_FAILED:    return "Failed to stop device";
    case VOICE_ERROR_CODEC_NOT_FOUND:       return "Codec not found";
    case VOICE_ERROR_CODEC_INIT_FAILED:     return "Codec initialization failed";
    case VOICE_ERROR_CODEC_ENCODE_FAILED:   return "Encoding failed";
    case VOICE_ERROR_CODEC_DECODE_FAILED:   return "Decoding failed";
    case VOICE_ERROR_NETWORK:               return "Network error";
    case VOICE_ERROR_NETWORK_TIMEOUT:       return "Network timeout";
    case VOICE_ERROR_FILE:                  return "File error";
    case VOICE_ERROR_FILE_OPEN_FAILED:      return "Failed to open file";
    case VOICE_ERROR_FILE_READ_FAILED:      return "Failed to read file";
    case VOICE_ERROR_FILE_WRITE_FAILED:     return "Failed to write file";
    case VOICE_ERROR_FILE_CORRUPT:          return "File is corrupt";
    default:                                return "Unknown error code";
    }
}
