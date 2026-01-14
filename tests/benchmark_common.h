/**
 * @file benchmark_common.h
 * @brief Performance benchmark framework for SonicKit
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1: 性能基准测试框架
 * - 高精度计时
 * - 统计分析 (平均值、中位数、标准差、百分位)
 * - 多次运行取稳定值
 * - 吞吐量计算
 */

#ifndef BENCHMARK_COMMON_H
#define BENCHMARK_COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#include <sys/time.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 常量定义
 * ============================================ */

#define BENCH_DEFAULT_ITERATIONS    1000
#define BENCH_WARMUP_ITERATIONS     10
#define BENCH_MAX_NAME_LEN          64
#define BENCH_MAX_RESULTS           100

/* ============================================
 * 类型定义
 * ============================================ */

/** 基准测试函数类型 */
typedef void (*bench_func_t)(void *ctx);

/** 单次测试结果 */
typedef struct {
    double elapsed_ns;      /**< 耗时 (纳秒) */
    double throughput;      /**< 吞吐量 (samples/sec 或 bytes/sec) */
} bench_sample_t;

/** 统计结果 */
typedef struct {
    char name[BENCH_MAX_NAME_LEN];
    uint32_t iterations;
    double mean_ns;         /**< 平均耗时 (ns) */
    double median_ns;       /**< 中位数耗时 (ns) */
    double min_ns;          /**< 最小耗时 (ns) */
    double max_ns;          /**< 最大耗时 (ns) */
    double stddev_ns;       /**< 标准差 (ns) */
    double p95_ns;          /**< 95th 百分位 (ns) */
    double p99_ns;          /**< 99th 百分位 (ns) */
    double throughput;      /**< 平均吞吐量 */
    const char *unit;       /**< 吞吐量单位 */
} bench_stats_t;

/** 基准测试上下文 */
typedef struct {
    const char *name;
    bench_func_t func;
    void *user_data;
    size_t data_size;       /**< 数据大小 (用于吞吐量计算) */
    const char *unit;       /**< 吞吐量单位 */
    uint32_t iterations;
    uint32_t warmup;
    bench_sample_t *samples;
    bench_stats_t stats;
} bench_context_t;

/* ============================================
 * 高精度计时器
 * ============================================ */

#ifdef _WIN32

static inline int64_t bench_get_counter(void) {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

static inline double bench_get_frequency(void) {
    static double freq = 0.0;
    if (freq == 0.0) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq = (double)f.QuadPart;
    }
    return freq;
}

static inline double bench_counter_to_ns(int64_t ticks) {
    return (double)ticks * 1e9 / bench_get_frequency();
}

#else

