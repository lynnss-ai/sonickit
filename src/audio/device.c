/**
 * @file device.c
 * @brief Audio device implementation using miniaudio
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio/device.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_device_context_s {
    ma_context context;
    bool initialized;
    
    /* 设备枚举缓存 */
    ma_device_info *capture_devices;
    uint32_t capture_count;
    ma_device_info *playback_devices;
    uint32_t playback_count;
};

struct voice_device_s {
    ma_device device;
    voice_device_desc_t desc;
    bool started;
    bool initialized;
};

/* 全局设备上下文 */
static voice_device_context_t *g_device_context = NULL;

/* ============================================
 * 设备上下文
 * ============================================ */

voice_error_t voice_device_context_init(void)
{
    if (g_device_context) {
        return VOICE_ERROR_ALREADY_INITIALIZED;
    }
    
    g_device_context = (voice_device_context_t *)calloc(1, sizeof(voice_device_context_t));
    if (!g_device_context) {
        return VOICE_ERROR_OUT_OF_MEMORY;
    }
    
    ma_context_config config = ma_context_config_init();
    
    if (ma_context_init(NULL, 0, &config, &g_device_context->context) != MA_SUCCESS) {
        free(g_device_context);
        g_device_context = NULL;
        return VOICE_ERROR_DEVICE_OPEN_FAILED;
    }
    
    /* 枚举设备 */
    ma_result result = ma_context_get_devices(
        &g_device_context->context,
        &g_device_context->playback_devices,
        &g_device_context->playback_count,
        &g_device_context->capture_devices,
        &g_device_context->capture_count
    );
    
    if (result != MA_SUCCESS) {
        VOICE_LOG_W("Failed to enumerate devices");
    }
    
    g_device_context->initialized = true;
    VOICE_LOG_I("Audio device context initialized");
    
    return VOICE_OK;
}

void voice_device_context_deinit(void)
{
    if (g_device_context) {
        if (g_device_context->initialized) {
            ma_context_uninit(&g_device_context->context);
        }
        free(g_device_context);
        g_device_context = NULL;
        VOICE_LOG_I("Audio device context deinitialized");
    }
}

voice_device_context_t *voice_device_context_get(void)
{
    return g_device_context;
}

/* ============================================
 * miniaudio格式转换
 * ============================================ */

static ma_format voice_format_to_ma(voice_format_t format)
{
    switch (format) {
        case VOICE_FORMAT_U8:  return ma_format_u8;
        case VOICE_FORMAT_S16: return ma_format_s16;
        case VOICE_FORMAT_S24: return ma_format_s24;
        case VOICE_FORMAT_S32: return ma_format_s32;
        case VOICE_FORMAT_F32: return ma_format_f32;
        default: return ma_format_s16;
    }
}

static voice_format_t ma_format_to_voice(ma_format format)
{
    switch (format) {
        case ma_format_u8:  return VOICE_FORMAT_U8;
        case ma_format_s16: return VOICE_FORMAT_S16;
        case ma_format_s24: return VOICE_FORMAT_S24;
        case ma_format_s32: return VOICE_FORMAT_S32;
        case ma_format_f32: return VOICE_FORMAT_F32;
        default: return VOICE_FORMAT_S16;
    }
}

/* ============================================
 * 回调包装
 * ============================================ */

static void device_data_callback(
    ma_device *pDevice,
    void *pOutput,
    const void *pInput,
    ma_uint32 frameCount)
{
    voice_device_t *device = (voice_device_t *)pDevice->pUserData;
    if (device && device->desc.data_callback) {
        device->desc.data_callback(device, pOutput, pInput, frameCount, device->desc.user_data);
    }
}

static void device_stop_callback(ma_device *pDevice)
{
    voice_device_t *device = (voice_device_t *)pDevice->pUserData;
    if (device) {
        device->started = false;
        if (device->desc.stop_callback) {
            device->desc.stop_callback(device, device->desc.user_data);
        }
    }
}

/* ============================================
 * 设备操作
 * ============================================ */

void voice_device_desc_init(voice_device_desc_t *desc, voice_device_mode_t mode)
{
    if (!desc) return;
    
    memset(desc, 0, sizeof(voice_device_desc_t));
    desc->mode = mode;
    
    /* 默认采集配置 */
    desc->capture.format = VOICE_FORMAT_S16;
    desc->capture.channels = 1;
    desc->capture.sample_rate = 48000;
    
    /* 默认播放配置 */
    desc->playback.format = VOICE_FORMAT_S16;
    desc->playback.channels = 1;
    desc->playback.sample_rate = 48000;
    
    desc->period_size_frames = 480;  /* 10ms @ 48kHz */
    desc->periods = 2;
}

