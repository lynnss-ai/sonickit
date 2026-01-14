/**
 * @file simd_utils.h
 * @brief SIMD utilities for audio processing optimization
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * SIMD 工具函数，用于音频处理性能优化
 * 支持 SSE2/AVX2 (x86) 和 NEON (ARM)
 */

#ifndef VOICE_SIMD_UTILS_H
#define VOICE_SIMD_UTILS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * SIMD 特性检测
 * ============================================ */

/** SIMD 能力标志 */
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
 * @brief 检测当前 CPU 的 SIMD 能力
 * @return SIMD 能力标志组合
 */
uint32_t voice_simd_detect(void);

/**
 * @brief 检查是否支持指定 SIMD
 */
bool voice_simd_supported(voice_simd_flags_t flag);

/**
 * @brief 获取 SIMD 能力描述字符串
 */
const char *voice_simd_get_description(void);

/* ============================================
 * 内存对齐
 * ============================================ */

/** 缓存行大小 */
#define VOICE_CACHE_LINE_SIZE 64

/** SIMD 对齐 (AVX-512 需要 64 字节) */
#define VOICE_SIMD_ALIGN 64

/**
 * @brief 分配对齐内存
 * @param size 大小
 * @param alignment 对齐 (必须是 2 的幂)
 */
void *voice_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief 释放对齐内存
 */
void voice_aligned_free(void *ptr);

/**
 * @brief 检查指针是否对齐
 */
static inline bool voice_is_aligned(const void *ptr, size_t alignment) {
    return ((uintptr_t)ptr & (alignment - 1)) == 0;
}

/* ============================================
 * 音频格式转换 (SIMD 优化)
 * ============================================ */

/**
 * @brief int16 转 float (归一化到 [-1, 1])
 * 自动选择最优 SIMD 实现
 */
void voice_int16_to_float(
    const int16_t *src,
    float *dst,
    size_t count
);

/**
 * @brief float 转 int16 (带饱和)
 * 自动选择最优 SIMD 实现
 */
void voice_float_to_int16(
    const float *src,
    int16_t *dst,
    size_t count
);

/**
 * @brief int16 转 float32 (交错立体声)
 */
void voice_int16_to_float_stereo(
    const int16_t *src,
    float *dst_left,
    float *dst_right,
    size_t frames
);

/**
 * @brief float32 转 int16 (交错立体声)
 */
void voice_float_to_int16_stereo(
    const float *src_left,
    const float *src_right,
    int16_t *dst,
    size_t frames
);

/* ============================================
 * 音频处理 (SIMD 优化)
 * ============================================ */

/**
 * @brief 应用增益 (int16)
 * @param samples 音频样本 (原地修改)
 * @param count 样本数
 * @param gain 增益 (线性)
 */
void voice_apply_gain_int16(
    int16_t *samples,
    size_t count,
    float gain
);

/**
 * @brief 应用增益 (float)
 */
void voice_apply_gain_float(
    float *samples,
    size_t count,
    float gain
);

/**
 * @brief 混合两个音频流 (int16)
 * dst[i] = dst[i] + src[i] (带饱和)
 */
void voice_mix_add_int16(
    int16_t *dst,
    const int16_t *src,
    size_t count
);

/**
 * @brief 混合两个音频流 (float)
 * dst[i] = dst[i] + src[i]
 */
void voice_mix_add_float(
    float *dst,
    const float *src,
    size_t count
);

/**
 * @brief 混合两个音频流 (带增益)
 * dst[i] = dst[i] * dst_gain + src[i] * src_gain
 */
void voice_mix_with_gain_float(
    float *dst,
    const float *src,
    size_t count,
    float dst_gain,
    float src_gain
);

/**
 * @brief 计算峰值 (int16)
 * @return 绝对值最大的样本值
 */
int16_t voice_find_peak_int16(const int16_t *samples, size_t count);

/**
 * @brief 计算峰值 (float)
 * @return 绝对值最大的样本值
 */
float voice_find_peak_float(const float *samples, size_t count);

/**
 * @brief 计算能量/功率 (int16)
 * @return sum(samples[i]^2) / count
 */
float voice_compute_energy_int16(const int16_t *samples, size_t count);

/**
 * @brief 计算能量/功率 (float)
 */
float voice_compute_energy_float(const float *samples, size_t count);

/**
 * @brief 软限制器 (tanh 风格)
 */
void voice_soft_clip_float(float *samples, size_t count, float threshold);

/**
 * @brief 硬限制器
 */
void voice_hard_clip_int16(int16_t *samples, size_t count, int16_t threshold);

/* ============================================
 * 复数运算 (SIMD 优化) - FFT/IFFT 用
 * ============================================ */

/**
 * @brief 复数乘法 (分离实虚部格式)
 *
 * result_r[i] = a_r[i] * b_r[i] - a_i[i] * b_i[i]
 * result_i[i] = a_r[i] * b_i[i] + a_i[i] * b_r[i]
 */
void voice_complex_mul(
    const float *a_real, const float *a_imag,
    const float *b_real, const float *b_imag,
    float *result_real, float *result_imag,
    size_t count
);

/**
 * @brief 复数乘以共轭
 *
 * result_r[i] = a_r[i] * b_r[i] + a_i[i] * b_i[i]
 * result_i[i] = a_i[i] * b_r[i] - a_r[i] * b_i[i]
 */
void voice_complex_mul_conj(
    const float *a_real, const float *a_imag,
    const float *b_real, const float *b_imag,
    float *result_real, float *result_imag,
    size_t count
);

/**
 * @brief 计算复数幅度
 *
 * mag[i] = sqrt(real[i]^2 + imag[i]^2)
 */
void voice_complex_magnitude(
    const float *real, const float *imag,
    float *magnitude,
    size_t count
);

/**
 * @brief 复数除以幅度 (PHAT 加权)
 *
 * result_r[i] = real[i] / mag[i]
 * result_i[i] = imag[i] / mag[i]
 */
void voice_complex_normalize(
    float *real, float *imag,
    size_t count,
    float min_magnitude
);

/**
 * @brief FFT 蝶形运算 (radix-2)
 *
 * 就地处理: 处理配对 (i, i+half_step)
 */
void voice_fft_butterfly(
    float *real, float *imag,
    size_t n, size_t step,
    float wr_init, float wi_init,
    float wpr, float wpi
);

/* ============================================
 * 批量操作
 * ============================================ */

/**
 * @brief 清零内存 (SIMD 优化)
 */
void voice_memzero(void *dst, size_t size);

/**
 * @brief 复制内存 (SIMD 优化)
 */
void voice_memcpy(void *dst, const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_SIMD_UTILS_H */
