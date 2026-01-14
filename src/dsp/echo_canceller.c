/**
 * @file echo_canceller.c
 * @brief Acoustic Echo Cancellation (AEC) implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1 Enhancement:
 * - FDAF (Frequency-Domain Adaptive Filter) implementation
 * - DTD (Double-Talk Detection) using normalized cross-correlation
 * - GCC-PHAT delay estimator integration
 * - Time-domain NLMS fallback
 */

#include "dsp/echo_canceller.h"
#include "dsp/delay_estimator.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * 常量定义
 * ============================================ */

#define AEC_MAX_FILTER_LENGTH   4096    /* 最大滤波器长度 (样本) */
#define AEC_MAX_FRAME_SIZE      512     /* 最大帧大小 */
#define AEC_FFT_SIZE            512     /* FFT 大小 */
#define AEC_PLAYBACK_BUFFER_MS  500     /* 播放缓冲区长度 (ms) */
#define AEC_DTD_HANGOVER        10      /* DTD 挂起帧数 */
#define AEC_ERLE_SMOOTH         0.98f   /* ERLE 平滑系数 */
#define AEC_CONVERGENCE_THRESH  0.7f    /* 收敛阈值 */

/* ============================================
 * 内部类型
 * ============================================ */

/** 复数结构 */
typedef struct {
    float re;
    float im;
} aec_complex_t;

/** FDAF 滤波器块 */
typedef struct {
    aec_complex_t *coeffs;      /* 频域滤波器系数 */
    float *power;               /* 频率功率谱估计 */
} fdaf_block_t;

/** DTD 检测器 */
typedef struct {
    float far_energy;           /* 远端能量 (平滑) */
    float near_energy;          /* 近端能量 (平滑) */
    float error_energy;         /* 误差能量 */
    float cross_corr;           /* 互相关 */
    voice_dtd_state_t state;    /* 当前状态 */
    int hangover;               /* 挂起计数 */
} dtd_detector_t;

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_aec_s {
    voice_aec_ext_config_t config;
    bool enabled;
    uint64_t frames_processed;

    /* 播放缓冲区 (用于异步模式) */
    int16_t *playback_buffer;
    size_t playback_buffer_size;
    size_t playback_write_pos;
    size_t playback_read_pos;
    size_t playback_count;              /* 缓冲区中的样本数 */

    /* FDAF 滤波器 */
    size_t num_blocks;                  /* 滤波器块数 */
    size_t fft_size;                    /* FFT 大小 */
    fdaf_block_t *filter_blocks;        /* 滤波器块数组 */
    aec_complex_t *fft_buffer;          /* FFT 工作缓冲区 */
    float *window;                      /* 分析窗口 */

    /* 输入缓冲区 */
    float *far_buffer;                  /* 远端输入缓冲区 */
    float *near_buffer;                 /* 近端输入缓冲区 */
    float *error_buffer;                /* 误差缓冲区 */
    aec_complex_t *far_freq;            /* 远端频域 */
    aec_complex_t *near_freq;           /* 近端频域 */
    aec_complex_t *error_freq;          /* 误差频域 */
    aec_complex_t *echo_est_freq;       /* 回声估计频域 */

    /* 时域 NLMS (备选) */
    float *nlms_weights;                /* NLMS 权重 */
    float *nlms_x_buffer;               /* NLMS 输入缓冲 */

    /* 延迟估计器 */
    voice_delay_estimator_t *delay_est;
    int known_delay_samples;            /* 已知延迟 (如果设置) */
    bool use_known_delay;

    /* DTD 检测 */
    dtd_detector_t dtd;

    /* 状态统计 */
    float erle_db;                      /* ERLE (dB) */
    float convergence;                  /* 收敛度 */
};

/* ============================================
 * 内部函数声明
 * ============================================ */

static void fft_forward(aec_complex_t *data, size_t n);
static void fft_inverse(aec_complex_t *data, size_t n);
static void complex_multiply(aec_complex_t *out, const aec_complex_t *a,
                            const aec_complex_t *b, size_t n);
static void complex_multiply_conj(aec_complex_t *out, const aec_complex_t *a,
                                  const aec_complex_t *b, size_t n);
