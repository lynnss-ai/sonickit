/**
 * @file simd_utils.c
 * @brief SIMD utilities implementation
 */

#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * SIMD 检测
 * ============================================ */

#if defined(_MSC_VER)
    #include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
    #if defined(__x86_64__) || defined(__i386__)
        #include <cpuid.h>
    #endif
#endif

/* x86 SIMD 内联头文件 */
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define VOICE_X86 1
    #ifdef __SSE2__
        #include <emmintrin.h>
        #define VOICE_HAS_SSE2 1
    #endif
    #ifdef __SSE4_1__
        #include <smmintrin.h>
        #define VOICE_HAS_SSE4_1 1
    #endif
    #ifdef __AVX__
        #include <immintrin.h>
        #define VOICE_HAS_AVX 1
    #endif
    #ifdef __AVX2__
        #include <immintrin.h>
        #define VOICE_HAS_AVX2 1
    #endif
#endif

/* ARM NEON */
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    #include <arm_neon.h>
    #define VOICE_HAS_NEON 1
#endif

static uint32_t g_simd_flags = 0;
static bool g_simd_detected = false;

uint32_t voice_simd_detect(void) {
    if (g_simd_detected) {
        return g_simd_flags;
    }
    
    g_simd_flags = VOICE_SIMD_NONE;
    
#if defined(VOICE_X86)
    #if defined(_MSC_VER)
        int cpuinfo[4] = {0};
        __cpuid(cpuinfo, 1);
        
        if (cpuinfo[3] & (1 << 26)) g_simd_flags |= VOICE_SIMD_SSE2;
        if (cpuinfo[2] & (1 << 19)) g_simd_flags |= VOICE_SIMD_SSE4_1;
        if (cpuinfo[2] & (1 << 28)) g_simd_flags |= VOICE_SIMD_AVX;
        
        __cpuidex(cpuinfo, 7, 0);
        if (cpuinfo[1] & (1 << 5)) g_simd_flags |= VOICE_SIMD_AVX2;
        if (cpuinfo[1] & (1 << 16)) g_simd_flags |= VOICE_SIMD_AVX512;
        
    #elif defined(__GNUC__) || defined(__clang__)
        unsigned int eax, ebx, ecx, edx;
        
        if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
            if (edx & (1 << 26)) g_simd_flags |= VOICE_SIMD_SSE2;
            if (ecx & (1 << 19)) g_simd_flags |= VOICE_SIMD_SSE4_1;
            if (ecx & (1 << 28)) g_simd_flags |= VOICE_SIMD_AVX;
        }
        
        if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
            if (ebx & (1 << 5)) g_simd_flags |= VOICE_SIMD_AVX2;
            if (ebx & (1 << 16)) g_simd_flags |= VOICE_SIMD_AVX512;
        }
    #endif
#endif

#if defined(VOICE_HAS_NEON)
    g_simd_flags |= VOICE_SIMD_NEON;
#endif
    
    g_simd_detected = true;
    return g_simd_flags;
}

bool voice_simd_supported(voice_simd_flags_t flag) {
    if (!g_simd_detected) {
        voice_simd_detect();
    }
    return (g_simd_flags & flag) != 0;
}

const char *voice_simd_get_description(void) {
    if (!g_simd_detected) {
        voice_simd_detect();
    }
    
    static char desc[128];
    desc[0] = '\0';
    
    if (g_simd_flags & VOICE_SIMD_AVX512) strcat(desc, "AVX-512 ");
    else if (g_simd_flags & VOICE_SIMD_AVX2) strcat(desc, "AVX2 ");
    else if (g_simd_flags & VOICE_SIMD_AVX) strcat(desc, "AVX ");
    else if (g_simd_flags & VOICE_SIMD_SSE4_1) strcat(desc, "SSE4.1 ");
    else if (g_simd_flags & VOICE_SIMD_SSE2) strcat(desc, "SSE2 ");
    else if (g_simd_flags & VOICE_SIMD_NEON) strcat(desc, "NEON ");
    else strcat(desc, "Scalar ");
    
    return desc;
}

/* ============================================
 * 内存对齐
 * ============================================ */

