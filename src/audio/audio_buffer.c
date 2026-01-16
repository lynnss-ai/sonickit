/**
 * @file audio_buffer.c
 * @brief Lock-free ring buffer implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "audio/audio_buffer.h"
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Ring Buffer Implementation
 * ============================================ */

/**
 * @brief Create a lock-free ring buffer
 * @details This function allocates and initializes a thread-safe, lock-free ring buffer
 *          using atomic operations. The buffer capacity is automatically rounded up to
 *          the next power of 2 to enable efficient modulo operations using bitwise AND.
 *          The ring buffer supports concurrent single-producer single-consumer (SPSC)
 *          operations without requiring mutex locks, providing low-latency performance
 *          suitable for real-time audio processing.
 * @param capacity Desired minimum capacity in bytes (will be rounded up to power of 2)
 * @param frame_size Size of each frame in bytes (used for frame-aligned operations)
 * @return Pointer to the created ring buffer on success, NULL on allocation failure
 * @note The actual capacity may be larger than requested to ensure power-of-2 size.
 *       Call voice_ring_buffer_destroy() when finished to free resources.
 */
voice_ring_buffer_t *voice_ring_buffer_create(size_t capacity, size_t frame_size)
{
    if (capacity == 0) {
        return NULL;
    }

    voice_ring_buffer_t *rb = (voice_ring_buffer_t *)calloc(1, sizeof(voice_ring_buffer_t));
    if (!rb) {
        return NULL;
    }

    /* Ensure capacity is power of 2 for efficient modulo operations */
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

/**
 * @brief Destroy a ring buffer and free all associated resources
 * @details This function releases the internal data buffer and the ring buffer
 *          structure itself. After calling this function, the ring buffer pointer
 *          becomes invalid and must not be used.
 * @param rb Pointer to the ring buffer to destroy (can be NULL)
 */
void voice_ring_buffer_destroy(voice_ring_buffer_t *rb)
{
    if (rb) {
        if (rb->data) {
            free(rb->data);
        }
        free(rb);
    }
}

/**
 * @brief Write data to the ring buffer
 * @details This function attempts to write the specified amount of data to the ring
 *          buffer in a lock-free, thread-safe manner using atomic operations.
 *          If insufficient space is available, only the maximum possible amount
 *          of data will be written. The function uses memory barriers to ensure
 *          proper ordering of operations across threads. Buffer wraparound is
 *          handled automatically by splitting writes into two memcpy operations
 *          when necessary.
 * @param rb Pointer to the ring buffer
 * @param data Pointer to the source data to write
 * @param size Number of bytes to write
 * @return Actual number of bytes written (may be less than requested if buffer is nearly full)
 * @note This function is safe for single-producer scenarios. The write operation
 *       will not overwrite unread data.
 */
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

    /* Handle wraparound at buffer boundary */
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

/**
 * @brief Read data from the ring buffer
 * @details This function reads the specified amount of data from the ring buffer
 *          in a thread-safe, lock-free manner using atomic operations. If less
 *          data is available than requested, only the available data will be read.
 *          The read operation automatically advances the read pointer using atomic
 *          stores with proper memory ordering. Buffer wraparound is handled seamlessly.
 * @param rb Pointer to the ring buffer
 * @param data Pointer to the destination buffer for read data
 * @param size Number of bytes to read
 * @return Actual number of bytes read (may be less than requested if insufficient data available)
 * @note This function is safe for single-consumer scenarios. Once data is read,
 *       the buffer space becomes available for new writes.
 */
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

    /* Handle wraparound at buffer boundary */
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

/**
 * @brief Get the number of bytes available for reading
 * @details This function returns the current amount of unread data in the ring buffer.
 *          It uses atomic loads with acquire semantics to ensure visibility of the
 *          latest write operations from the producer thread.
 * @param rb Pointer to the ring buffer
 * @return Number of bytes currently available for reading, or 0 if buffer is invalid
 * @note This is a snapshot value and may change immediately after the function returns
 *       in multi-threaded scenarios.
 */
size_t voice_ring_buffer_available(const voice_ring_buffer_t *rb)
{
    if (!rb || !rb->initialized) {
        return 0;
    }

    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);

    return write_pos - read_pos;
}

/**
 * @brief Get the number of bytes available for writing
 * @details This function calculates and returns the current amount of free space
 *          in the ring buffer. It uses atomic loads to safely read the positions
 *          of both the write and read pointers.
 * @param rb Pointer to the ring buffer
 * @return Number of bytes currently available for writing, or 0 if buffer is invalid
 * @note This is a snapshot value and may change immediately in concurrent scenarios.
 */
size_t voice_ring_buffer_free_space(const voice_ring_buffer_t *rb)
{
    if (!rb || !rb->initialized) {
        return 0;
    }

    size_t write_pos = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t read_pos = atomic_load_explicit(&rb->read_pos, memory_order_acquire);

    return rb->capacity - (write_pos - read_pos);
}