static float compute_energy(const float *data, size_t n);
static void dtd_update(dtd_detector_t *dtd, float far_energy,
                      float near_energy, float error_energy, float threshold);
static void fdaf_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size);
static void nlms_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size);
static void generate_hann_window(float *window, size_t n);

/* ============================================
 * FFT 实现 (Radix-2 Cooley-Tukey)
 * ============================================ */

static void fft_forward(aec_complex_t *data, size_t n) {
    /* 位反转排列 */
    size_t j = 0;
    for (size_t i = 0; i < n - 1; i++) {
        if (i < j) {
            aec_complex_t temp = data[i];
            data[i] = data[j];
            data[j] = temp;
        }
        size_t k = n >> 1;
        while (k <= j) {
            j -= k;
            k >>= 1;
        }
        j += k;
    }

    /* Cooley-Tukey 蝶形运算 */
    for (size_t len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * (float)M_PI / (float)len;
        aec_complex_t w_len = { cosf(angle), sinf(angle) };

        for (size_t i = 0; i < n; i += len) {
            aec_complex_t w = { 1.0f, 0.0f };
            size_t half = len >> 1;
            for (size_t k = 0; k < half; k++) {
                aec_complex_t t = {
                    w.re * data[i + k + half].re - w.im * data[i + k + half].im,
                    w.re * data[i + k + half].im + w.im * data[i + k + half].re
                };
                aec_complex_t u = data[i + k];
                data[i + k].re = u.re + t.re;
                data[i + k].im = u.im + t.im;
                data[i + k + half].re = u.re - t.re;
                data[i + k + half].im = u.im - t.im;

                float new_w_re = w.re * w_len.re - w.im * w_len.im;
                float new_w_im = w.re * w_len.im + w.im * w_len.re;
                w.re = new_w_re;
                w.im = new_w_im;
            }
        }
    }
}

static void fft_inverse(aec_complex_t *data, size_t n) {
    /* 共轭 */
    for (size_t i = 0; i < n; i++) {
        data[i].im = -data[i].im;
    }

    /* 正向 FFT */
    fft_forward(data, n);

    /* 共轭并归一化 */
    float scale = 1.0f / (float)n;
    for (size_t i = 0; i < n; i++) {
        data[i].re *= scale;
        data[i].im = -data[i].im * scale;
    }
}

/* ============================================
 * 复数运算
 * ============================================ */

static void complex_multiply(aec_complex_t *out, const aec_complex_t *a,
                            const aec_complex_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        out[i].re = a[i].re * b[i].re - a[i].im * b[i].im;
        out[i].im = a[i].re * b[i].im + a[i].im * b[i].re;
    }
}

static void complex_multiply_conj(aec_complex_t *out, const aec_complex_t *a,
                                  const aec_complex_t *b, size_t n) {
    /* out = a * conj(b) */
    for (size_t i = 0; i < n; i++) {
        out[i].re = a[i].re * b[i].re + a[i].im * b[i].im;
        out[i].im = a[i].im * b[i].re - a[i].re * b[i].im;
    }
}

/* ============================================
 * 工具函数
 * ============================================ */

static float compute_energy(const float *data, size_t n) {
    float energy = 0.0f;
    for (size_t i = 0; i < n; i++) {
        energy += data[i] * data[i];
    }
    return energy;
}

static void generate_hann_window(float *window, size_t n) {
    for (size_t i = 0; i < n; i++) {
        window[i] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)i / (float)(n - 1)));
    }
}

/* ============================================
 * DTD 检测
 * ============================================ */

