/**
 * @file device.h
 * @brief Audio device abstraction layer (miniaudio backend)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 音频设备上下文
 * ============================================ */

/** 设备上下文 (全局单例) */
typedef struct voice_device_context_s voice_device_context_t;

/**
 * @brief 初始化设备上下文
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return 错误码
 */
voice_error_t voice_device_context_init(void);

/**
 * @brief 销毁设备上下文
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_device_context_deinit(void);

/**
 * @brief 获取设备上下文
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return 设备上下文指针
 */
voice_device_context_t *voice_device_context_get(void);

/* ============================================
 * 音频设备
 * ============================================ */

/** 音频设备句柄 */
typedef struct voice_device_s voice_device_t;

/** 设备数据回调 */
typedef void (*voice_device_data_callback_t)(
    voice_device_t *device,
    void *output,
    const void *input,
    uint32_t frame_count,
    void *user_data
);

/** 设备停止回调 */
typedef void (*voice_device_stop_callback_t)(
    voice_device_t *device,
    void *user_data
);

/** 设备类型 */
typedef enum {
    VOICE_DEVICE_CAPTURE,           /**< 仅采集 */
    VOICE_DEVICE_PLAYBACK,          /**< 仅播放 */
    VOICE_DEVICE_DUPLEX             /**< 全双工 */
} voice_device_mode_t;

/* 兼容别名 - 示例代码使用 */
#define VOICE_DEVICE_MODE_CAPTURE   VOICE_DEVICE_CAPTURE
#define VOICE_DEVICE_MODE_PLAYBACK  VOICE_DEVICE_PLAYBACK
#define VOICE_DEVICE_MODE_DUPLEX    VOICE_DEVICE_DUPLEX

/** 采集数据回调 (简化版) */
typedef void (*voice_capture_callback_t)(
    voice_device_t *device,
    const int16_t *input,
    size_t frame_count,
    void *user_data
);

/** 播放数据回调 (简化版) */
typedef void (*voice_playback_callback_t)(
    voice_device_t *device,
    int16_t *output,
    size_t frame_count,
    void *user_data
);

/** 设备配置 */
typedef struct {
    voice_device_mode_t mode;       /**< 设备模式 */
    
    /* 采集配置 */
    struct {
        const char *device_id;      /**< 设备ID (NULL=默认) */
        voice_format_t format;      /**< 格式 */
        uint8_t channels;           /**< 通道数 */
        uint32_t sample_rate;       /**< 采样率 */
    } capture;
    
    /* 播放配置 */
    struct {
        const char *device_id;      /**< 设备ID (NULL=默认) */
        voice_format_t format;      /**< 格式 */
        uint8_t channels;           /**< 通道数 */
        uint32_t sample_rate;       /**< 采样率 */
    } playback;
    
    uint32_t period_size_frames;    /**< 周期大小(帧) */
    uint32_t periods;               /**< 周期数 */
    
    /* 回调 */
    voice_device_data_callback_t data_callback;
    voice_device_stop_callback_t stop_callback;
    void *user_data;
} voice_device_desc_t;

/**
 * @brief 初始化默认设备描述
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param desc 设备描述指针
 * @param mode 设备模式
 */
void voice_device_desc_init(voice_device_desc_t *desc, voice_device_mode_t mode);

/**
 * @brief 创建音频设备
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param desc 设备描述
 * @return 设备句柄
 */
voice_device_t *voice_device_create(const voice_device_desc_t *desc);

/**
 * @brief 销毁音频设备
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 */
void voice_device_destroy(voice_device_t *device);

/**
 * @brief 启动设备
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 * @return 错误码
 */
voice_error_t voice_device_start(voice_device_t *device);

/**
 * @brief 停止设备
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 * @return 错误码
 */
voice_error_t voice_device_stop(voice_device_t *device);

/**
 * @brief 检查设备是否运行中
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 * @return true 运行中
 */
bool voice_device_is_started(voice_device_t *device);

/**
 * @brief 获取设备采样率
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 * @return 采样率
 */
uint32_t voice_device_get_sample_rate(voice_device_t *device);

/**
 * @brief 获取设备通道数
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 * @param mode 设备模式 (采集/播放)
 * @return 通道数
 */
uint8_t voice_device_get_channels(voice_device_t *device, voice_device_mode_t mode);

/**
 * @brief 获取设备格式
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param device 设备句柄
 * @param mode 设备模式
 * @return 格式
 */
voice_format_t voice_device_get_format(voice_device_t *device, voice_device_mode_t mode);

/* ============================================
 * 设备枚举
 * ============================================ */

/** 设备信息 */
typedef struct {
    char id[256];
    char name[256];
    bool is_default;
    uint32_t min_channels;
    uint32_t max_channels;
    uint32_t min_sample_rate;
    uint32_t max_sample_rate;
} voice_device_enum_info_t;

/**
 * @brief 获取采集设备数量
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return 设备数量
 */
uint32_t voice_device_get_capture_count(void);

/**
 * @brief 获取播放设备数量
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return 设备数量
 */
uint32_t voice_device_get_playback_count(void);

/**
 * @brief 获取采集设备信息
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param index 设备索引
 * @param info 设备信息输出
 * @return 错误码
 */
voice_error_t voice_device_get_capture_info(uint32_t index, voice_device_enum_info_t *info);

/**
 * @brief 获取播放设备信息
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param index 设备索引
 * @param info 设备信息输出
 * @return 错误码
 */
voice_error_t voice_device_get_playback_info(uint32_t index, voice_device_enum_info_t *info);

/* ============================================
 * 兼容性 API (用于示例代码)
 * ============================================ */

/** 设备信息 (简化版，用于枚举) */
typedef struct {
    char id[256];                   /**< 设备ID */
    char name[256];                 /**< 设备名称 */
    bool is_default;                /**< 是否默认设备 */
} voice_device_info_t;

/** 设备配置 (简化版) */
typedef struct {
    voice_device_mode_t mode;       /**< 设备模式 */
    uint32_t sample_rate;           /**< 采样率 */
    uint8_t channels;               /**< 通道数 */
    uint32_t frame_size;            /**< 帧大小 (样本数) */
    
    /* 采集回调 */
    voice_capture_callback_t capture_callback;
    void *capture_user_data;
    
    /* 播放回调 */
    voice_playback_callback_t playback_callback;
    void *playback_user_data;
} voice_device_config_t;

/**
 * @brief 初始化默认设备配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param config 配置结构指针
 */
void voice_device_config_init(voice_device_config_t *config);

/**
 * @brief 使用简化配置创建设备
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param config 简化配置
 * @return 设备句柄
 */
voice_device_t *voice_device_create_simple(const voice_device_config_t *config);

/**
 * @brief 枚举设备 (兼容API)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param mode 设备模式
 * @param devices 设备信息数组
 * @param count 输入：数组大小，输出：实际设备数
 * @return 错误码
 */
voice_error_t voice_device_enumerate(
    voice_device_mode_t mode,
    voice_device_info_t *devices,
    size_t *count
);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_DEVICE_H */