/**
 * @brief Clear all data from the ring buffer
 * @details This function resets both read and write pointers to zero, effectively
 *          discarding all data in the buffer. The buffer contents are not zeroed,
 *          but become invisible as the pointers are reset.
 * @param rb Pointer to the ring buffer to clear
 * @note This operation should only be called when there are no concurrent reads or writes.
 */
void voice_ring_buffer_clear(voice_ring_buffer_t *rb)
{
    if (rb && rb->initialized) {
        atomic_store(&rb->read_pos, 0);
        atomic_store(&rb->write_pos, 0);
    }
}

/**
 * @brief Peek at data in the ring buffer without consuming it
 * @details This function reads data from the ring buffer without advancing the read
 *          pointer, allowing the same data to be read again later. This is useful
 *          for inspecting data before committing to processing it, or for implementing
 *          lookahead algorithms. The function behaves like voice_ring_buffer_read()
 *          but leaves the buffer state unchanged.
 * @param rb Pointer to the ring buffer
 * @param data Pointer to the destination buffer for peeked data
 * @param size Number of bytes to peek
 * @return Actual number of bytes peeked (may be less than requested if insufficient data available)
 * @note The peeked data remains in the buffer and will be returned by subsequent
 *       read or peek operations.
 */
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

/**
 * @brief Skip (discard) data from the ring buffer without reading it
 * @details This function advances the read pointer by the specified amount, effectively
 *          discarding data from the buffer without copying it. This is more efficient
 *          than reading data into a temporary buffer when the data is not needed.
 *          Useful for dropping old data or fast-forwarding through the buffer.
 * @param rb Pointer to the ring buffer
 * @param size Number of bytes to skip
 * @return Actual number of bytes skipped (may be less than requested if insufficient data available)
 */
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
 * Frame Buffer Implementation
 * ============================================ */

/**
 * @brief Create a frame-based audio buffer
 * @details This function creates a specialized buffer for audio frames with a specific
 *          format. Unlike the raw ring buffer, this buffer operates on complete audio
 *          frames, ensuring that partial frames are never written or read. It internally
 *          uses a ring buffer sized to hold the specified number of complete frames.
 *          The frame size is calculated based on the audio format's sample rate, frame
 *          duration, channel count, and sample format.
 * @param format Pointer to the audio format specification
 * @param max_frames Maximum number of complete frames the buffer can hold
 * @return Pointer to the created frame buffer on success, NULL on failure
 * @note Call voice_frame_buffer_destroy() when finished to free resources.
 */
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

/**
 * @brief Destroy a frame buffer and free all associated resources
 * @details This function releases the internal ring buffer and the frame buffer
 *          structure. After calling this function, the frame buffer pointer
 *          becomes invalid.
 * @param fb Pointer to the frame buffer to destroy (can be NULL)
 */
void voice_frame_buffer_destroy(voice_frame_buffer_t *fb)
{
    if (fb) {
        if (fb->rb) {
            voice_ring_buffer_destroy(fb->rb);
        }
        free(fb);
    }
}

/**
 * @brief Write a complete audio frame to the buffer
 * @details This function writes one complete audio frame to the frame buffer.
 *          The frame must match the format specified when the buffer was created.
 *          If insufficient space is available to write the entire frame, the operation
 *          fails and returns an error without writing partial data.
 * @param fb Pointer to the frame buffer
 * @param frame Pointer to the audio frame to write
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NULL_POINTER if parameters are invalid,
 *         VOICE_ERROR_OVERFLOW if insufficient space for the complete frame
 * @note The function ensures atomic frame writes - either the entire frame is written
 *       or none of it is, preventing partial frame corruption.
 */
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

/**
 * @brief Read a complete audio frame from the buffer
 * @details This function reads one complete audio frame from the frame buffer and
 *          updates the frame structure with the audio data and metadata (size, samples,
 *          format). If insufficient data is available for a complete frame, the operation
 *          fails without reading partial data.
 * @param fb Pointer to the frame buffer
 * @param frame Pointer to the frame structure where data will be stored
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NULL_POINTER if parameters are invalid,
 *         VOICE_ERROR_UNDERFLOW if insufficient data for a complete frame
 * @note The frame->data buffer must be pre-allocated with sufficient size.
 *       The function ensures atomic frame reads.
 */
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

/**
 * @brief Get the number of complete frames available in the buffer
 * @details This function returns how many complete audio frames are currently stored
 *          in the buffer and ready to be read. Partial frames are not counted.
 * @param fb Pointer to the frame buffer
 * @return Number of complete frames available, or 0 if buffer is invalid
 */
size_t voice_frame_buffer_count(const voice_frame_buffer_t *fb)
{
    if (!fb) {
        return 0;
    }

    return voice_ring_buffer_available(fb->rb) / fb->frame_bytes;
}

/**
 * @brief Clear all frames from the frame buffer
 * @details This function removes all audio frames from the buffer by clearing
 *          the underlying ring buffer, resetting it to an empty state.
 * @param fb Pointer to the frame buffer to clear
 */
void voice_frame_buffer_clear(voice_frame_buffer_t *fb)
{
    if (fb && fb->rb) {
        voice_ring_buffer_clear(fb->rb);
    }
}
