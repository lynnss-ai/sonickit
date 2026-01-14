/**
 * @file audio_buffer.h
 * @brief Lock-free ring buffer for audio data
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 环形缓冲区
 * ============================================ */

typedef struct {
    uint8_t *data;                      /**< 数据缓冲区 */
    size_t capacity;                    /**< 容量(字节) */
    atomic_size_t read_pos;             /**< 读位置 */
    atomic_size_t write_pos;            /**< 写位置 */
    size_t frame_size;                  /**< 帧大小(字节) */
    bool initialized;
} voice_ring_buffer_t;

/**
 * @brief 创建环形缓冲区
 * @param capacity 容量(字节)
 * @param frame_size 帧大小(字节)
 * @return 缓冲区指针
 */
voice_ring_buffer_t *voice_ring_buffer_create(size_t capacity, size_t frame_size);

/**
 * @brief 销毁环形缓冲区
 * @param rb 缓冲区指针
 */
void voice_ring_buffer_destroy(voice_ring_buffer_t *rb);

/**
 * @brief 写入数据
 * @param rb 缓冲区指针
 * @param data 数据指针
 * @param size 数据大小(字节)
 * @return 实际写入的字节数
 */
size_t voice_ring_buffer_write(voice_ring_buffer_t *rb, const void *data, size_t size);

/**
 * @brief 读取数据
 * @param rb 缓冲区指针
 * @param data 输出缓冲区
 * @param size 请求大小(字节)
 * @return 实际读取的字节数
 */
size_t voice_ring_buffer_read(voice_ring_buffer_t *rb, void *data, size_t size);

/**
 * @brief 获取可读取的字节数
 * @param rb 缓冲区指针
 * @return 可读取的字节数
 */
size_t voice_ring_buffer_available(const voice_ring_buffer_t *rb);

/**
 * @brief 获取可写入的字节数
 * @param rb 缓冲区指针
 * @return 可写入的字节数
 */
size_t voice_ring_buffer_free_space(const voice_ring_buffer_t *rb);

/**
 * @brief 清空缓冲区
 * @param rb 缓冲区指针
 */
void voice_ring_buffer_clear(voice_ring_buffer_t *rb);

/**
 * @brief 查看数据(不移动读指针)
 * @param rb 缓冲区指针
 * @param data 输出缓冲区
 * @param size 请求大小(字节)
 * @return 实际查看的字节数
 */
size_t voice_ring_buffer_peek(const voice_ring_buffer_t *rb, void *data, size_t size);

/**
 * @brief 跳过数据
 * @param rb 缓冲区指针
 * @param size 跳过的字节数
 * @return 实际跳过的字节数
 */
size_t voice_ring_buffer_skip(voice_ring_buffer_t *rb, size_t size);

/* ============================================
 * 帧缓冲区 (按帧操作)
 * ============================================ */

typedef struct {
    voice_ring_buffer_t *rb;
    voice_audio_format_t format;
    size_t frame_samples;               /**< 每帧样本数 */
    size_t frame_bytes;                 /**< 每帧字节数 */
} voice_frame_buffer_t;

/**
 * @brief 创建帧缓冲区
 * @param format 音频格式
 * @param max_frames 最大帧数
 * @return 帧缓冲区指针
 */
voice_frame_buffer_t *voice_frame_buffer_create(
    const voice_audio_format_t *format,
    size_t max_frames
);

/**
 * @brief 销毁帧缓冲区
 * @param fb 帧缓冲区指针
 */
void voice_frame_buffer_destroy(voice_frame_buffer_t *fb);

/**
 * @brief 写入一帧
 * @param fb 帧缓冲区指针
 * @param frame 帧数据
 * @return 错误码
 */
voice_error_t voice_frame_buffer_write(voice_frame_buffer_t *fb, const voice_frame_t *frame);

/**
 * @brief 读取一帧
 * @param fb 帧缓冲区指针
 * @param frame 输出帧
 * @return 错误码
 */
voice_error_t voice_frame_buffer_read(voice_frame_buffer_t *fb, voice_frame_t *frame);

/**
 * @brief 获取可读帧数
 * @param fb 帧缓冲区指针
 * @return 可读帧数
 */
size_t voice_frame_buffer_count(const voice_frame_buffer_t *fb);

/**
 * @brief 清空帧缓冲区
 * @param fb 帧缓冲区指针
 */
void voice_frame_buffer_clear(voice_frame_buffer_t *fb);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_BUFFER_H */
