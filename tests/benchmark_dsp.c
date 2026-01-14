/**
 * @file benchmark_dsp.c
 * @brief DSP module performance benchmarks
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 测试 DSP 模块 (AEC, Time Stretcher, Delay Estimator) 的性能
 */

#include "benchmark_common.h"
#include "dsp/echo_canceller.h"
#include "dsp/time_stretcher.h"
#include "dsp/delay_estimator.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================
 * 测试常量
 * ============================================ */

#define SAMPLE_RATE     16000
#define FRAME_SIZE      160     /* 10ms */
#define TEST_ITERATIONS 1000

/* ============================================
 * AEC 基准测试
 * ============================================ */

typedef struct {
    voice_aec_t *aec;
    int16_t *mic_in;
    int16_t *speaker_ref;
    int16_t *output;
    size_t frame_size;
} aec_bench_ctx_t;

static void bench_aec_process(void *ctx) {
    aec_bench_ctx_t *t = (aec_bench_ctx_t *)ctx;
    voice_aec_process(t->aec, t->mic_in, t->speaker_ref, t->output, t->frame_size);
}

static void run_aec_benchmark(void) {
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                     AEC Benchmark                                  \n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");

    /* 创建 AEC */
    voice_aec_ext_config_t aec_config;
    voice_aec_ext_config_init(&aec_config);
    aec_config.sample_rate = SAMPLE_RATE;
    aec_config.frame_size = FRAME_SIZE;
    aec_config.algorithm = VOICE_AEC_ALG_FDAF;

    voice_aec_t *aec = voice_aec_create(&aec_config);
    if (!aec) {
        printf("Failed to create AEC\n");
        return;
    }

    /* 分配测试数据 */
    aec_bench_ctx_t ctx;
    ctx.aec = aec;
    ctx.frame_size = FRAME_SIZE;
    ctx.mic_in = (int16_t *)malloc(FRAME_SIZE * sizeof(int16_t));
    ctx.speaker_ref = (int16_t *)malloc(FRAME_SIZE * sizeof(int16_t));
    ctx.output = (int16_t *)malloc(FRAME_SIZE * sizeof(int16_t));

    if (!ctx.mic_in || !ctx.speaker_ref || !ctx.output) {
        printf("Failed to allocate test buffers\n");
        goto cleanup_aec;
    }

    /* 生成测试信号 */
    bench_generate_sine_int16(ctx.speaker_ref, FRAME_SIZE, 440.0f, (float)SAMPLE_RATE);
    /* 麦克风信号 = 回声 + 近端语音 */
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        ctx.mic_in[i] = ctx.speaker_ref[i] / 2 +
                       (int16_t)(sinf(2.0f * 3.14159f * 1000.0f * (float)i / SAMPLE_RATE) * 8000);
    }

    /* 运行基准测试 - FDAF 算法 */
    bench_context_t bench;
    bench_init(&bench, "AEC FDAF (10ms frame)", bench_aec_process, &ctx);
    bench_set_throughput(&bench, FRAME_SIZE, "samples/sec");
    bench_set_iterations(&bench, TEST_ITERATIONS, 50);
    bench_run(&bench);
    bench_print_result(&bench);

    /* 计算实时因子 */
    double frame_duration_ns = (double)FRAME_SIZE / (double)SAMPLE_RATE * 1e9;
    double rtf = bench.stats.mean_ns / frame_duration_ns;
    printf("Real-Time Factor: %.4f (< 1.0 means real-time capable)\n\n", rtf);

    bench_cleanup(&bench);

    /* 测试 NLMS 算法 */
    voice_aec_destroy(aec);
    aec_config.algorithm = VOICE_AEC_ALG_NLMS;
    aec = voice_aec_create(&aec_config);
    ctx.aec = aec;

    bench_init(&bench, "AEC NLMS (10ms frame)", bench_aec_process, &ctx);
    bench_set_throughput(&bench, FRAME_SIZE, "samples/sec");
    bench_set_iterations(&bench, TEST_ITERATIONS, 50);
    bench_run(&bench);
    bench_print_result(&bench);

    rtf = bench.stats.mean_ns / frame_duration_ns;
    printf("Real-Time Factor: %.4f\n\n", rtf);

    bench_cleanup(&bench);

cleanup_aec:
    if (ctx.mic_in) free(ctx.mic_in);
    if (ctx.speaker_ref) free(ctx.speaker_ref);
    if (ctx.output) free(ctx.output);
    if (aec) voice_aec_destroy(aec);
}

/* ============================================
 * Time Stretcher 基准测试
 * ============================================ */

typedef struct {
    voice_time_stretcher_t *ts;
    int16_t *input;
    int16_t *output;
    size_t input_size;
    size_t output_size;
} ts_bench_ctx_t;

static void bench_time_stretch(void *ctx) {
    ts_bench_ctx_t *t = (ts_bench_ctx_t *)ctx;
    size_t out_count;
    voice_time_stretcher_process(t->ts, t->input, t->input_size, t->output, t->output_size, &out_count);
}

