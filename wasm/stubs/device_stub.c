/**
 * @file device_stub.c
 * @brief WebAssembly 音频设备桩实现
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * WebAssembly 环境下，音频输入输出由 Web Audio API (AudioWorklet) 处理
 * 此文件提供桩实现，实际音频处理在 JavaScript 层完成
 */

#include "audio/device.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__

/* ============================================
 * 设备上下文（桩实现）
 * ============================================ */

struct voice_device_context_s {
    bool initialized;
};

static voice_device_context_t *g_device_context = NULL;

voice_error_t voice_device_context_init(void) {
    if (g_device_context) {
        return VOICE_ERROR_ALREADY_INITIALIZED;
    }
    
    g_device_context = (voice_device_context_t *)calloc(1, sizeof(voice_device_context_t));
    if (!g_device_context) {
        return VOICE_ERROR_OUT_OF_MEMORY;
    }
    
    g_device_context->initialized = true;
    return VOICE_ERROR_NONE;
}

void voice_device_context_cleanup(void) {
    if (g_device_context) {
        free(g_device_context);
        g_device_context = NULL;
    }
}

voice_device_context_t *voice_device_context_get(void) {
    return g_device_context;
}

/* ============================================
 * 设备枚举（Web Audio API 不提供详细设备信息）
 * ============================================ */

voice_error_t voice_device_enumerate(
    voice_device_type_t type,
    voice_device_info_t **devices,
    uint32_t *count
) {
    if (!devices || !count) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    if (!g_device_context || !g_device_context->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    /* WebAssembly 使用默认设备 */
    *devices = (voice_device_info_t *)calloc(1, sizeof(voice_device_info_t));
    if (!*devices) {
        return VOICE_ERROR_OUT_OF_MEMORY;
    }
    
    (*devices)[0].type = type;
    strncpy((*devices)[0].id, "default", sizeof((*devices)[0].id) - 1);
    strncpy((*devices)[0].name, "Default Audio Device", sizeof((*devices)[0].name) - 1);
    (*devices)[0].is_default = true;
    (*devices)[0].sample_rate = 48000;
    (*devices)[0].channels = (type == VOICE_DEVICE_CAPTURE) ? 1 : 2;
    
    *count = 1;
    return VOICE_ERROR_NONE;
}

void voice_device_free_list(voice_device_info_t *devices) {
    if (devices) {
        free(devices);
    }
}

/* ============================================
 * 设备对象（桩实现）
 * ============================================ */

struct voice_device_s {
    voice_device_desc_t desc;
    bool started;
    bool initialized;
};

void voice_device_desc_init(voice_device_desc_t *desc) {
    if (!desc) return;
    
    memset(desc, 0, sizeof(voice_device_desc_t));
    desc->type = VOICE_DEVICE_DUPLEX;
    desc->sample_rate = 48000;
    desc->channels = 1;
    desc->frame_size = 128;
    desc->capture_callback = NULL;
    desc->playback_callback = NULL;
    desc->user_data = NULL;
}

voice_device_t *voice_device_create(const voice_device_desc_t *desc) {
    if (!desc) {
        return NULL;
    }
    
    if (!g_device_context || !g_device_context->initialized) {
        return NULL;
    }
    
    voice_device_t *device = (voice_device_t *)calloc(1, sizeof(voice_device_t));
    if (!device) {
        return NULL;
    }
    
    memcpy(&device->desc, desc, sizeof(voice_device_desc_t));
    device->initialized = true;
    device->started = false;
    
    return device;
}

void voice_device_destroy(voice_device_t *device) {
    if (device) {
        if (device->started) {
            voice_device_stop(device);
        }
        free(device);
    }
}

voice_error_t voice_device_start(voice_device_t *device) {
    if (!device || !device->initialized) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    if (device->started) {
        return VOICE_ERROR_ALREADY_INITIALIZED;
    }
    
    /* 实际的音频流由 JavaScript AudioWorklet 管理 */
    device->started = true;
    return VOICE_ERROR_NONE;
}

voice_error_t voice_device_stop(voice_device_t *device) {
    if (!device || !device->initialized) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    if (!device->started) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    device->started = false;
    return VOICE_ERROR_NONE;
}

bool voice_device_is_started(const voice_device_t *device) {
    return device && device->initialized && device->started;
}

/* ============================================
 * 音频回调（在 JavaScript 层调用）
 * ============================================ */

voice_error_t voice_device_get_callbacks(
    const voice_device_t *device,
    voice_device_capture_callback_t *capture_cb,
    voice_device_playback_callback_t *playback_cb,
    void **user_data
) {
    if (!device) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    if (capture_cb) {
        *capture_cb = device->desc.capture_callback;
    }
    if (playback_cb) {
        *playback_cb = device->desc.playback_callback;
    }
    if (user_data) {
        *user_data = device->desc.user_data;
    }
    
    return VOICE_ERROR_NONE;
}

/* ============================================
 * 设备信息查询
 * ============================================ */

voice_error_t voice_device_get_info(const voice_device_t *device, voice_device_info_t *info) {
    if (!device || !info) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    memset(info, 0, sizeof(voice_device_info_t));
    info->type = device->desc.type;
    strncpy(info->id, "wasm_default", sizeof(info->id) - 1);
    strncpy(info->name, "WebAssembly Audio Device", sizeof(info->name) - 1);
    info->is_default = true;
    info->sample_rate = device->desc.sample_rate;
    info->channels = device->desc.channels;
    
    return VOICE_ERROR_NONE;
}

voice_error_t voice_device_get_current_config(
    const voice_device_t *device,
    uint32_t *sample_rate,
    uint32_t *channels,
    uint32_t *frame_size
) {
    if (!device) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    if (sample_rate) {
        *sample_rate = device->desc.sample_rate;
    }
    if (channels) {
        *channels = device->desc.channels;
    }
    if (frame_size) {
        *frame_size = device->desc.frame_size;
    }
    
    return VOICE_ERROR_NONE;
}

/* ============================================
 * 延迟信息（Web Audio API 限制）
 * ============================================ */

voice_error_t voice_device_get_latency(const voice_device_t *device, float *latency_ms) {
    if (!device || !latency_ms) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* AudioWorklet 通常有约 3-10ms 延迟 */
    *latency_ms = 5.0f;
    return VOICE_ERROR_NONE;
}

/* ============================================
 * 音量控制（应在 JavaScript 层实现）
 * ============================================ */

voice_error_t voice_device_set_volume(voice_device_t *device, float volume) {
    if (!device) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    if (volume < 0.0f || volume > 1.0f) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* 音量控制在 JavaScript Web Audio API 中实现 */
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_device_get_volume(const voice_device_t *device, float *volume) {
    if (!device || !volume) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    *volume = 1.0f;  /* 默认音量 */
    return VOICE_ERROR_NONE;
}

#endif /* __EMSCRIPTEN__ */
