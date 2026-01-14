/**
 * @file file_io.h
 * @brief Audio file I/O interface (WAV/MP3/FLAC)
 */

#ifndef AUDIO_FILE_IO_H
#define AUDIO_FILE_IO_H

#include "voice/types.h"
#include "voice/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 文件格式
 * ============================================ */

typedef enum {
    AUDIO_FILE_FORMAT_UNKNOWN = 0,
    AUDIO_FILE_FORMAT_WAV,
    AUDIO_FILE_FORMAT_MP3,
    AUDIO_FILE_FORMAT_FLAC,
    AUDIO_FILE_FORMAT_OGG,
} audio_file_format_t;

/* ============================================
 * 文件信息
 * ============================================ */

typedef struct {
    audio_file_format_t format;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint64_t total_frames;      /**< 总帧数 (每通道样本数) */
    double duration_seconds;    /**< 时长 (秒) */
    uint32_t bitrate;           /**< 比特率 (bps, 仅压缩格式) */
} audio_file_info_t;

/* ============================================
 * 文件读取器
 * ============================================ */

typedef struct audio_reader_s audio_reader_t;

/**
 * @brief 打开音频文件用于读取
 * @param path 文件路径
 * @return 读取器句柄
 */
audio_reader_t *audio_reader_open(const char *path);

/**
 * @brief 从内存打开音频数据
 * @param data 音频数据
 * @param size 数据大小
 * @return 读取器句柄
 */
audio_reader_t *audio_reader_open_memory(const void *data, size_t size);

/**
 * @brief 关闭读取器
 */
void audio_reader_close(audio_reader_t *reader);

/**
 * @brief 获取音频信息
 */
voice_error_t audio_reader_get_info(
    audio_reader_t *reader,
    audio_file_info_t *info
);

/**
 * @brief 读取PCM数据 (int16)
 * @param reader 读取器句柄
 * @param output 输出缓冲区
 * @param frames_to_read 要读取的帧数
 * @param frames_read 实际读取的帧数
 * @return 错误码
 */
voice_error_t audio_reader_read_s16(
    audio_reader_t *reader,
    int16_t *output,
    size_t frames_to_read,
    size_t *frames_read
);

/**
 * @brief 读取PCM数据 (float)
 * @param reader 读取器句柄
 * @param output 输出缓冲区
 * @param frames_to_read 要读取的帧数
 * @param frames_read 实际读取的帧数
 * @return 错误码
 */
voice_error_t audio_reader_read_f32(
    audio_reader_t *reader,
    float *output,
    size_t frames_to_read,
    size_t *frames_read
);

/**
 * @brief 定位到指定帧
 * @param reader 读取器句柄
 * @param frame 目标帧号
 * @return 错误码
 */
voice_error_t audio_reader_seek(
    audio_reader_t *reader,
    uint64_t frame
);

/**
 * @brief 获取当前帧位置
 */
uint64_t audio_reader_tell(audio_reader_t *reader);

/**
 * @brief 检查是否到达文件末尾
 */
bool audio_reader_is_eof(audio_reader_t *reader);

/* ============================================
 * 文件写入器
 * ============================================ */

typedef struct audio_writer_s audio_writer_t;

/** 写入器配置 */
typedef struct {
    audio_file_format_t format;
    uint32_t sample_rate;
    uint8_t channels;
    uint8_t bits_per_sample;    /**< WAV: 8/16/24/32 */
    uint32_t bitrate;           /**< MP3/压缩格式比特率 */
    int quality;                /**< 编码质量 (0-10) */
} audio_writer_config_t;

/**
 * @brief 初始化默认写入配置
 */
void audio_writer_config_init(audio_writer_config_t *config);

/**
 * @brief 创建音频文件写入器
 * @param path 文件路径
 * @param config 配置
 * @return 写入器句柄
 */
audio_writer_t *audio_writer_create(
    const char *path,
    const audio_writer_config_t *config
);

/**
 * @brief 关闭写入器 (完成文件写入)
 */
voice_error_t audio_writer_close(audio_writer_t *writer);

/**
 * @brief 写入PCM数据 (int16)
 * @param writer 写入器句柄
 * @param input 输入缓冲区
 * @param frames 帧数
 * @return 错误码
 */
voice_error_t audio_writer_write_s16(
    audio_writer_t *writer,
    const int16_t *input,
    size_t frames
);

/**
 * @brief 写入PCM数据 (float)
 * @param writer 写入器句柄
 * @param input 输入缓冲区
 * @param frames 帧数
 * @return 错误码
 */
voice_error_t audio_writer_write_f32(
    audio_writer_t *writer,
    const float *input,
    size_t frames
);

/**
 * @brief 获取已写入的帧数
 */
uint64_t audio_writer_get_frames_written(audio_writer_t *writer);

/* ============================================
 * 工具函数
 * ============================================ */

/**
 * @brief 从文件扩展名推断格式
 */
audio_file_format_t audio_file_format_from_path(const char *path);

/**
 * @brief 获取格式名称
 */
const char *audio_file_format_name(audio_file_format_t format);

/**
 * @brief 检查格式是否支持
 */
bool audio_file_format_supported(audio_file_format_t format);

/* ============================================
 * 兼容性 API (用于示例代码)
 * ============================================ */

/**
 * @brief 获取音频信息 (简化版 - 兼容旧示例)
 * @param reader 读取器句柄
 * @param sample_rate 采样率输出
 * @param channels 通道数输出
 * @param total_frames 总帧数输出
 * @return 错误码
 */
static inline voice_error_t audio_reader_get_info_ex(
    audio_reader_t *reader,
    uint32_t *sample_rate,
    uint8_t *channels,
    uint64_t *total_frames)
{
    audio_file_info_t info;
    voice_error_t err = audio_reader_get_info(reader, &info);
    if (err == VOICE_OK) {
        if (sample_rate) *sample_rate = info.sample_rate;
        if (channels) *channels = info.channels;
        if (total_frames) *total_frames = info.total_frames;
    }
    return err;
}

/**
 * @brief 创建音频写入器 (简化版 - 兼容旧示例)
 * @param path 文件路径
 * @param sample_rate 采样率
 * @param channels 通道数
 * @return 写入器句柄
 */
audio_writer_t *audio_writer_create_simple(
    const char *path,
    uint32_t sample_rate,
    uint8_t channels
);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_FILE_IO_H */