static inline int64_t bench_get_counter(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

static inline double bench_counter_to_ns(int64_t ticks) {
    return (double)ticks;
}

#endif

/* ============================================
 * 统计函数
 * ============================================ */

static int bench_compare_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static inline double bench_percentile(double *sorted, size_t n, double p) {
    if (n == 0) return 0.0;
    double index = p * (double)(n - 1);
    size_t lower = (size_t)index;
    size_t upper = lower + 1;
    if (upper >= n) upper = n - 1;
    double weight = index - (double)lower;
    return sorted[lower] * (1.0 - weight) + sorted[upper] * weight;
}

static inline void bench_compute_stats(bench_context_t *ctx) {
    if (ctx->iterations == 0) return;

    double *values = (double *)malloc(ctx->iterations * sizeof(double));
    if (!values) return;

    double sum = 0.0;
    double throughput_sum = 0.0;

    for (uint32_t i = 0; i < ctx->iterations; i++) {
        values[i] = ctx->samples[i].elapsed_ns;
        sum += values[i];
        throughput_sum += ctx->samples[i].throughput;
    }

    /* 排序用于百分位计算 */
    qsort(values, ctx->iterations, sizeof(double), bench_compare_double);

    /* 基本统计 */
    ctx->stats.mean_ns = sum / (double)ctx->iterations;
    ctx->stats.min_ns = values[0];
    ctx->stats.max_ns = values[ctx->iterations - 1];
    ctx->stats.median_ns = bench_percentile(values, ctx->iterations, 0.5);
    ctx->stats.p95_ns = bench_percentile(values, ctx->iterations, 0.95);
    ctx->stats.p99_ns = bench_percentile(values, ctx->iterations, 0.99);
    ctx->stats.throughput = throughput_sum / (double)ctx->iterations;
    ctx->stats.iterations = ctx->iterations;
    ctx->stats.unit = ctx->unit;

    /* 标准差 */
    double variance = 0.0;
    for (uint32_t i = 0; i < ctx->iterations; i++) {
        double diff = values[i] - ctx->stats.mean_ns;
        variance += diff * diff;
    }
    ctx->stats.stddev_ns = sqrt(variance / (double)ctx->iterations);

    /* 复制名称 */
    strncpy(ctx->stats.name, ctx->name, BENCH_MAX_NAME_LEN - 1);
    ctx->stats.name[BENCH_MAX_NAME_LEN - 1] = '\0';

    free(values);
}

/* ============================================
 * 基准测试 API
 * ============================================ */

/**
 * @brief 初始化基准测试上下文
 */
static inline void bench_init(
    bench_context_t *ctx,
    const char *name,
    bench_func_t func,
    void *user_data
) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->name = name;
    ctx->func = func;
    ctx->user_data = user_data;
    ctx->iterations = BENCH_DEFAULT_ITERATIONS;
    ctx->warmup = BENCH_WARMUP_ITERATIONS;
    ctx->unit = "ops/sec";
}

/**
 * @brief 设置吞吐量计算参数
 */
static inline void bench_set_throughput(
    bench_context_t *ctx,
    size_t data_size,
    const char *unit
) {
    ctx->data_size = data_size;
    ctx->unit = unit;
}

/**
 * @brief 设置迭代次数
 */
static inline void bench_set_iterations(
    bench_context_t *ctx,
    uint32_t iterations,
    uint32_t warmup
) {
    ctx->iterations = iterations;
    ctx->warmup = warmup;
}

/**
 * @brief 运行基准测试
 */
static inline void bench_run(bench_context_t *ctx) {
    /* 分配样本存储 */
    ctx->samples = (bench_sample_t *)calloc(ctx->iterations, sizeof(bench_sample_t));
    if (!ctx->samples) {
        fprintf(stderr, "Failed to allocate benchmark samples\n");
        return;
    }

    /* 预热 */
    for (uint32_t i = 0; i < ctx->warmup; i++) {
        ctx->func(ctx->user_data);
    }

    /* 正式测试 */
    for (uint32_t i = 0; i < ctx->iterations; i++) {
        int64_t start = bench_get_counter();
        ctx->func(ctx->user_data);
        int64_t end = bench_get_counter();

        double elapsed_ns = bench_counter_to_ns(end - start);
        ctx->samples[i].elapsed_ns = elapsed_ns;

        /* 计算吞吐量 */
        if (ctx->data_size > 0 && elapsed_ns > 0) {
            ctx->samples[i].throughput = (double)ctx->data_size / (elapsed_ns * 1e-9);
        } else if (elapsed_ns > 0) {
            ctx->samples[i].throughput = 1e9 / elapsed_ns;
        }
    }

    /* 计算统计 */
    bench_compute_stats(ctx);
}

/**
 * @brief 打印结果
 */