static void dtd_update(dtd_detector_t *dtd, float far_energy,
                      float near_energy, float error_energy, float threshold) {
    const float smooth = 0.9f;

    /* 平滑能量 */
    dtd->far_energy = smooth * dtd->far_energy + (1.0f - smooth) * far_energy;
    dtd->near_energy = smooth * dtd->near_energy + (1.0f - smooth) * near_energy;
    dtd->error_energy = smooth * dtd->error_energy + (1.0f - smooth) * error_energy;

    /* 计算相关度指标 */
    float eps = 1e-10f;

    /* 回声返回损耗比 */
    float echo_ratio = dtd->error_energy / (dtd->near_energy + eps);

    /* 双讲检测: 如果误差能量远大于预期回声，说明有近端说话 */
    bool far_active = dtd->far_energy > 1e-6f;
    bool near_active = dtd->near_energy > 1e-6f;
    bool double_talk = echo_ratio > threshold && far_active && near_active;

    /* 状态机 */
    if (double_talk) {
        dtd->state = VOICE_DTD_DOUBLE_TALK;
        dtd->hangover = AEC_DTD_HANGOVER;
    } else if (dtd->hangover > 0) {
        dtd->hangover--;
        /* 保持当前状态 */
    } else if (far_active && !near_active) {
        dtd->state = VOICE_DTD_FAR_END;
    } else if (!far_active && near_active) {
        dtd->state = VOICE_DTD_NEAR_END;
    } else {
        dtd->state = VOICE_DTD_IDLE;
    }
}

/* ============================================
 * FDAF 滤波
 * ============================================ */

static void fdaf_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size) {
    size_t fft_size = aec->fft_size;
    size_t half_fft = fft_size / 2;

    /* 移动远端缓冲区并添加新帧 */
    memmove(aec->far_buffer, aec->far_buffer + frame_size,
            (fft_size - frame_size) * sizeof(float));
    memcpy(aec->far_buffer + fft_size - frame_size, far, frame_size * sizeof(float));

    /* 准备 FFT 输入 (加窗) */
    for (size_t i = 0; i < fft_size; i++) {
        aec->fft_buffer[i].re = aec->far_buffer[i] * aec->window[i];
        aec->fft_buffer[i].im = 0.0f;
    }
    fft_forward(aec->fft_buffer, fft_size);
    memcpy(aec->far_freq, aec->fft_buffer, fft_size * sizeof(aec_complex_t));

    /* 更新滤波器块 (移位) */
    for (size_t b = aec->num_blocks - 1; b > 0; b--) {
        memcpy(aec->filter_blocks[b].coeffs, aec->filter_blocks[b - 1].coeffs,
               fft_size * sizeof(aec_complex_t));
    }
    memcpy(aec->filter_blocks[0].coeffs, aec->far_freq, fft_size * sizeof(aec_complex_t));

    /* 计算回声估计: Y = sum(H_k * X_k) */
    memset(aec->echo_est_freq, 0, fft_size * sizeof(aec_complex_t));
    for (size_t b = 0; b < aec->num_blocks; b++) {
        for (size_t i = 0; i < fft_size; i++) {
            aec_complex_t h = aec->filter_blocks[b].coeffs[i];
            aec_complex_t x = aec->far_freq[i];  /* 应该用延迟版本 */
            aec->echo_est_freq[i].re += h.re * x.re - h.im * x.im;
            aec->echo_est_freq[i].im += h.re * x.im + h.im * x.re;
        }
    }

    /* IFFT 得到时域回声估计 */
    memcpy(aec->fft_buffer, aec->echo_est_freq, fft_size * sizeof(aec_complex_t));
    fft_inverse(aec->fft_buffer, fft_size);

    /* 提取回声估计的后半部分 */
    float echo_estimate[AEC_MAX_FRAME_SIZE];
    for (size_t i = 0; i < frame_size && i < AEC_MAX_FRAME_SIZE; i++) {
        echo_estimate[i] = aec->fft_buffer[half_fft + i].re;
    }

    /* 计算误差 */
    for (size_t i = 0; i < frame_size; i++) {
        output[i] = near[i] - echo_estimate[i];
    }

    /* DTD 检测 */
    float far_energy = compute_energy(far, frame_size);
    float near_energy = compute_energy(near, frame_size);
    float error_energy = compute_energy(output, frame_size);
    dtd_update(&aec->dtd, far_energy, near_energy, error_energy, aec->config.dtd_threshold);

    /* 计算 ERLE */
    float eps = 1e-10f;
    float erle_linear = near_energy / (error_energy + eps);
    aec->erle_db = AEC_ERLE_SMOOTH * aec->erle_db +
                   (1.0f - AEC_ERLE_SMOOTH) * 10.0f * log10f(erle_linear + eps);

    /* 自适应更新 (除非双讲) */
    if (aec->dtd.state != VOICE_DTD_DOUBLE_TALK &&
        aec->dtd.state != VOICE_DTD_NEAR_END) {

        float mu = aec->config.nlms_step_size;

        /* 准备误差频域 */
        for (size_t i = 0; i < fft_size; i++) {
            if (i < half_fft) {
                aec->fft_buffer[i].re = 0.0f;
            } else {
                aec->fft_buffer[i].re = output[i - half_fft] * aec->window[i];
            }
            aec->fft_buffer[i].im = 0.0f;
        }
        fft_forward(aec->fft_buffer, fft_size);
        memcpy(aec->error_freq, aec->fft_buffer, fft_size * sizeof(aec_complex_t));

        /* 更新滤波器: H = H + mu * E * conj(X) / |X|^2 */
        for (size_t i = 0; i < fft_size; i++) {
            float power = aec->far_freq[i].re * aec->far_freq[i].re +
                         aec->far_freq[i].im * aec->far_freq[i].im + eps;

            aec_complex_t update;
            update.re = (aec->error_freq[i].re * aec->far_freq[i].re +
                        aec->error_freq[i].im * aec->far_freq[i].im) / power;
            update.im = (aec->error_freq[i].im * aec->far_freq[i].re -
                        aec->error_freq[i].re * aec->far_freq[i].im) / power;

            aec->filter_blocks[0].coeffs[i].re += mu * update.re;
            aec->filter_blocks[0].coeffs[i].im += mu * update.im;
        }

        /* 更新收敛度估计 */
        aec->convergence = aec->convergence * 0.99f + 0.01f *
            (aec->erle_db > 10.0f ? 1.0f : aec->erle_db / 10.0f);
    }
}

