/**
 * @file simd_utils.h
 * @brief SIMD utilities for audio processing optimization
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * SIMD utility functions for audio processing performance optimization
 * Supports SSE2/AVX2 (x86) and NEON (ARM)
 */

#ifndef VOICE_SIMD_UTILS_H
#define VOICE_SIMD_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * SIMD Feature Detection
 * ============================================ */

/** SIMD capability flags */
typedef enum {
    VOICE_SIMD_NONE     = 0,
    VOICE_SIMD_SSE2     = (1 << 0),
    VOICE_SIMD_SSE4_1   = (1 << 1),
    VOICE_SIMD_AVX      = (1 << 2),
    VOICE_SIMD_AVX2     = (1 << 3),
    VOICE_SIMD_AVX512   = (1 << 4),
    VOICE_SIMD_NEON     = (1 << 5),
} voice_simd_flags_t;

/**
 * @brief Detect SIMD capabilities of current CPU
 * @return Combination of SIMD capability flags
 */
VOICE_API uint32_t voice_simd_detect(void);

/**
 * @brief Check if specified SIMD is supported
 */
VOICE_API bool voice_simd_supported(voice_simd_flags_t flag);

/**
 * @brief Get SIMD capability description string
 */
VOICE_API const char *voice_simd_get_description(void);

/* ============================================
 * Memory Alignment
 * ============================================ */

/** Cache line size */
#define VOICE_CACHE_LINE_SIZE 64

/** SIMD alignment (AVX-512 requires 64 bytes) */
#define VOICE_SIMD_ALIGN 64

/**
 * @brief Allocate aligned memory
 * @param size Size
 * @param alignment Alignment (must be power of 2)
 */
VOICE_API void *voice_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief Free aligned memory
 */
VOICE_API void voice_aligned_free(void *ptr);

/**
 * @brief Check if pointer is aligned
 */
static inline bool voice_is_aligned(const void *ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

/* ============================================
 * Audio Format Conversion (SIMD Optimized)
 * ============================================ */

/**
 * @brief int16 to float (normalized to [-1, 1])
 * Automatically selects optimal SIMD implementation
 */
VOICE_API void voice_int16_to_float(
    const int16_t *src,
    float *dst,
    size_t count
);

/**
 * @brief float to int16 (with saturation)
 * Automatically selects optimal SIMD implementation
 */
VOICE_API void voice_float_to_int16(
    const float *src,
    int16_t *dst,
    size_t count
);

/**
 * @brief int16 to float32 (interleaved stereo)
 */
VOICE_API void voice_int16_to_float_stereo(
    const int16_t *src,
    float *dst_left,
    float *dst_right,
    size_t frames
);

/**
 * @brief float32 to int16 (interleaved stereo)
 */
VOICE_API void voice_float_to_int16_stereo(
    const float *src_left,
    const float *src_right,
    int16_t *dst,
    size_t frames
);

/* ============================================
 * Audio Processing (SIMD Optimized)
 * ============================================ */

/**
 * @brief Apply gain (int16)
 * @param samples Audio samples (modified in-place)
 * @param count Sample count
 * @param gain Gain (linear)
 */
VOICE_API void voice_apply_gain_int16(
    int16_t *samples,
    size_t count,
    float gain
);

/**
 * @brief Apply gain (float)
 */
VOICE_API void voice_apply_gain_float(
    float *samples,
    size_t count,
    float gain
);

/**
 * @brief Mix two audio streams (int16)
 * dst[i] = dst[i] + src[i] (with saturation)
 */
VOICE_API void voice_mix_add_int16(
    int16_t *dst,
    const int16_t *src,
    size_t count
);

/**
 * @brief Mix two audio streams (float)
 * dst[i] = dst[i] + src[i]
 */
VOICE_API void voice_mix_add_float(
    float *dst,
    const float *src,
    size_t count
);

/**
 * @brief Mix two audio streams (with gain)
 * dst[i] = dst[i] * dst_gain + src[i] * src_gain
 */
VOICE_API void voice_mix_with_gain_float(
    float *dst,
    const float *src,
    size_t count,
    float dst_gain,
    float src_gain
);

/**
 * @brief Find peak value (int16)
 * @return Maximum absolute sample value
 */
VOICE_API int16_t voice_find_peak_int16(const int16_t *samples, size_t count);

/**
 * @brief Find peak value (float)
 * @return Maximum absolute sample value
 */
VOICE_API float voice_find_peak_float(const float *samples, size_t count);

/**
 * @brief Compute energy/power (int16)
 * @return sum(samples[i]^2) / count
 */
VOICE_API float voice_compute_energy_int16(const int16_t *samples, size_t count);

/**
 * @brief Compute energy/power (float)
 */
VOICE_API float voice_compute_energy_float(const float *samples, size_t count);

/**
 * @brief Compute dot product (float)
 * @return sum(a[i] * b[i])
 */
VOICE_API float voice_dot_product_float(const float *a, const float *b, size_t count);

/**
 * @brief Soft limiter (tanh style)
 */
VOICE_API void voice_soft_clip_float(float *samples, size_t count, float threshold);

/**
 * @brief Hard limiter
 */
VOICE_API void voice_hard_clip_int16(int16_t *samples, size_t count, int16_t threshold);

/* ============================================
 * Complex Operations (SIMD Optimized) - For FFT/IFFT
 * ============================================ */

/**
 * @brief Complex multiplication (split real/imag format)
 *
 * result_r[i] = a_r[i] * b_r[i] - a_i[i] * b_i[i]
 * result_i[i] = a_r[i] * b_i[i] + a_i[i] * b_r[i]
 */
VOICE_API void voice_complex_mul(
    const float *a_real, const float *a_imag,
    const float *b_real, const float *b_imag,
    float *result_real, float *result_imag,
    size_t count
);

/**
 * @brief Complex multiply by conjugate
 *
 * result_r[i] = a_r[i] * b_r[i] + a_i[i] * b_i[i]
 * result_i[i] = a_i[i] * b_r[i] - a_r[i] * b_i[i]
 */
VOICE_API void voice_complex_mul_conj(
    const float *a_real, const float *a_imag,
    const float *b_real, const float *b_imag,
    float *result_real, float *result_imag,
    size_t count
);

/**
 * @brief Compute complex magnitude
 *
 * mag[i] = sqrt(real[i]^2 + imag[i]^2)
 */
VOICE_API void voice_complex_magnitude(
    const float *real, const float *imag,
    float *magnitude,
    size_t count
);

/**
 * @brief Complex divide by magnitude (PHAT weighting)
 *
 * result_r[i] = real[i] / mag[i]
 * result_i[i] = imag[i] / mag[i]
 */
VOICE_API void voice_complex_normalize(
    float *real, float *imag,
    size_t count,
    float min_magnitude
);

/**
 * @brief FFT butterfly operation (radix-2)
 *
 * In-place processing: handles pairs (i, i+half_step)
 */
VOICE_API void voice_fft_butterfly(
    float *real, float *imag,
    size_t n, size_t step,
    float wr_init, float wi_init,
    float wpr, float wpi
);

/* ============================================
 * Batch Operations
 * ============================================ */

/**
 * @brief Zero memory (SIMD optimized)
 */
VOICE_API void voice_memzero(void *dst, size_t size);

/**
 * @brief Copy memory (SIMD optimized)
 */
VOICE_API void voice_memcpy(void *dst, const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_SIMD_UTILS_H */
