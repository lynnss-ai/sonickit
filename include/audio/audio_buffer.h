/**
 * @file audio_buffer.h
 * @brief Lock-free ring buffer for audio data
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Ring Buffer
 * ============================================ */

typedef struct {
    uint8_t *data;                      /**< Data buffer */
    size_t capacity;                    /**< Capacity (bytes) */
    atomic_size_t read_pos;             /**< Read position */
    atomic_size_t write_pos;            /**< Write position */
    size_t frame_size;                  /**< Frame size (bytes) */
    bool initialized;
} voice_ring_buffer_t;

/**
 * @brief Create ring buffer
 * @param capacity Capacity (bytes)
 * @param frame_size Frame size (bytes)
 * @return Buffer pointer
 */
VOICE_API voice_ring_buffer_t *voice_ring_buffer_create(size_t capacity, size_t frame_size);

/**
 * @brief Destroy ring buffer
 * @param rb Buffer pointer
 */
VOICE_API void voice_ring_buffer_destroy(voice_ring_buffer_t *rb);

/**
 * @brief Write data
 * @param rb Buffer pointer
 * @param data Data pointer
 * @param size Data size (bytes)
 * @return Actual bytes written
 */
VOICE_API size_t voice_ring_buffer_write(voice_ring_buffer_t *rb, const void *data, size_t size);

/**
 * @brief Read data
 * @param rb Buffer pointer
 * @param data Output buffer
 * @param size Requested size (bytes)
 * @return Actual bytes read
 */
VOICE_API size_t voice_ring_buffer_read(voice_ring_buffer_t *rb, void *data, size_t size);

/**
 * @brief Get available bytes to read
 * @param rb Buffer pointer
 * @return Available bytes to read
 */
VOICE_API size_t voice_ring_buffer_available(const voice_ring_buffer_t *rb);

/**
 * @brief Get free space for writing
 * @param rb Buffer pointer
 * @return Free bytes available for writing
 */
VOICE_API size_t voice_ring_buffer_free_space(const voice_ring_buffer_t *rb);

/**
 * @brief Clear buffer
 * @param rb Buffer pointer
 */
VOICE_API void voice_ring_buffer_clear(voice_ring_buffer_t *rb);

/**
 * @brief Peek data (without moving read pointer)
 * @param rb Buffer pointer
 * @param data Output buffer
 * @param size Requested size (bytes)
 * @return Actual bytes peeked
 */
VOICE_API size_t voice_ring_buffer_peek(const voice_ring_buffer_t *rb, void *data, size_t size);

/**
 * @brief Skip data
 * @param rb Buffer pointer
 * @param size Bytes to skip
 * @return Actual bytes skipped
 */
VOICE_API size_t voice_ring_buffer_skip(voice_ring_buffer_t *rb, size_t size);

/* ============================================
 * Frame Buffer (per-frame operations)
 * ============================================ */

typedef struct {
    voice_ring_buffer_t *rb;
    voice_audio_format_t format;
    size_t frame_samples;               /**< Samples per frame */
    size_t frame_bytes;                 /**< Bytes per frame */
} voice_frame_buffer_t;

/**
 * @brief Create frame buffer
 * @param format Audio format
 * @param max_frames Maximum number of frames
 * @return Frame buffer pointer
 */
VOICE_API voice_frame_buffer_t *voice_frame_buffer_create(
    const voice_audio_format_t *format,
    size_t max_frames
);

/**
 * @brief Destroy frame buffer
 * @param fb Frame buffer pointer
 */
VOICE_API void voice_frame_buffer_destroy(voice_frame_buffer_t *fb);

/**
 * @brief Write one frame
 * @param fb Frame buffer pointer
 * @param frame Frame data
 * @return Error code
 */
VOICE_API voice_error_t voice_frame_buffer_write(voice_frame_buffer_t *fb, const voice_frame_t *frame);

/**
 * @brief Read one frame
 * @param fb Frame buffer pointer
 * @param frame Output frame
 * @return Error code
 */
VOICE_API voice_error_t voice_frame_buffer_read(voice_frame_buffer_t *fb, voice_frame_t *frame);

/**
 * @brief Get readable frame count
 * @param fb Frame buffer pointer
 * @return Readable frame count
 */
VOICE_API size_t voice_frame_buffer_count(const voice_frame_buffer_t *fb);

/**
 * @brief Clear frame buffer
 * @param fb Frame buffer pointer
 */
VOICE_API void voice_frame_buffer_clear(voice_frame_buffer_t *fb);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_BUFFER_H */