void *voice_aligned_alloc(size_t size, size_t alignment) {
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    return aligned_alloc(alignment, size);
#else
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) {
        return NULL;
    }
    return ptr;
#endif
}

void voice_aligned_free(void *ptr) {
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    free(ptr);
#endif
}

/* ============================================
 * 格式转换
 * ============================================ */

/* 标量版本 */
static void int16_to_float_scalar(const int16_t *src, float *dst, size_t count) {
    const float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < count; i++) {
        dst[i] = (float)src[i] * scale;
    }
}

static void float_to_int16_scalar(const float *src, int16_t *dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        float sample = src[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        dst[i] = (int16_t)sample;
    }
}

#if defined(VOICE_HAS_SSE2)
static void int16_to_float_sse2(const int16_t *src, float *dst, size_t count) {
    const __m128 scale = _mm_set1_ps(1.0f / 32768.0f);
    size_t i = 0;
    
    /* 处理对齐的部分 (每次 8 个样本) */
    for (; i + 8 <= count; i += 8) {
        __m128i s16 = _mm_loadu_si128((const __m128i *)(src + i));
        
        /* 低 4 个转换 */
        __m128i lo = _mm_unpacklo_epi16(s16, s16);
        lo = _mm_srai_epi32(lo, 16);
        __m128 flo = _mm_cvtepi32_ps(lo);
        flo = _mm_mul_ps(flo, scale);
        _mm_storeu_ps(dst + i, flo);
        
        /* 高 4 个转换 */
        __m128i hi = _mm_unpackhi_epi16(s16, s16);
        hi = _mm_srai_epi32(hi, 16);
        __m128 fhi = _mm_cvtepi32_ps(hi);
        fhi = _mm_mul_ps(fhi, scale);
        _mm_storeu_ps(dst + i + 4, fhi);
    }
    
    /* 处理剩余 */
    for (; i < count; i++) {
        dst[i] = (float)src[i] * (1.0f / 32768.0f);
    }
}

