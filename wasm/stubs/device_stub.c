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
    return VOICE_SUCCESS;
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
 * 设备枚举（桩实现）
 * ============================================ */

voice_error_t voice_device_enumerate(
    voice_device_mode_t mode,
    voice_device_info_t *devices,
    size_t *count)
{
    (void)mode;

    if (!count) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* WebAssembly 返回虚拟设备 */
    if (devices && *count > 0) {
        strncpy(devices[0].id, "wasm-default", sizeof(devices[0].id) - 1);
        strncpy(devices[0].name, "WebAssembly Audio", sizeof(devices[0].name) - 1);
        devices[0].is_default = true;
        *count = 1;
    } else {
        *count = 1;
    }

    return VOICE_SUCCESS;
}

uint32_t voice_device_get_capture_count(void) {
    return 1;
}

uint32_t voice_device_get_playback_count(void) {
    return 1;
}

voice_error_t voice_device_get_capture_info(uint32_t index, voice_device_enum_info_t *info) {
    if (!info || index > 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    memset(info, 0, sizeof(voice_device_enum_info_t));
    strncpy(info->id, "wasm-capture", sizeof(info->id) - 1);
    strncpy(info->name, "WebAssembly Capture", sizeof(info->name) - 1);
    info->is_default = true;

    return VOICE_SUCCESS;
}

voice_error_t voice_device_get_playback_info(uint32_t index, voice_device_enum_info_t *info) {
    if (!info || index > 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    memset(info, 0, sizeof(voice_device_enum_info_t));
    strncpy(info->id, "wasm-playback", sizeof(info->id) - 1);
    strncpy(info->name, "WebAssembly Playback", sizeof(info->name) - 1);
    info->is_default = true;

    return VOICE_SUCCESS;
}

/* ============================================
 * 设备描述初始化
 * ============================================ */

void voice_device_desc_init(voice_device_desc_t *desc, voice_device_mode_t mode) {
    if (!desc) return;

    memset(desc, 0, sizeof(voice_device_desc_t));
    desc->mode = mode;

    /* 默认配置 */
    desc->capture.sample_rate = 48000;
    desc->capture.channels = 1;
    desc->capture.format = VOICE_FORMAT_S16;

    desc->playback.sample_rate = 48000;
    desc->playback.channels = 2;
    desc->playback.format = VOICE_FORMAT_S16;

    desc->period_size_frames = 480;
    desc->periods = 4;
}

void voice_device_config_init(voice_device_ext_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_device_ext_config_t));
    config->mode = VOICE_DEVICE_MODE_DUPLEX;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_size = 480;
}

/* ============================================
 * 设备对象（桩实现）
 * ============================================ */

struct voice_device_s {
    voice_device_desc_t desc;
    bool running;
};

voice_device_t *voice_device_create(const voice_device_desc_t *desc) {
    if (!desc) {
        return NULL;
    }

    voice_device_t *device = (voice_device_t *)calloc(1, sizeof(voice_device_t));
    if (!device) {
        return NULL;
    }

    device->desc = *desc;
    device->running = false;

    return device;
}

voice_device_t *voice_device_create_simple(const voice_device_ext_config_t *config) {
    if (!config) {
        return NULL;
    }

    voice_device_desc_t desc;
    voice_device_desc_init(&desc, config->mode);
    desc.capture.sample_rate = config->sample_rate;
    desc.capture.channels = config->channels;
    desc.playback.sample_rate = config->sample_rate;
    desc.playback.channels = config->channels;
    desc.period_size_frames = config->frame_size;

    return voice_device_create(&desc);
}

void voice_device_destroy(voice_device_t *device) {
    if (device) {
        free(device);
    }
}

voice_error_t voice_device_start(voice_device_t *device) {
    if (!device) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* WebAssembly: 实际启动在 JavaScript 层处理 */
    device->running = true;
    return VOICE_SUCCESS;
}

voice_error_t voice_device_stop(voice_device_t *device) {
    if (!device) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    device->running = false;
    return VOICE_SUCCESS;
}

bool voice_device_is_started(voice_device_t *device) {
    return device ? device->running : false;
}

voice_device_mode_t voice_device_get_mode(voice_device_t *device) {
    return device ? device->desc.mode : VOICE_DEVICE_MODE_CAPTURE;
}

uint32_t voice_device_get_sample_rate(voice_device_t *device) {
    return device ? device->desc.capture.sample_rate : 48000;
}

uint8_t voice_device_get_channels(voice_device_t *device, voice_device_mode_t mode) {
    if (!device) return 1;
    if (mode == VOICE_DEVICE_MODE_PLAYBACK) {
        return device->desc.playback.channels;
    }
    return device->desc.capture.channels;
}

#endif /* __EMSCRIPTEN__ */