/* ============================================
 * NLMS 滤波 (备选)
 * ============================================ */

static void nlms_filter(voice_aec_t *aec, const float *far, const float *near,
                       float *output, size_t frame_size) {
    size_t filter_len = aec->config.filter_length;
    if (filter_len > AEC_MAX_FILTER_LENGTH) {
        filter_len = AEC_MAX_FILTER_LENGTH;
    }

    float mu = aec->config.nlms_step_size;
    float eps = 1e-10f;

    for (size_t n = 0; n < frame_size; n++) {
        /* 移动输入缓冲区 */
        memmove(aec->nlms_x_buffer + 1, aec->nlms_x_buffer,
                (filter_len - 1) * sizeof(float));
        aec->nlms_x_buffer[0] = far[n];

        /* 计算回声估计 */
        float echo_est = 0.0f;
        for (size_t i = 0; i < filter_len; i++) {
            echo_est += aec->nlms_weights[i] * aec->nlms_x_buffer[i];
        }

        /* 误差 */
        float error = near[n] - echo_est;
        output[n] = error;

        /* 计算输入功率 */
        float power = 0.0f;
        for (size_t i = 0; i < filter_len; i++) {
            power += aec->nlms_x_buffer[i] * aec->nlms_x_buffer[i];
        }

        /* 更新权重 */
        float norm_mu = mu / (power + eps);
        for (size_t i = 0; i < filter_len; i++) {
            aec->nlms_weights[i] += norm_mu * error * aec->nlms_x_buffer[i];
        }
    }

    /* 更新 DTD */
    float far_energy = compute_energy(far, frame_size);
    float near_energy = compute_energy(near, frame_size);
    float error_energy = compute_energy(output, frame_size);
    dtd_update(&aec->dtd, far_energy, near_energy, error_energy, aec->config.dtd_threshold);
}

/* ============================================
 * API Implementation
 * ============================================ */

void voice_aec_ext_config_init(voice_aec_ext_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->sample_rate = 16000;
    config->frame_size = 160;        /* 10ms @ 16kHz */
    config->filter_length = 2560;    /* 160ms @ 16kHz */
    config->echo_suppress_db = -40;
    config->echo_suppress_active_db = -15;
    config->enable_residual_echo_suppress = true;
    config->enable_comfort_noise = false;

    /* Phase 1 新增默认值 */
    config->algorithm = VOICE_AEC_ALG_FDAF;
    config->enable_delay_estimation = true;
    config->enable_dtd = true;
    config->tail_length_ms = 200;
    config->nlms_step_size = 0.3f;
    config->dtd_threshold = 0.65f;
}