static void run_time_stretcher_benchmark(void) {
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                     Time Stretcher Benchmark                       \n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");

    /* 创建 Time Stretcher */
    voice_time_stretcher_config_t ts_config;
    voice_time_stretcher_config_init(&ts_config);
    ts_config.sample_rate = SAMPLE_RATE;

    voice_time_stretcher_t *ts = voice_time_stretcher_create(&ts_config);
    if (!ts) {
        printf("Failed to create Time Stretcher\n");
        return;
    }

    /* 分配测试数据 */
    ts_bench_ctx_t ctx;
    ctx.ts = ts;
    ctx.input_size = FRAME_SIZE;
    ctx.output_size = FRAME_SIZE * 2;  /* 最大输出 */
    ctx.input = (int16_t *)malloc(ctx.input_size * sizeof(int16_t));
    ctx.output = (int16_t *)malloc(ctx.output_size * sizeof(int16_t));

    if (!ctx.input || !ctx.output) {
        printf("Failed to allocate test buffers\n");
        goto cleanup_ts;
    }

    bench_generate_sine_int16(ctx.input, ctx.input_size, 440.0f, (float)SAMPLE_RATE);

    /* 测试不同的拉伸比例 */
    float rates[] = {0.5f, 0.8f, 1.0f, 1.2f, 1.5f, 2.0f};
    int num_rates = sizeof(rates) / sizeof(rates[0]);

    printf("Rate   | Mean Time (ns) | RTF      | Throughput (Ksamples/s)\n");
    printf("-------|----------------|----------|-------------------------\n");

    for (int r = 0; r < num_rates; r++) {
        voice_time_stretcher_set_rate(ts, rates[r]);
        voice_time_stretcher_reset(ts);

        bench_context_t bench;
        char name[64];
        snprintf(name, sizeof(name), "WSOLA rate=%.1f", rates[r]);

        bench_init(&bench, name, bench_time_stretch, &ctx);
        bench_set_throughput(&bench, ctx.input_size, "samples/sec");
        bench_set_iterations(&bench, 2000, 50);
        bench_run(&bench);

        double frame_duration_ns = (double)FRAME_SIZE / (double)SAMPLE_RATE * 1e9;
        double rtf = bench.stats.mean_ns / frame_duration_ns;

        printf("%.1fx   | %14.2f | %8.4f | %23.2f\n",
               rates[r],
               bench.stats.mean_ns,
               rtf,
               bench.stats.throughput / 1e3);

        bench_cleanup(&bench);
    }

    printf("\n");

cleanup_ts:
    if (ctx.input) free(ctx.input);
    if (ctx.output) free(ctx.output);
    if (ts) voice_time_stretcher_destroy(ts);
}

/* ============================================
 * Delay Estimator 基准测试
 * ============================================ */

typedef struct {
    voice_delay_estimator_t *de;
    float *far_signal;
    float *near_signal;
    size_t frame_size;
} de_bench_ctx_t;

static void bench_delay_estimate(void *ctx) {
    de_bench_ctx_t *t = (de_bench_ctx_t *)ctx;
    voice_delay_estimate_t result;
    voice_delay_estimator_estimate_float(t->de, t->far_signal, t->near_signal, t->frame_size, &result);
}

static void run_delay_estimator_benchmark(void) {
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                     Delay Estimator Benchmark                      \n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");

    /* 创建 Delay Estimator */
    voice_delay_estimator_config_t de_config;
    voice_delay_estimator_config_init(&de_config);
    de_config.sample_rate = SAMPLE_RATE;
    de_config.frame_size = FRAME_SIZE;

    voice_delay_estimator_t *de = voice_delay_estimator_create(&de_config);
    if (!de) {
        printf("Failed to create Delay Estimator\n");
        return;
    }

    /* 分配测试数据 */
    de_bench_ctx_t ctx;
    ctx.de = de;
    ctx.frame_size = FRAME_SIZE;
    ctx.far_signal = (float *)malloc(FRAME_SIZE * sizeof(float));
    ctx.near_signal = (float *)malloc(FRAME_SIZE * sizeof(float));

    if (!ctx.far_signal || !ctx.near_signal) {
        printf("Failed to allocate test buffers\n");
        goto cleanup_de;
    }

    /* 生成带延迟的信号 */
    bench_generate_sine_float(ctx.far_signal, FRAME_SIZE, 440.0f, (float)SAMPLE_RATE);
    bench_generate_sine_float(ctx.near_signal, FRAME_SIZE, 440.0f, (float)SAMPLE_RATE);

    /* 运行基准测试 */
    bench_context_t bench;
    bench_init(&bench, "GCC-PHAT Delay Estimation", bench_delay_estimate, &ctx);
    bench_set_throughput(&bench, FRAME_SIZE, "samples/sec");
    bench_set_iterations(&bench, TEST_ITERATIONS, 50);
    bench_run(&bench);
    bench_print_result(&bench);

    double frame_duration_ns = (double)FRAME_SIZE / (double)SAMPLE_RATE * 1e9;
    double rtf = bench.stats.mean_ns / frame_duration_ns;
    printf("Real-Time Factor: %.4f\n\n", rtf);

    bench_cleanup(&bench);

cleanup_de:
    if (ctx.far_signal) free(ctx.far_signal);
    if (ctx.near_signal) free(ctx.near_signal);
    if (de) voice_delay_estimator_destroy(de);
}

/* ============================================
 * 主函数
 * ============================================ */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    printf("╔════════════════════════════════════════════════════════════════╗\n");
    printf("║          SonicKit DSP Performance Benchmark                    ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");

    printf("Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("Frame Size: %d samples (%.1f ms)\n", FRAME_SIZE,
           (float)FRAME_SIZE / SAMPLE_RATE * 1000.0f);
    printf("Iterations: %d\n\n", TEST_ITERATIONS);

    run_aec_benchmark();
    run_time_stretcher_benchmark();
    run_delay_estimator_benchmark();

    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                     Benchmark Complete                             \n");
    printf("═══════════════════════════════════════════════════════════════════\n");

    return 0;
}