static void float_to_int16_sse2(const float *src, int16_t *dst, size_t count) {
    const __m128 scale = _mm_set1_ps(32768.0f);
    const __m128 max_val = _mm_set1_ps(32767.0f);
    const __m128 min_val = _mm_set1_ps(-32768.0f);
    size_t i = 0;
    
    for (; i + 8 <= count; i += 8) {
        /* 加载并缩放 */
        __m128 f0 = _mm_loadu_ps(src + i);
        __m128 f1 = _mm_loadu_ps(src + i + 4);
        
        f0 = _mm_mul_ps(f0, scale);
        f1 = _mm_mul_ps(f1, scale);
        
        /* 饱和 */
        f0 = _mm_min_ps(_mm_max_ps(f0, min_val), max_val);
        f1 = _mm_min_ps(_mm_max_ps(f1, min_val), max_val);
        
        /* 转换到 int32 */
        __m128i i0 = _mm_cvtps_epi32(f0);
        __m128i i1 = _mm_cvtps_epi32(f1);
        
        /* 打包到 int16 */
        __m128i packed = _mm_packs_epi32(i0, i1);
        _mm_storeu_si128((__m128i *)(dst + i), packed);
    }
    
    /* 处理剩余 */
    for (; i < count; i++) {
        float sample = src[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        dst[i] = (int16_t)sample;
    }
}
#endif

#if defined(VOICE_HAS_NEON)
static void int16_to_float_neon(const int16_t *src, float *dst, size_t count) {
    const float32x4_t scale = vdupq_n_f32(1.0f / 32768.0f);
    size_t i = 0;
    
    for (; i + 8 <= count; i += 8) {
        int16x8_t s16 = vld1q_s16(src + i);
        
        int32x4_t lo = vmovl_s16(vget_low_s16(s16));
        int32x4_t hi = vmovl_s16(vget_high_s16(s16));
        
        float32x4_t flo = vcvtq_f32_s32(lo);
        float32x4_t fhi = vcvtq_f32_s32(hi);
        
        flo = vmulq_f32(flo, scale);
        fhi = vmulq_f32(fhi, scale);
        
        vst1q_f32(dst + i, flo);
        vst1q_f32(dst + i + 4, fhi);
    }
    
    for (; i < count; i++) {
        dst[i] = (float)src[i] * (1.0f / 32768.0f);
    }
}

static void float_to_int16_neon(const float *src, int16_t *dst, size_t count) {
    const float32x4_t scale = vdupq_n_f32(32768.0f);
    size_t i = 0;
    
    for (; i + 8 <= count; i += 8) {
        float32x4_t f0 = vld1q_f32(src + i);
        float32x4_t f1 = vld1q_f32(src + i + 4);
        
        f0 = vmulq_f32(f0, scale);
        f1 = vmulq_f32(f1, scale);
        
        int32x4_t i0 = vcvtq_s32_f32(f0);
        int32x4_t i1 = vcvtq_s32_f32(f1);
        
        int16x4_t s0 = vqmovn_s32(i0);
        int16x4_t s1 = vqmovn_s32(i1);
        
        vst1q_s16(dst + i, vcombine_s16(s0, s1));
    }
    
    for (; i < count; i++) {
        float sample = src[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        dst[i] = (int16_t)sample;
    }
}
#endif

/* 公共接口 - 自动选择最优实现 */
void voice_int16_to_float(const int16_t *src, float *dst, size_t count) {
    if (!g_simd_detected) {
        voice_simd_detect();
    }
    
#if defined(VOICE_HAS_NEON)
    int16_to_float_neon(src, dst, count);
#elif defined(VOICE_HAS_SSE2)
    if (g_simd_flags & VOICE_SIMD_SSE2) {
        int16_to_float_sse2(src, dst, count);
        return;
    }
    int16_to_float_scalar(src, dst, count);
#else
    int16_to_float_scalar(src, dst, count);
#endif
}

void voice_float_to_int16(const float *src, int16_t *dst, size_t count) {
    if (!g_simd_detected) {
        voice_simd_detect();
    }
    
#if defined(VOICE_HAS_NEON)
    float_to_int16_neon(src, dst, count);
#elif defined(VOICE_HAS_SSE2)
    if (g_simd_flags & VOICE_SIMD_SSE2) {
        float_to_int16_sse2(src, dst, count);
        return;
    }
    float_to_int16_scalar(src, dst, count);
#else
    float_to_int16_scalar(src, dst, count);
#endif
}

void voice_int16_to_float_stereo(
    const int16_t *src,
    float *dst_left,
    float *dst_right,
    size_t frames
) {
    const float scale = 1.0f / 32768.0f;
    for (size_t i = 0; i < frames; i++) {
        dst_left[i] = (float)src[i * 2] * scale;
        dst_right[i] = (float)src[i * 2 + 1] * scale;
    }
}

void voice_float_to_int16_stereo(
    const float *src_left,
    const float *src_right,
    int16_t *dst,
    size_t frames
) {
    for (size_t i = 0; i < frames; i++) {
        float l = src_left[i] * 32768.0f;
        float r = src_right[i] * 32768.0f;
        
        if (l > 32767.0f) l = 32767.0f;
        if (l < -32768.0f) l = -32768.0f;
        if (r > 32767.0f) r = 32767.0f;
        if (r < -32768.0f) r = -32768.0f;
        
        dst[i * 2] = (int16_t)l;
        dst[i * 2 + 1] = (int16_t)r;
    }
}

/* ============================================
 * 音频处理
 * ============================================ */

void voice_apply_gain_int16(int16_t *samples, size_t count, float gain) {
    if (gain == 1.0f) return;
    
    for (size_t i = 0; i < count; i++) {
        float sample = (float)samples[i] * gain;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        samples[i] = (int16_t)sample;
    }
}

void voice_apply_gain_float(float *samples, size_t count, float gain) {
    if (gain == 1.0f) return;
    
#if defined(VOICE_HAS_SSE2)
    if (g_simd_flags & VOICE_SIMD_SSE2) {
        __m128 g = _mm_set1_ps(gain);
        size_t i = 0;
        
        for (; i + 4 <= count; i += 4) {
            __m128 s = _mm_loadu_ps(samples + i);
            s = _mm_mul_ps(s, g);
            _mm_storeu_ps(samples + i, s);
        }
        
        for (; i < count; i++) {
            samples[i] *= gain;
        }
        return;
    }
#endif

#if defined(VOICE_HAS_NEON)
    float32x4_t g = vdupq_n_f32(gain);
    size_t i = 0;
    
    for (; i + 4 <= count; i += 4) {
        float32x4_t s = vld1q_f32(samples + i);
        s = vmulq_f32(s, g);
        vst1q_f32(samples + i, s);
    }
    
    for (; i < count; i++) {
        samples[i] *= gain;
    }
    return;
#endif

    /* 标量版本 */
    for (size_t i = 0; i < count; i++) {
        samples[i] *= gain;
    }
}

void voice_mix_add_int16(int16_t *dst, const int16_t *src, size_t count) {
    for (size_t i = 0; i < count; i++) {
        int32_t sum = (int32_t)dst[i] + (int32_t)src[i];
        if (sum > 32767) sum = 32767;
        if (sum < -32768) sum = -32768;
        dst[i] = (int16_t)sum;
    }
}

void voice_mix_add_float(float *dst, const float *src, size_t count) {
#if defined(VOICE_HAS_SSE2)
    if (g_simd_flags & VOICE_SIMD_SSE2) {
        size_t i = 0;
        for (; i + 4 <= count; i += 4) {
            __m128 d = _mm_loadu_ps(dst + i);
            __m128 s = _mm_loadu_ps(src + i);
            d = _mm_add_ps(d, s);
            _mm_storeu_ps(dst + i, d);
        }
        for (; i < count; i++) {
            dst[i] += src[i];
        }
        return;
    }
#endif

#if defined(VOICE_HAS_NEON)
    size_t i = 0;
    for (; i + 4 <= count; i += 4) {
        float32x4_t d = vld1q_f32(dst + i);
        float32x4_t s = vld1q_f32(src + i);
        d = vaddq_f32(d, s);
        vst1q_f32(dst + i, d);
    }
    for (; i < count; i++) {
        dst[i] += src[i];
    }
    return;
#endif

    for (size_t i = 0; i < count; i++) {
        dst[i] += src[i];
    }
}

void voice_mix_with_gain_float(
    float *dst,
    const float *src,
    size_t count,
    float dst_gain,
    float src_gain
) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = dst[i] * dst_gain + src[i] * src_gain;
    }
}