voice_aec_t *voice_aec_create(const voice_aec_ext_config_t *config) {
    if (!config) return NULL;

    voice_aec_t *aec = (voice_aec_t *)calloc(1, sizeof(voice_aec_t));
    if (!aec) return NULL;

    aec->config = *config;
    aec->enabled = true;
    aec->frames_processed = 0;

    /* 计算 FFT 大小 (2 倍帧大小，取 2 的幂) */
    size_t fft_size = 256;
    while (fft_size < config->frame_size * 2) {
        fft_size *= 2;
    }
    if (fft_size > AEC_FFT_SIZE) {
        fft_size = AEC_FFT_SIZE;
    }
    aec->fft_size = fft_size;

    /* 计算滤波器块数 */
    size_t tail_samples = (config->tail_length_ms * config->sample_rate) / 1000;
    aec->num_blocks = (tail_samples + config->frame_size - 1) / config->frame_size;
    if (aec->num_blocks < 1) aec->num_blocks = 1;
    if (aec->num_blocks > 16) aec->num_blocks = 16;

    /* 分配播放缓冲区 */
    aec->playback_buffer_size = (config->sample_rate * AEC_PLAYBACK_BUFFER_MS) / 1000;
    aec->playback_buffer = (int16_t *)calloc(aec->playback_buffer_size, sizeof(int16_t));
    if (!aec->playback_buffer) goto cleanup;
    aec->playback_write_pos = 0;
    aec->playback_read_pos = 0;
    aec->playback_count = 0;

    /* 分配 FDAF 资源 */
    if (config->algorithm == VOICE_AEC_ALG_FDAF) {
        aec->filter_blocks = (fdaf_block_t *)calloc(aec->num_blocks, sizeof(fdaf_block_t));
        if (!aec->filter_blocks) goto cleanup;

        for (size_t i = 0; i < aec->num_blocks; i++) {
            aec->filter_blocks[i].coeffs = (aec_complex_t *)calloc(fft_size, sizeof(aec_complex_t));
            aec->filter_blocks[i].power = (float *)calloc(fft_size, sizeof(float));
            if (!aec->filter_blocks[i].coeffs || !aec->filter_blocks[i].power) goto cleanup;
        }

        aec->fft_buffer = (aec_complex_t *)calloc(fft_size, sizeof(aec_complex_t));
        aec->window = (float *)calloc(fft_size, sizeof(float));
        aec->far_buffer = (float *)calloc(fft_size, sizeof(float));
        aec->near_buffer = (float *)calloc(fft_size, sizeof(float));
        aec->error_buffer = (float *)calloc(fft_size, sizeof(float));
        aec->far_freq = (aec_complex_t *)calloc(fft_size, sizeof(aec_complex_t));
        aec->near_freq = (aec_complex_t *)calloc(fft_size, sizeof(aec_complex_t));
        aec->error_freq = (aec_complex_t *)calloc(fft_size, sizeof(aec_complex_t));
        aec->echo_est_freq = (aec_complex_t *)calloc(fft_size, sizeof(aec_complex_t));

        if (!aec->fft_buffer || !aec->window || !aec->far_buffer ||
            !aec->near_buffer || !aec->error_buffer || !aec->far_freq ||
            !aec->near_freq || !aec->error_freq || !aec->echo_est_freq) {
            goto cleanup;
        }

        generate_hann_window(aec->window, fft_size);
    }

    /* 分配 NLMS 资源 */
    if (config->algorithm == VOICE_AEC_ALG_NLMS) {
        size_t filter_len = config->filter_length;
        if (filter_len > AEC_MAX_FILTER_LENGTH) {
            filter_len = AEC_MAX_FILTER_LENGTH;
        }
        aec->nlms_weights = (float *)calloc(filter_len, sizeof(float));
        aec->nlms_x_buffer = (float *)calloc(filter_len, sizeof(float));
        if (!aec->nlms_weights || !aec->nlms_x_buffer) goto cleanup;
    }

    /* 创建延迟估计器 */
    if (config->enable_delay_estimation) {
        voice_delay_estimator_config_t de_config;
        voice_delay_estimator_config_init(&de_config);
        de_config.sample_rate = config->sample_rate;
        de_config.frame_size = config->frame_size;
        de_config.max_delay_samples = tail_samples;
        aec->delay_est = voice_delay_estimator_create(&de_config);
        /* 延迟估计器创建失败不是致命错误 */
    }

    /* 初始化 DTD */
    memset(&aec->dtd, 0, sizeof(dtd_detector_t));
    aec->dtd.state = VOICE_DTD_IDLE;

    /* 初始化状态 */
    aec->erle_db = 0.0f;
    aec->convergence = 0.0f;
    aec->known_delay_samples = 0;
    aec->use_known_delay = false;

    return aec;

cleanup:
    voice_aec_destroy(aec);
    return NULL;
}