static inline void bench_print_result(const bench_context_t *ctx) {
    const bench_stats_t *s = &ctx->stats;

    printf("+------------------------------------------------------------------+\n");
    printf("| Benchmark: %-52s |\n", s->name);
    printf("+------------------------------------------------------------------+\n");
    printf("| Iterations: %-10u                                        |\n", s->iterations);
    printf("| Mean:       %10.2f ns  (%.2f us)                         |\n",
           s->mean_ns, s->mean_ns / 1000.0);
    printf("| Median:     %10.2f ns                                    |\n", s->median_ns);
    printf("| Std Dev:    %10.2f ns                                    |\n", s->stddev_ns);
    printf("| Min:        %10.2f ns                                    |\n", s->min_ns);
    printf("| Max:        %10.2f ns                                    |\n", s->max_ns);
    printf("| P95:        %10.2f ns                                    |\n", s->p95_ns);
    printf("| P99:        %10.2f ns                                    |\n", s->p99_ns);

    if (s->throughput > 0) {
        if (s->throughput > 1e9) {
            printf("| Throughput: %10.2f G%s                                 |\n",
                   s->throughput / 1e9, s->unit);
        } else if (s->throughput > 1e6) {
            printf("| Throughput: %10.2f M%s                                 |\n",
                   s->throughput / 1e6, s->unit);
        } else if (s->throughput > 1e3) {
            printf("| Throughput: %10.2f K%s                                 |\n",
                   s->throughput / 1e3, s->unit);
        } else {
            printf("| Throughput: %10.2f %s                                  |\n",
                   s->throughput, s->unit);
        }
    }

    printf("+------------------------------------------------------------------+\n\n");
}

/**
 * @brief 清理基准测试上下文
 */
static inline void bench_cleanup(bench_context_t *ctx) {
    if (ctx->samples) {
        free(ctx->samples);
        ctx->samples = NULL;
    }
}

/**
 * @brief 简化的基准测试宏
 */
#define BENCH_RUN(name, func, user_data, data_size, unit) \
    do { \
        bench_context_t _ctx; \
        bench_init(&_ctx, name, func, user_data); \
        bench_set_throughput(&_ctx, data_size, unit); \
        bench_run(&_ctx); \
        bench_print_result(&_ctx); \
        bench_cleanup(&_ctx); \
    } while (0)

/**
 * @brief 比较两个基准测试结果
 */
static inline void bench_compare(
    const bench_stats_t *baseline,
    const bench_stats_t *test,
    const char *label
) {
    double speedup = baseline->mean_ns / test->mean_ns;
    double diff_percent = ((baseline->mean_ns - test->mean_ns) / baseline->mean_ns) * 100.0;

    printf("+------------------------------------------------------------------+\n");
    printf("| Comparison: %-50s |\n", label);
    printf("+------------------------------------------------------------------+\n");
    printf("| Baseline:   %-52s |\n", baseline->name);
    printf("|             %10.2f ns                                      |\n", baseline->mean_ns);
    printf("| Test:       %-52s |\n", test->name);
    printf("|             %10.2f ns                                      |\n", test->mean_ns);
    printf("| Speedup:    %10.2fx                                        |\n", speedup);
    printf("| Difference: %+9.1f%%                                        |\n", diff_percent);
    printf("+------------------------------------------------------------------+\n\n");
}

/* ============================================
 * 测试数据生成
 * ============================================ */

/**
 * @brief 生成随机 int16 音频数据
 */
static inline void bench_generate_int16(int16_t *buf, size_t count, int seed) {
    srand(seed);
    for (size_t i = 0; i < count; i++) {
        buf[i] = (int16_t)(rand() % 65536 - 32768);
    }
}

/**
 * @brief 生成随机 float 音频数据 [-1, 1]
 */
static inline void bench_generate_float(float *buf, size_t count, int seed) {
    srand(seed);
    for (size_t i = 0; i < count; i++) {
        buf[i] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    }
}

/**
 * @brief 生成正弦波 int16
 */
static inline void bench_generate_sine_int16(
    int16_t *buf,
    size_t count,
    float freq,
    float sample_rate
) {
    for (size_t i = 0; i < count; i++) {
        float t = (float)i / sample_rate;
        float sample = sinf(2.0f * 3.14159265f * freq * t);
        buf[i] = (int16_t)(sample * 32767.0f);
    }
}

/**
 * @brief 生成正弦波 float
 */
static inline void bench_generate_sine_float(
    float *buf,
    size_t count,
    float freq,
    float sample_rate
) {
    for (size_t i = 0; i < count; i++) {
        float t = (float)i / sample_rate;
        buf[i] = sinf(2.0f * 3.14159265f * freq * t);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* BENCHMARK_COMMON_H */