int16_t voice_find_peak_int16(const int16_t *samples, size_t count) {
    int16_t peak = 0;
    for (size_t i = 0; i < count; i++) {
        int16_t abs_val = samples[i] < 0 ? -samples[i] : samples[i];
        if (abs_val > peak) peak = abs_val;
    }
    return peak;
}

float voice_find_peak_float(const float *samples, size_t count) {
    float peak = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float abs_val = samples[i] < 0.0f ? -samples[i] : samples[i];
        if (abs_val > peak) peak = abs_val;
    }
    return peak;
}

float voice_compute_energy_int16(const int16_t *samples, size_t count) {
    if (count == 0) return 0.0f;
    
    int64_t sum_sq = 0;
    for (size_t i = 0; i < count; i++) {
        sum_sq += (int32_t)samples[i] * (int32_t)samples[i];
    }
    return (float)sum_sq / (float)count;
}

float voice_compute_energy_float(const float *samples, size_t count) {
    if (count == 0) return 0.0f;
    
    float sum_sq = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum_sq += samples[i] * samples[i];
    }
    return sum_sq / (float)count;
}

void voice_soft_clip_float(float *samples, size_t count, float threshold) {
    for (size_t i = 0; i < count; i++) {
        if (samples[i] > threshold) {
            samples[i] = threshold + (1.0f - threshold) * tanhf((samples[i] - threshold) / (1.0f - threshold));
        } else if (samples[i] < -threshold) {
            samples[i] = -threshold - (1.0f - threshold) * tanhf((-samples[i] - threshold) / (1.0f - threshold));
        }
    }
}

void voice_hard_clip_int16(int16_t *samples, size_t count, int16_t threshold) {
    for (size_t i = 0; i < count; i++) {
        if (samples[i] > threshold) samples[i] = threshold;
        else if (samples[i] < -threshold) samples[i] = -threshold;
    }
}

void voice_memzero(void *dst, size_t size) {
    memset(dst, 0, size);
}

void voice_memcpy(void *dst, const void *src, size_t size) {
    memcpy(dst, src, size);
}