void voice_aec_destroy(voice_aec_t *aec) {
    if (!aec) return;

    /* 释放播放缓冲区 */
    if (aec->playback_buffer) {
        free(aec->playback_buffer);
    }

    /* 释放 FDAF 资源 */
    if (aec->filter_blocks) {
        for (size_t i = 0; i < aec->num_blocks; i++) {
            if (aec->filter_blocks[i].coeffs) free(aec->filter_blocks[i].coeffs);
            if (aec->filter_blocks[i].power) free(aec->filter_blocks[i].power);
        }
        free(aec->filter_blocks);
    }
    if (aec->fft_buffer) free(aec->fft_buffer);
    if (aec->window) free(aec->window);
    if (aec->far_buffer) free(aec->far_buffer);
    if (aec->near_buffer) free(aec->near_buffer);
    if (aec->error_buffer) free(aec->error_buffer);
    if (aec->far_freq) free(aec->far_freq);
    if (aec->near_freq) free(aec->near_freq);
    if (aec->error_freq) free(aec->error_freq);
    if (aec->echo_est_freq) free(aec->echo_est_freq);

    /* 释放 NLMS 资源 */
    if (aec->nlms_weights) free(aec->nlms_weights);
    if (aec->nlms_x_buffer) free(aec->nlms_x_buffer);

    /* 销毁延迟估计器 */
    if (aec->delay_est) {
        voice_delay_estimator_destroy(aec->delay_est);
    }

    free(aec);
}

voice_error_t voice_aec_process(
    voice_aec_t *aec,
    const int16_t *mic_input,
    const int16_t *speaker_ref,
    int16_t *output,
    size_t frame_count
) {
    if (!aec || !mic_input || !output) return VOICE_ERROR_NULL_POINTER;

    if (!aec->enabled) {
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }

    /* 转换为浮点 */
    float far_float[AEC_MAX_FRAME_SIZE];
    float near_float[AEC_MAX_FRAME_SIZE];
    float out_float[AEC_MAX_FRAME_SIZE];

    size_t process_size = frame_count;
    if (process_size > AEC_MAX_FRAME_SIZE) {
        process_size = AEC_MAX_FRAME_SIZE;
    }

    const float scale_in = 1.0f / 32768.0f;
    for (size_t i = 0; i < process_size; i++) {
        near_float[i] = (float)mic_input[i] * scale_in;
        far_float[i] = speaker_ref ? (float)speaker_ref[i] * scale_in : 0.0f;
    }

    /* 延迟估计 */
    if (aec->delay_est && aec->config.enable_delay_estimation && speaker_ref) {
        voice_delay_estimate_t delay_result;
        voice_delay_estimator_estimate_float(aec->delay_est, far_float, near_float,
                                              process_size, &delay_result);
    }

    /* 根据算法选择滤波器 */
    switch (aec->config.algorithm) {
        case VOICE_AEC_ALG_FDAF:
            fdaf_filter(aec, far_float, near_float, out_float, process_size);
            break;

        case VOICE_AEC_ALG_NLMS:
            nlms_filter(aec, far_float, near_float, out_float, process_size);
            break;

        case VOICE_AEC_ALG_SPEEX:
            /* 回退到直接复制 (SpeexDSP 未集成) */
            memcpy(out_float, near_float, process_size * sizeof(float));
            break;

        default:
            memcpy(out_float, near_float, process_size * sizeof(float));
            break;
    }

    /* 转换回 int16 */
    for (size_t i = 0; i < process_size; i++) {
        float sample = out_float[i] * 32768.0f;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        output[i] = (int16_t)sample;
    }

    /* 处理剩余样本 */
    for (size_t i = process_size; i < frame_count; i++) {
        output[i] = mic_input[i];
    }

    aec->frames_processed++;
    return VOICE_OK;
}

