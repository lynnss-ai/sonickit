/**
 * @file benchmark_simd.c
 * @brief SIMD performance benchmarks
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 测试 SIMD 优化函数的性能提升
 */

#include "benchmark_common.h"
#include "utils/simd_utils.h"
#include <stdio.h>
#include <stdlib.h>

/* ============================================
 * 测试数据
 * ============================================ */

#define TEST_SIZE       4096
#define DEFAULT_ITERATIONS 10000

static int g_iterations = DEFAULT_ITERATIONS;

/* ============================================
 * 命令行帮助
 * ============================================ */

static void print_usage(const char *prog_name) {
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("SIMD Performance Benchmark for SonicKit\n\n");
    printf("Options:\n");
    printf("  -n, --iterations N   Number of iterations (default: %d)\n", DEFAULT_ITERATIONS);
    printf("  -h, --help           Show this help message\n");
    printf("\nExample:\n");
    printf("  %s -n 50000          Run with 50000 iterations\n", prog_name);
}

static int parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return -1;  /* Signal to exit */
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--iterations") == 0) {
            if (i + 1 < argc) {
                g_iterations = atoi(argv[++i]);
                if (g_iterations <= 0) {
                    fprintf(stderr, "Error: iterations must be positive\n");
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: --iterations requires a value\n");
                return 1;
            }
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    return 0;
}

typedef struct {
    int16_t *int16_buf;
    float *float_buf_src;
    float *float_buf_dst;
    size_t size;
} simd_test_ctx_t;

/* ============================================
 * 基准测试函数
 * ============================================ */

static void bench_int16_to_float(void *ctx) {
    simd_test_ctx_t *t = (simd_test_ctx_t *)ctx;
    voice_int16_to_float(t->int16_buf, t->float_buf_dst, t->size);
}

static void bench_float_to_int16(void *ctx) {
    simd_test_ctx_t *t = (simd_test_ctx_t *)ctx;
    voice_float_to_int16(t->float_buf_src, t->int16_buf, t->size);
}

static void bench_apply_gain(void *ctx) {
    simd_test_ctx_t *t = (simd_test_ctx_t *)ctx;
    voice_apply_gain_float(t->float_buf_dst, t->size, 0.5f);
}

static void bench_mix_add(void *ctx) {
    simd_test_ctx_t *t = (simd_test_ctx_t *)ctx;
    voice_mix_add_float(t->float_buf_dst, t->float_buf_src, t->size);
}

static void bench_find_peak(void *ctx) {
    simd_test_ctx_t *t = (simd_test_ctx_t *)ctx;
    volatile float peak = voice_find_peak_float(t->float_buf_src, t->size);
    (void)peak;
}

static void bench_compute_energy(void *ctx) {
    simd_test_ctx_t *t = (simd_test_ctx_t *)ctx;
    volatile float energy = voice_compute_energy_float(t->float_buf_src, t->size);
    (void)energy;
}

/* ============================================
 * 主函数
 * ============================================ */