voice_device_t *voice_device_create(const voice_device_desc_t *desc)
{
    if (!desc) {
        return NULL;
    }
    
    if (!g_device_context || !g_device_context->initialized) {
        VOICE_LOG_E("Device context not initialized");
        return NULL;
    }
    
    voice_device_t *device = (voice_device_t *)calloc(1, sizeof(voice_device_t));
    if (!device) {
        return NULL;
    }
    
    device->desc = *desc;
    
    ma_device_config config;
    
    switch (desc->mode) {
        case VOICE_DEVICE_CAPTURE:
            config = ma_device_config_init(ma_device_type_capture);
            break;
        case VOICE_DEVICE_PLAYBACK:
            config = ma_device_config_init(ma_device_type_playback);
            break;
        case VOICE_DEVICE_DUPLEX:
        default:
            config = ma_device_config_init(ma_device_type_duplex);
            break;
    }
    
    /* 采集配置 */
    if (desc->mode == VOICE_DEVICE_CAPTURE || desc->mode == VOICE_DEVICE_DUPLEX) {
        config.capture.format = voice_format_to_ma(desc->capture.format);
        config.capture.channels = desc->capture.channels;
    }
    
    /* 播放配置 */
    if (desc->mode == VOICE_DEVICE_PLAYBACK || desc->mode == VOICE_DEVICE_DUPLEX) {
        config.playback.format = voice_format_to_ma(desc->playback.format);
        config.playback.channels = desc->playback.channels;
    }
    
    config.sampleRate = desc->capture.sample_rate;  /* 使用采集采样率 */
    config.periodSizeInFrames = desc->period_size_frames;
    config.periods = desc->periods;
    config.dataCallback = device_data_callback;
    config.stopCallback = device_stop_callback;
    config.pUserData = device;
    
    if (ma_device_init(&g_device_context->context, &config, &device->device) != MA_SUCCESS) {
        VOICE_LOG_E("Failed to initialize audio device");
        free(device);
        return NULL;
    }
    
    device->initialized = true;
    VOICE_LOG_I("Audio device created: %s", device->device.playback.name);
    
    return device;
}

void voice_device_destroy(voice_device_t *device)
{
    if (device) {
        if (device->started) {
            voice_device_stop(device);
        }
        if (device->initialized) {
            ma_device_uninit(&device->device);
        }
        free(device);
    }
}

voice_error_t voice_device_start(voice_device_t *device)
{
    if (!device || !device->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (device->started) {
        return VOICE_OK;
    }
    
    if (ma_device_start(&device->device) != MA_SUCCESS) {
        return VOICE_ERROR_DEVICE_START_FAILED;
    }
    
    device->started = true;
    VOICE_LOG_I("Audio device started");
    
    return VOICE_OK;
}

voice_error_t voice_device_stop(voice_device_t *device)
{
    if (!device || !device->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!device->started) {
        return VOICE_OK;
    }
    
    if (ma_device_stop(&device->device) != MA_SUCCESS) {
        return VOICE_ERROR_DEVICE_STOP_FAILED;
    }
    
    device->started = false;
    VOICE_LOG_I("Audio device stopped");
    
    return VOICE_OK;
}

bool voice_device_is_started(voice_device_t *device)
{
    return device && device->started;
}

uint32_t voice_device_get_sample_rate(voice_device_t *device)
{
    if (!device || !device->initialized) {
        return 0;
    }
    return device->device.sampleRate;
}

uint8_t voice_device_get_channels(voice_device_t *device, voice_device_mode_t mode)
{
    if (!device || !device->initialized) {
        return 0;
    }
    
    if (mode == VOICE_DEVICE_CAPTURE) {
        return device->device.capture.channels;
    } else {
        return device->device.playback.channels;
    }
}

voice_format_t voice_device_get_format(voice_device_t *device, voice_device_mode_t mode)
{
    if (!device || !device->initialized) {
        return VOICE_FORMAT_UNKNOWN;
    }
    
    if (mode == VOICE_DEVICE_CAPTURE) {
        return ma_format_to_voice(device->device.capture.format);
    } else {
        return ma_format_to_voice(device->device.playback.format);
    }
}

/* ============================================
 * 设备枚举
 * ============================================ */

uint32_t voice_device_get_capture_count(void)
{
    if (!g_device_context) return 0;
    return g_device_context->capture_count;
}

uint32_t voice_device_get_playback_count(void)
{
    if (!g_device_context) return 0;
    return g_device_context->playback_count;
}

voice_error_t voice_device_get_capture_info(uint32_t index, voice_device_enum_info_t *info)
{
    if (!g_device_context || !info) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (index >= g_device_context->capture_count) {
        return VOICE_ERROR_DEVICE_NOT_FOUND;
    }
    
    ma_device_info *ma_info = &g_device_context->capture_devices[index];
    
    strncpy(info->id, ma_info->id.custom, sizeof(info->id) - 1);
    strncpy(info->name, ma_info->name, sizeof(info->name) - 1);
    info->is_default = ma_info->isDefault;
    info->min_channels = 1;
    info->max_channels = 2;
    info->min_sample_rate = 8000;
    info->max_sample_rate = 48000;
    
    return VOICE_OK;
}

voice_error_t voice_device_get_playback_info(uint32_t index, voice_device_enum_info_t *info)
{
    if (!g_device_context || !info) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (index >= g_device_context->playback_count) {
        return VOICE_ERROR_DEVICE_NOT_FOUND;
    }
    
    ma_device_info *ma_info = &g_device_context->playback_devices[index];
    
    strncpy(info->id, ma_info->id.custom, sizeof(info->id) - 1);
    strncpy(info->name, ma_info->name, sizeof(info->name) - 1);
    info->is_default = ma_info->isDefault;
    info->min_channels = 1;
    info->max_channels = 2;
    info->min_sample_rate = 8000;
    info->max_sample_rate = 48000;
    
    return VOICE_OK;
}

/* ============================================
 * 兼容性 API 实现
 * ============================================ */

/** 简化版回调包装器数据 */
typedef struct {
    voice_capture_callback_t capture_cb;
    void *capture_user_data;
    voice_playback_callback_t playback_cb;
    void *playback_user_data;
} simple_callback_data_t;

static simple_callback_data_t g_simple_callbacks[16] = {0};
static int g_simple_callback_count = 0;

static void simple_data_callback(
    voice_device_t *device,
    void *pOutput,
    const void *pInput,
    uint32_t frameCount,
    void *user_data)
{
    simple_callback_data_t *cb_data = (simple_callback_data_t *)user_data;
    if (!cb_data) return;
    
    /* 采集回调 */
    if (pInput && cb_data->capture_cb) {
        cb_data->capture_cb(device, (const int16_t *)pInput, frameCount, cb_data->capture_user_data);
    }
    
    /* 播放回调 */
    if (pOutput && cb_data->playback_cb) {
        cb_data->playback_cb(device, (int16_t *)pOutput, frameCount, cb_data->playback_user_data);
    }
}

void voice_device_config_init(voice_device_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(voice_device_config_t));
    config->mode = VOICE_DEVICE_CAPTURE;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_size = 960;  /* 20ms @ 48kHz */
}

