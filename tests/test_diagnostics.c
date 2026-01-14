/**
 * @file test_diagnostics.c
 * @brief 诊断模块测试
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "utils/diagnostics.h"

#define TEST_SAMPLE_RATE 48000
#define TEST_FRAME_SIZE 480

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(test_func) do { \
    tests_run++; \
    printf("Testing: %s... ", #test_func); \
    if (test_func()) { \
        printf("PASSED\n"); \
        tests_passed++; \
    } else { \
        printf("FAILED\n"); \
    } \
} while(0)

/* ============================================
 * Quality Monitor Tests
 * ============================================ */

static int test_quality_monitor(void) {
    voice_quality_monitor_config_t config;
    voice_quality_monitor_config_init(&config);
    config.sample_rate = TEST_SAMPLE_RATE;
    config.frame_size = TEST_FRAME_SIZE;

    voice_quality_monitor_t *monitor = voice_quality_monitor_create(&config);
    if (!monitor) return 0;

    /* Generate some test samples (int16) */
    int16_t samples[TEST_FRAME_SIZE];
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = (int16_t)(sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE) * 16000);
    }

    voice_error_t err = voice_quality_monitor_process(monitor, samples, TEST_FRAME_SIZE);

    voice_quality_metrics_t metrics;
    if (err == VOICE_OK) {
        err = voice_quality_monitor_get_metrics(monitor, &metrics);
    }

    voice_quality_monitor_destroy(monitor);

    return err == VOICE_OK;
}

static int test_quality_monitor_float(void) {
    voice_quality_monitor_config_t config;
    voice_quality_monitor_config_init(&config);
    config.sample_rate = TEST_SAMPLE_RATE;
    config.frame_size = TEST_FRAME_SIZE;

    voice_quality_monitor_t *monitor = voice_quality_monitor_create(&config);
    if (!monitor) return 0;

    /* Generate some test samples (float) */
    float samples[TEST_FRAME_SIZE];
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }

    voice_error_t err = voice_quality_monitor_process_float(monitor, samples, TEST_FRAME_SIZE);

    voice_quality_metrics_t metrics;
    if (err == VOICE_OK) {
        err = voice_quality_monitor_get_metrics(monitor, &metrics);
    }

    voice_quality_monitor_destroy(monitor);

    return err == VOICE_OK;
}

static int test_quality_config_init(void) {
    voice_quality_monitor_config_t config;
    voice_quality_monitor_config_init(&config);

    /* Just verify no crash */
    return 1;
}

/* ============================================
 * Main
 * ============================================ */

int main(void) {
    printf("========================================\n");
    printf("Voice Library - Diagnostics Test\n");
    printf("========================================\n\n");

    RUN_TEST(test_quality_config_init);
    RUN_TEST(test_quality_monitor);
    RUN_TEST(test_quality_monitor_float);

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
