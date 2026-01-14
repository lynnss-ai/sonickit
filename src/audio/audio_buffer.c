/**
 * @file audio_buffer.c
 * @brief Lock-free ring buffer implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/audio_buffer.h"
#include <stdlib.h>
#include <string.h>

/* ============================================
 * 环形缓冲区实现
 * ============================================ */

voice_ring_buffer_t *voice_ring_buffer_create(size_t capacity, size_t frame_size)
{
    if (capacity == 0) {
        return NULL;
    }
    
    voice_ring_buffer_t *rb = (voice_ring_buffer_t *)calloc(1, sizeof(voice_ring_buffer_t));
    if (!rb) {
        return NULL;
    }
    
    /* 确保容量是2的幂次方，便于取模运算 */
    size_t actual_capacity = 1;
    while (actual_capacity < capacity) {
        actual_capacity <<= 1;
    }
    
    rb->data = (uint8_t *)calloc(actual_capacity, 1);
    if (!rb->data) {
        free(rb);
        return NULL;
    }
    
    rb->capacity = actual_capacity;
    rb->frame_size = frame_size > 0 ? frame_size : 1;
    atomic_store(&rb->read_pos, 0);
    atomic_store(&rb->write_pos, 0);
    rb->initialized = true;
    
    return rb;
}

void voice_ring_buffer_destroy(voice_ring_buffer_t *rb)
{
    if (rb) {
        if (rb->data) {
            free(rb->data);
        }
        free(rb);
    }
}

size_t voice_ring_buffer_write(voice_ring_buffer_t *rb, const void *data, size_t size)
{
    if (!rb || !rb->initialized || !data || size == 0) {
        return 0;
    }
    
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    
    size_t free_space = rb->capacity - (write_pos - read_pos);
    size_t to_write = (size < free_space) ? size : free_space;
    
    if (to_write == 0) {
        return 0;
    }
    
    size_t mask = rb->capacity - 1;
    size_t write_index = write_pos & mask;
    
    /* 处理环绕 */
    size_t first_chunk = rb->capacity - write_index;
    if (first_chunk >= to_write) {
        memcpy(rb->data + write_index, data, to_write);
    } else {
        memcpy(rb->data + write_index, data, first_chunk);
        memcpy(rb->data, (const uint8_t *)data + first_chunk, to_write - first_chunk);
    }
    
    atomic_store_explicit(&rb->write_pos, write_pos + to_write, memory_order_release);
    
    return to_write;
}

size_t voice_ring_buffer_read(voice_ring_buffer_t *rb, void *data, size_t size)
{
    if (!rb || !rb->initialized || !data || size == 0) {
        return 0;
    }
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    
    size_t available = write_pos - read_pos;
    size_t to_read = (size < available) ? size : available;
    
    if (to_read == 0) {
        return 0;
    }
    
    size_t mask = rb->capacity - 1;
    size_t read_index = read_pos & mask;
    
    /* 处理环绕 */
    size_t first_chunk = rb->capacity - read_index;
    if (first_chunk >= to_read) {
        memcpy(data, rb->data + read_index, to_read);
    } else {
        memcpy(data, rb->data + read_index, first_chunk);
        memcpy((uint8_t *)data + first_chunk, rb->data, to_read - first_chunk);
    }
    
    atomic_store_explicit(&rb->read_pos, read_pos + to_read, memory_order_release);
    
    return to_read;
}

size_t voice_ring_buffer_available(const voice_ring_buffer_t *rb)
{
    if (!rb || !rb->initialized) {
        return 0;
    }
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    
    return write_pos - read_pos;
}

size_t voice_ring_buffer_free_space(const voice_ring_buffer_t *rb)
{
    if (!rb || !rb->initialized) {
        return 0;
    }
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    
    return rb->capacity - (write_pos - read_pos);
}