int main(int argc, char *argv[]) {
    int ret = parse_args(argc, argv);
    if (ret != 0) {
        return ret < 0 ? 0 : ret;  /* -1 means help, exit with 0 */
    }

    printf("+================================================================+\n");
    printf("|          SonicKit SIMD Performance Benchmark                   |\n");
    printf("+================================================================+\n\n");

    /* 检测 SIMD 能力 */
    uint32_t simd_flags = voice_simd_detect();
    printf("SIMD Capabilities: %s\n", voice_simd_get_description());
    printf("Test buffer size: %d samples\n", TEST_SIZE);
    printf("Iterations: %d\n\n", g_iterations);

    /* 分配测试数据 */
    simd_test_ctx_t ctx;
    ctx.size = TEST_SIZE;
    ctx.int16_buf = (int16_t *)voice_aligned_alloc(TEST_SIZE * sizeof(int16_t), 64);
    ctx.float_buf_src = (float *)voice_aligned_alloc(TEST_SIZE * sizeof(float), 64);
    ctx.float_buf_dst = (float *)voice_aligned_alloc(TEST_SIZE * sizeof(float), 64);

    if (!ctx.int16_buf || !ctx.float_buf_src || !ctx.float_buf_dst) {
        fprintf(stderr, "Failed to allocate test buffers\n");
        return 1;
    }

    /* 生成测试数据 */
    bench_generate_int16(ctx.int16_buf, TEST_SIZE, 42);
    bench_generate_float(ctx.float_buf_src, TEST_SIZE, 42);
    memcpy(ctx.float_buf_dst, ctx.float_buf_src, TEST_SIZE * sizeof(float));

    /* 运行基准测试 */
    bench_context_t bench;

    printf("===================================================================\n");
    printf("                     Format Conversion Tests                        \n");
    printf("===================================================================\n\n");

    bench_init(&bench, "int16_to_float", bench_int16_to_float, &ctx);
    bench_set_throughput(&bench, TEST_SIZE, "samples/sec");
    bench_set_iterations(&bench, g_iterations, 100);
    bench_run(&bench);
    bench_print_result(&bench);
    bench_cleanup(&bench);

    bench_init(&bench, "float_to_int16", bench_float_to_int16, &ctx);
    bench_set_throughput(&bench, TEST_SIZE, "samples/sec");
    bench_set_iterations(&bench, g_iterations, 100);
    bench_run(&bench);
    bench_print_result(&bench);
    bench_cleanup(&bench);

    printf("===================================================================\n");
    printf("                     Audio Processing Tests                         \n");
    printf("===================================================================\n\n");

    /* 重置数据 */
    memcpy(ctx.float_buf_dst, ctx.float_buf_src, TEST_SIZE * sizeof(float));

    bench_init(&bench, "apply_gain_float", bench_apply_gain, &ctx);
    bench_set_throughput(&bench, TEST_SIZE, "samples/sec");
    bench_set_iterations(&bench, g_iterations, 100);
    bench_run(&bench);
    bench_print_result(&bench);
    bench_cleanup(&bench);

    bench_init(&bench, "mix_add_float", bench_mix_add, &ctx);
    bench_set_throughput(&bench, TEST_SIZE, "samples/sec");
    bench_set_iterations(&bench, g_iterations, 100);
    bench_run(&bench);
    bench_print_result(&bench);
    bench_cleanup(&bench);

    printf("===================================================================\n");
    printf("                     Analysis Tests                                 \n");
    printf("===================================================================\n\n");

    bench_init(&bench, "find_peak_float", bench_find_peak, &ctx);
    bench_set_throughput(&bench, TEST_SIZE, "samples/sec");
    bench_set_iterations(&bench, g_iterations, 100);
    bench_run(&bench);
    bench_print_result(&bench);
    bench_cleanup(&bench);

    bench_init(&bench, "compute_energy_float", bench_compute_energy, &ctx);
    bench_set_throughput(&bench, TEST_SIZE, "samples/sec");
    bench_set_iterations(&bench, g_iterations, 100);
    bench_run(&bench);
    bench_print_result(&bench);
    bench_cleanup(&bench);

    /* 不同缓冲区大小测试 */
    printf("===================================================================\n");
    printf("                     Buffer Size Scaling Test                       \n");
    printf("===================================================================\n\n");

    size_t sizes[] = {64, 256, 1024, 4096, 16384};
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("Buffer Size | int16_to_float (ns) | Throughput (Msamples/s)\n");
    printf("------------|---------------------|-------------------------\n");

    for (int s = 0; s < num_sizes; s++) {
        size_t test_size = sizes[s];

        int16_t *test_int16 = (int16_t *)voice_aligned_alloc(test_size * sizeof(int16_t), 64);
        float *test_float = (float *)voice_aligned_alloc(test_size * sizeof(float), 64);

        if (!test_int16 || !test_float) continue;

        bench_generate_int16(test_int16, test_size, 42);

        simd_test_ctx_t size_ctx = { test_int16, test_float, test_float, test_size };

        bench_init(&bench, "scale_test", bench_int16_to_float, &size_ctx);
        bench_set_throughput(&bench, test_size, "samples/sec");
        bench_set_iterations(&bench, 5000, 100);
        bench_run(&bench);

        printf("%11zu | %19.2f | %23.2f\n",
               test_size,
               bench.stats.mean_ns,
               bench.stats.throughput / 1e6);

        bench_cleanup(&bench);
        voice_aligned_free(test_int16);
        voice_aligned_free(test_float);
    }

    printf("\n");

    /* 清理 */
    voice_aligned_free(ctx.int16_buf);
    voice_aligned_free(ctx.float_buf_src);
    voice_aligned_free(ctx.float_buf_dst);

    printf("===================================================================\n");
    printf("                     Benchmark Complete                             \n");
    printf("===================================================================\n");

    return 0;
}