voice_device_t *voice_device_create_simple(const voice_device_config_t *config)
{
    if (!config) return NULL;
    
    /* 分配回调数据 */
    if (g_simple_callback_count >= 16) {
        VOICE_LOG_E("Too many simple devices created");
        return NULL;
    }
    
    simple_callback_data_t *cb_data = &g_simple_callbacks[g_simple_callback_count++];
    cb_data->capture_cb = config->capture_callback;
    cb_data->capture_user_data = config->capture_user_data;
    cb_data->playback_cb = config->playback_callback;
    cb_data->playback_user_data = config->playback_user_data;
    
    /* 转换为 voice_device_desc_t */
    voice_device_desc_t desc;
    voice_device_desc_init(&desc, config->mode);
    
    desc.capture.sample_rate = config->sample_rate;
    desc.capture.channels = config->channels;
    desc.capture.format = VOICE_FORMAT_S16;
    
    desc.playback.sample_rate = config->sample_rate;
    desc.playback.channels = config->channels;
    desc.playback.format = VOICE_FORMAT_S16;
    
    desc.period_size_frames = config->frame_size;
    desc.periods = 2;
    
    desc.data_callback = simple_data_callback;
    desc.user_data = cb_data;
    
    return voice_device_create(&desc);
}

voice_error_t voice_device_enumerate(
    voice_device_mode_t mode,
    voice_device_info_t *devices,
    size_t *count)
{
    if (!devices || !count) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (!g_device_context) {
        /* 自动初始化上下文 */
        voice_error_t err = voice_device_context_init();
        if (err != VOICE_OK) {
            return err;
        }
    }
    
    size_t max_count = *count;
    size_t actual_count = 0;
    
    if (mode == VOICE_DEVICE_CAPTURE || mode == VOICE_DEVICE_DUPLEX) {
        /* 枚举采集设备 */
        for (uint32_t i = 0; i < g_device_context->capture_count && actual_count < max_count; i++) {
            ma_device_info *ma_info = &g_device_context->capture_devices[i];
            strncpy(devices[actual_count].id, ma_info->id.custom, sizeof(devices[actual_count].id) - 1);
            strncpy(devices[actual_count].name, ma_info->name, sizeof(devices[actual_count].name) - 1);
            devices[actual_count].is_default = ma_info->isDefault;
            actual_count++;
        }
    } else {
        /* 枚举播放设备 */
        for (uint32_t i = 0; i < g_device_context->playback_count && actual_count < max_count; i++) {
            ma_device_info *ma_info = &g_device_context->playback_devices[i];
            strncpy(devices[actual_count].id, ma_info->id.custom, sizeof(devices[actual_count].id) - 1);
            strncpy(devices[actual_count].name, ma_info->name, sizeof(devices[actual_count].name) - 1);
            devices[actual_count].is_default = ma_info->isDefault;
            actual_count++;
        }
    }
    
    *count = actual_count;
    return VOICE_OK;
}