void voice_ring_buffer_clear(voice_ring_buffer_t *rb)
{
    if (rb && rb->initialized) {
        atomic_store(&rb->read_pos, 0);
        atomic_store(&rb->write_pos, 0);
    }
}

size_t voice_ring_buffer_peek(const voice_ring_buffer_t *rb, void *data, size_t size)
{
    if (!rb || !rb->initialized || !data || size == 0) {
        return 0;
    }
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    
    size_t available = write_pos - read_pos;
    size_t to_peek = (size < available) ? size : available;
    
    if (to_peek == 0) {
        return 0;
    }
    
    size_t mask = rb->capacity - 1;
    size_t read_index = read_pos & mask;
    
    size_t first_chunk = rb->capacity - read_index;
    if (first_chunk >= to_peek) {
        memcpy(data, rb->data + read_index, to_peek);
    } else {
        memcpy(data, rb->data + read_index, first_chunk);
        memcpy((uint8_t *)data + first_chunk, rb->data, to_peek - first_chunk);
    }
    
    return to_peek;
}

size_t voice_ring_buffer_skip(voice_ring_buffer_t *rb, size_t size)
{
    if (!rb || !rb->initialized || size == 0) {
        return 0;
    }
    
    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    
    size_t available = write_pos - read_pos;
    size_t to_skip = (size < available) ? size : available;
    
    if (to_skip > 0) {
        atomic_store_explicit(&rb->read_pos, read_pos + to_skip, memory_order_release);
    }
    
    return to_skip;
}

/* ============================================
 * 帧缓冲区实现
 * ============================================ */

voice_frame_buffer_t *voice_frame_buffer_create(
    const voice_audio_format_t *format,
    size_t max_frames)
{
    if (!format || max_frames == 0) {
        return NULL;
    }
    
    voice_frame_buffer_t *fb = (voice_frame_buffer_t *)calloc(1, sizeof(voice_frame_buffer_t));
    if (!fb) {
        return NULL;
    }
    
    fb->format = *format;
    fb->frame_samples = VOICE_SAMPLES_PER_FRAME(format->sample_rate, format->frame_size_ms);
    fb->frame_bytes = fb->frame_samples * format->channels * voice_format_bytes(format->format);
    
    size_t capacity = fb->frame_bytes * max_frames;
    fb->rb = voice_ring_buffer_create(capacity, fb->frame_bytes);
    if (!fb->rb) {
        free(fb);
        return NULL;
    }
    
    return fb;
}

void voice_frame_buffer_destroy(voice_frame_buffer_t *fb)
{
    if (fb) {
        if (fb->rb) {
            voice_ring_buffer_destroy(fb->rb);
        }
        free(fb);
    }
}

voice_error_t voice_frame_buffer_write(voice_frame_buffer_t *fb, const voice_frame_t *frame)
{
    if (!fb || !frame || !frame->data) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    size_t written = voice_ring_buffer_write(fb->rb, frame->data, fb->frame_bytes);
    if (written < fb->frame_bytes) {
        return VOICE_ERROR_OVERFLOW;
    }
    
    return VOICE_OK;
}

voice_error_t voice_frame_buffer_read(voice_frame_buffer_t *fb, voice_frame_t *frame)
{
    if (!fb || !frame || !frame->data) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    size_t read = voice_ring_buffer_read(fb->rb, frame->data, fb->frame_bytes);
    if (read < fb->frame_bytes) {
        return VOICE_ERROR_UNDERFLOW;
    }
    
    frame->size = fb->frame_bytes;
    frame->samples = fb->frame_samples;
    frame->format = fb->format;
    
    return VOICE_OK;
}

size_t voice_frame_buffer_count(const voice_frame_buffer_t *fb)
{
    if (!fb) {
        return 0;
    }
    
    return voice_ring_buffer_available(fb->rb) / fb->frame_bytes;
}

void voice_frame_buffer_clear(voice_frame_buffer_t *fb)
{
    if (fb && fb->rb) {
        voice_ring_buffer_clear(fb->rb);
    }
}