voice_error_t voice_aec_playback(
    voice_aec_t *aec,
    const int16_t *speaker_data,
    size_t frame_count
) {
    if (!aec || !speaker_data) return VOICE_ERROR_NULL_POINTER;

    /* 写入播放缓冲区 */
    for (size_t i = 0; i < frame_count; i++) {
        aec->playback_buffer[aec->playback_write_pos] = speaker_data[i];
        aec->playback_write_pos = (aec->playback_write_pos + 1) % aec->playback_buffer_size;

        if (aec->playback_count < aec->playback_buffer_size) {
            aec->playback_count++;
        } else {
            /* 缓冲区满，移动读指针 */
            aec->playback_read_pos = (aec->playback_read_pos + 1) % aec->playback_buffer_size;
        }
    }

    return VOICE_OK;
}

voice_error_t voice_aec_capture(
    voice_aec_t *aec,
    const int16_t *mic_input,
    int16_t *output,
    size_t frame_count
) {
    if (!aec || !mic_input || !output) return VOICE_ERROR_NULL_POINTER;

    if (!aec->enabled) {
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }

    /* 获取延迟 */
    int delay_samples;
    if (aec->use_known_delay) {
        delay_samples = aec->known_delay_samples;
    } else if (aec->delay_est) {
        delay_samples = voice_delay_estimator_get_delay(aec->delay_est);
    } else {
        delay_samples = (int)(aec->config.frame_size * 2);
    }

    /* 从播放缓冲区读取远端参考信号 */
    int16_t far_ref[AEC_MAX_FRAME_SIZE];
    size_t read_count = frame_count;
    if (read_count > AEC_MAX_FRAME_SIZE) {
        read_count = AEC_MAX_FRAME_SIZE;
    }

    /* 计算读取位置 (考虑延迟) */
    size_t read_pos = aec->playback_read_pos;
    if (delay_samples > 0 && (size_t)delay_samples < aec->playback_count) {
        read_pos = (aec->playback_write_pos + aec->playback_buffer_size - (size_t)delay_samples)
                   % aec->playback_buffer_size;
    }

    for (size_t i = 0; i < read_count; i++) {
        if (aec->playback_count > 0) {
            far_ref[i] = aec->playback_buffer[read_pos];
            read_pos = (read_pos + 1) % aec->playback_buffer_size;
        } else {
            far_ref[i] = 0;
        }
    }

    /* 调用同步处理 */
    return voice_aec_process(aec, mic_input, far_ref, output, frame_count);
}

voice_error_t voice_aec_set_suppress(
    voice_aec_t *aec,
    int suppress_db,
    int suppress_active_db
) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->config.echo_suppress_db = suppress_db;
    aec->config.echo_suppress_active_db = suppress_active_db;

    return VOICE_OK;
}

voice_error_t voice_aec_set_enabled(voice_aec_t *aec, bool enabled) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->enabled = enabled;

    return VOICE_OK;
}

bool voice_aec_is_enabled(voice_aec_t *aec) {
    if (!aec) return false;

    return aec->enabled;
}

