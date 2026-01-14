/**
 * @file voice.h
 * @brief Voice library main API header
 * 
 * 跨平台实时语音处理库
 * 
 * 功能特性:
 * - 音频采集和播放 (miniaudio)
 * - 回声消除 (SpeexDSP AEC)
 * - 噪声抑制 (SpeexDSP / RNNoise)
 * - 自动增益控制 (AGC)
 * - 多编解码器支持 (Opus / G.711 / G.722)
 * - RTP/RTCP 网络传输
 * - SRTP 加密 (AES-CM / AES-GCM)
 * - DTLS-SRTP 密钥交换
 * - 音频文件读写 (WAV / MP3 / FLAC)
 * 
 * 支持平台:
 * - Windows (WASAPI)
 * - Linux (ALSA / PulseAudio)
 * - macOS (Core Audio)
 * - iOS (Core Audio / AVAudioSession)
 * - Android (AAudio / OpenSL ES)
 */

#ifndef VOICE_H
#define VOICE_H

#include "types.h"
#include "error.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 版本信息
 * ============================================ */

#define VOICE_VERSION_MAJOR 1
#define VOICE_VERSION_MINOR 0
#define VOICE_VERSION_PATCH 0

#define VOICE_VERSION_STRING "1.0.0"

/**
 * @brief 获取版本字符串
 * @return 版本字符串
 */
const char *voice_version(void);

/**
 * @brief 获取版本号
 * @param major 主版本号
 * @param minor 次版本号
 * @param patch 补丁号
 */
void voice_version_get(int *major, int *minor, int *patch);

/* ============================================
 * 库初始化
 * ============================================ */

/**
 * @brief 初始化语音库
 * @param config 全局配置 (NULL使用默认配置)
 * @return 错误码
 */
voice_error_t voice_init(const voice_global_config_t *config);

/**
 * @brief 释放语音库资源
 */
void voice_deinit(void);

/**
 * @brief 检查库是否已初始化
 * @return true 已初始化
 */
bool voice_is_initialized(void);

/* ============================================
 * 音频设备管理
 * ============================================ */

/** 设备类型 */
typedef enum {
    VOICE_DEVICE_TYPE_CAPTURE,      /**< 采集设备 */
    VOICE_DEVICE_TYPE_PLAYBACK      /**< 播放设备 */
} voice_device_type_t;

/** 设备信息 */
typedef struct {
    char id[256];                   /**< 设备ID */
    char name[256];                 /**< 设备名称 */
    voice_device_type_t type;       /**< 设备类型 */
    bool is_default;                /**< 是否默认设备 */
    uint32_t min_sample_rate;       /**< 最小采样率 */
    uint32_t max_sample_rate;       /**< 最大采样率 */
    uint8_t min_channels;           /**< 最小通道数 */
    uint8_t max_channels;           /**< 最大通道数 */
} voice_device_info_t;

/**
 * @brief 获取音频设备数量
 * @param type 设备类型
 * @return 设备数量
 */
int voice_device_get_count(voice_device_type_t type);

/**
 * @brief 获取音频设备信息
 * @param type 设备类型
 * @param index 设备索引
 * @param info 设备信息输出
 * @return 错误码
 */
voice_error_t voice_device_get_info(
    voice_device_type_t type,
    int index,
    voice_device_info_t *info
);

/**
 * @brief 获取默认设备信息
 * @param type 设备类型
 * @param info 设备信息输出
 * @return 错误码
 */
voice_error_t voice_device_get_default(
    voice_device_type_t type,
    voice_device_info_t *info
);

/* ============================================
 * 音频处理管线
 * ============================================ */

/** 管线句柄 */
typedef struct voice_pipeline_s voice_pipeline_t;

/**
 * @brief 创建音频处理管线
 * @param config 管线配置
 * @return 管线句柄 (NULL表示失败)
 */
voice_pipeline_t *voice_pipeline_create(const voice_pipeline_config_t *config);

/**
 * @brief 销毁音频处理管线
 * @param pipeline 管线句柄
 */
void voice_pipeline_destroy(voice_pipeline_t *pipeline);

/**
 * @brief 启动音频处理
 * @param pipeline 管线句柄
 * @return 错误码
 */
voice_error_t voice_pipeline_start(voice_pipeline_t *pipeline);

/**
 * @brief 停止音频处理
 * @param pipeline 管线句柄
 * @return 错误码
 */
voice_error_t voice_pipeline_stop(voice_pipeline_t *pipeline);

/**
 * @brief 检查管线是否运行中
 * @param pipeline 管线句柄
 * @return true 运行中
 */
bool voice_pipeline_is_running(voice_pipeline_t *pipeline);

/**
 * @brief 设置去噪引擎
 * @param pipeline 管线句柄
 * @param engine 去噪引擎类型
 * @return 错误码
 */
voice_error_t voice_pipeline_set_denoise_engine(
    voice_pipeline_t *pipeline,
    voice_denoise_engine_t engine
);

/**
 * @brief 设置编解码器
 * @param pipeline 管线句柄
 * @param type 编解码器类型
 * @return 错误码
 */
voice_error_t voice_pipeline_set_codec(
    voice_pipeline_t *pipeline,
    voice_codec_type_t type
);

/**
 * @brief 设置比特率
 * @param pipeline 管线句柄
 * @param bitrate 比特率 (bps)
 * @return 错误码
 */
voice_error_t voice_pipeline_set_bitrate(
    voice_pipeline_t *pipeline,
    uint32_t bitrate
);

/**
 * @brief 启用/禁用回声消除
 * @param pipeline 管线句柄
 * @param enabled 是否启用
 * @return 错误码
 */
voice_error_t voice_pipeline_set_aec_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief 启用/禁用去噪
 * @param pipeline 管线句柄
 * @param enabled 是否启用
 * @return 错误码
 */
voice_error_t voice_pipeline_set_denoise_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief 获取网络统计
 * @param pipeline 管线句柄
 * @param stats 统计信息输出
 * @return 错误码
 */
voice_error_t voice_pipeline_get_network_stats(
    voice_pipeline_t *pipeline,
    voice_network_stats_t *stats
);

/* ============================================
 * 简化API - 本地录音/播放
 * ============================================ */

/** 录音器句柄 */
typedef struct voice_recorder_s voice_recorder_t;

/** 播放器句柄 */
typedef struct voice_player_s voice_player_t;

/**
 * @brief 创建录音器
 * @param config 设备配置
 * @param output_file 输出文件路径 (NULL不保存文件)
 * @return 录音器句柄
 */
voice_recorder_t *voice_recorder_create(
    const voice_device_config_t *config,
    const char *output_file
);

/**
 * @brief 销毁录音器
 * @param recorder 录音器句柄
 */
void voice_recorder_destroy(voice_recorder_t *recorder);

/**
 * @brief 开始录音
 * @param recorder 录音器句柄
 * @return 错误码
 */
voice_error_t voice_recorder_start(voice_recorder_t *recorder);

/**
 * @brief 停止录音
 * @param recorder 录音器句柄
 * @return 错误码
 */
voice_error_t voice_recorder_stop(voice_recorder_t *recorder);

/**
 * @brief 设置录音数据回调
 * @param recorder 录音器句柄
 * @param callback 回调函数
 * @param user_data 用户数据
 */
void voice_recorder_set_callback(
    voice_recorder_t *recorder,
    voice_audio_callback_t callback,
    void *user_data
);

/**
 * @brief 创建播放器
 * @param config 设备配置
 * @return 播放器句柄
 */
voice_player_t *voice_player_create(const voice_device_config_t *config);

/**
 * @brief 销毁播放器
 * @param player 播放器句柄
 */
void voice_player_destroy(voice_player_t *player);

/**
 * @brief 播放文件
 * @param player 播放器句柄
 * @param path 文件路径
 * @return 错误码
 */
voice_error_t voice_player_play_file(voice_player_t *player, const char *path);

/**
 * @brief 播放PCM数据
 * @param player 播放器句柄
 * @param data PCM数据
 * @param size 数据大小
 * @return 错误码
 */
voice_error_t voice_player_play_pcm(
    voice_player_t *player,
    const void *data,
    size_t size
);

/**
 * @brief 停止播放
 * @param player 播放器句柄
 * @return 错误码
 */
voice_error_t voice_player_stop(voice_player_t *player);

/* ============================================
 * 平台特定功能
 * ============================================ */

/**
 * @brief 获取平台名称
 * @return 平台名称字符串
 */
const char *voice_platform_name(void);

/**
 * @brief 获取CPU使用率
 * @return CPU使用率 (0-100)
 */
float voice_platform_get_cpu_usage(void);

/**
 * @brief 获取电池电量
 * @return 电池电量 (0-100, -1表示无电池)
 */
int voice_platform_get_battery_level(void);

/**
 * @brief 检查是否使用电池供电
 * @return true 使用电池
 */
bool voice_platform_on_battery(void);

/**
 * @brief 请求音频焦点 (移动端)
 * @return 错误码
 */
voice_error_t voice_platform_request_audio_focus(void);

/**
 * @brief 释放音频焦点 (移动端)
 * @return 错误码
 */
voice_error_t voice_platform_release_audio_focus(void);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_H */