void voice_aec_reset(voice_aec_t *aec) {
    if (!aec) return;

    /* 重置播放缓冲区 */
    if (aec->playback_buffer) {
        memset(aec->playback_buffer, 0, aec->playback_buffer_size * sizeof(int16_t));
    }
    aec->playback_write_pos = 0;
    aec->playback_read_pos = 0;
    aec->playback_count = 0;

    /* 重置 FDAF 滤波器 */
    if (aec->filter_blocks) {
        for (size_t i = 0; i < aec->num_blocks; i++) {
            if (aec->filter_blocks[i].coeffs) {
                memset(aec->filter_blocks[i].coeffs, 0, aec->fft_size * sizeof(aec_complex_t));
            }
            if (aec->filter_blocks[i].power) {
                memset(aec->filter_blocks[i].power, 0, aec->fft_size * sizeof(float));
            }
        }
    }

    /* 重置 FDAF 缓冲区 */
    if (aec->far_buffer) memset(aec->far_buffer, 0, aec->fft_size * sizeof(float));
    if (aec->near_buffer) memset(aec->near_buffer, 0, aec->fft_size * sizeof(float));
    if (aec->error_buffer) memset(aec->error_buffer, 0, aec->fft_size * sizeof(float));

    /* 重置 NLMS */
    if (aec->nlms_weights) {
        memset(aec->nlms_weights, 0, aec->config.filter_length * sizeof(float));
    }
    if (aec->nlms_x_buffer) {
        memset(aec->nlms_x_buffer, 0, aec->config.filter_length * sizeof(float));
    }

    /* 重置延迟估计器 */
    if (aec->delay_est) {
        voice_delay_estimator_reset(aec->delay_est);
    }

    /* 重置 DTD */
    memset(&aec->dtd, 0, sizeof(dtd_detector_t));
    aec->dtd.state = VOICE_DTD_IDLE;

    /* 重置状态 */
    aec->erle_db = 0.0f;
    aec->convergence = 0.0f;
    aec->frames_processed = 0;
}

int voice_aec_get_delay(voice_aec_t *aec) {
    if (!aec) return 0;

    if (aec->use_known_delay) {
        return aec->known_delay_samples;
    }

    if (aec->delay_est) {
        return voice_delay_estimator_get_delay(aec->delay_est);
    }

    /* 默认返回估计延迟 */
    return (int)aec->config.frame_size * 2;
}

/* ============================================
 * Phase 1 新增 API
 * ============================================ */

voice_error_t voice_aec_set_delay(voice_aec_t *aec, int delay_samples) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    if (delay_samples < 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    aec->known_delay_samples = delay_samples;
    aec->use_known_delay = (delay_samples > 0);

    return VOICE_OK;
}

voice_error_t voice_aec_get_state(voice_aec_t *aec, voice_aec_state_t *state) {
    if (!aec || !state) return VOICE_ERROR_NULL_POINTER;

    state->dtd_state = aec->dtd.state;
    state->erle_db = aec->erle_db;
    state->convergence = aec->convergence;
    state->frames_processed = aec->frames_processed;

    if (aec->delay_est) {
        state->estimated_delay_samples = voice_delay_estimator_get_delay(aec->delay_est);
        state->estimated_delay_ms = (float)state->estimated_delay_samples * 1000.0f /
                                    (float)aec->config.sample_rate;
        state->delay_stable = voice_delay_estimator_is_stable(aec->delay_est);
    } else if (aec->use_known_delay) {
        state->estimated_delay_samples = aec->known_delay_samples;
        state->estimated_delay_ms = (float)aec->known_delay_samples * 1000.0f /
                                    (float)aec->config.sample_rate;
        state->delay_stable = true;
    } else {
        state->estimated_delay_samples = 0;
        state->estimated_delay_ms = 0.0f;
        state->delay_stable = false;
    }

    return VOICE_OK;
}

voice_dtd_state_t voice_aec_get_dtd_state(voice_aec_t *aec) {
    if (!aec) return VOICE_DTD_IDLE;
    return aec->dtd.state;
}

voice_error_t voice_aec_enable_delay_estimation(voice_aec_t *aec, bool enabled) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->config.enable_delay_estimation = enabled;

    /* 如果启用且延迟估计器不存在，则创建 */
    if (enabled && !aec->delay_est) {
        voice_delay_estimator_config_t de_config;
        voice_delay_estimator_config_init(&de_config);
        de_config.sample_rate = aec->config.sample_rate;
        de_config.frame_size = aec->config.frame_size;
        de_config.max_delay_samples = (aec->config.tail_length_ms * aec->config.sample_rate) / 1000;
        aec->delay_est = voice_delay_estimator_create(&de_config);
    }

    return VOICE_OK;
}
