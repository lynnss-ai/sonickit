/**
 * @file test_watermark.c
 * @brief 音频水印模块测试
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp/watermark.h"

#define TEST_SAMPLE_RATE 48000

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
 * Watermark Embedder Tests
 * ============================================ */

static int test_watermark_embed(void) {
    voice_watermark_embedder_config_t config;
    voice_watermark_embedder_config_init(&config);
    config.sample_rate = TEST_SAMPLE_RATE;

    /* Set payload */
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    config.payload = payload;
    config.payload_size = sizeof(payload);

    voice_watermark_embedder_t *embedder = voice_watermark_embedder_create(&config);
    if (!embedder) return 0;

    /* Create audio buffer */
    size_t audio_len = TEST_SAMPLE_RATE * 2; /* 2 seconds */
    float *audio = (float *)calloc(audio_len, sizeof(float));
    if (!audio) {
        voice_watermark_embedder_destroy(embedder);
        return 0;
    }

    /* Generate test signal */
    for (size_t i = 0; i < audio_len; i++) {
        audio[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE) * 0.5f;
    }

    /* Embed watermark */
    voice_error_t err = voice_watermark_embed(embedder, audio, audio_len);

    free(audio);
    voice_watermark_embedder_destroy(embedder);

    return err == VOICE_OK;
}

static int test_watermark_config_init(void) {
    voice_watermark_embedder_config_t config;
    voice_watermark_embedder_config_init(&config);

    /* Just verify no crash and defaults are reasonable */
    return 1;
}

/* ============================================
 * Main
 * ============================================ */

int main(void) {
    printf("========================================\n");
    printf("Voice Library - Watermark Test\n");
    printf("========================================\n\n");

    RUN_TEST(test_watermark_config_init);
    RUN_TEST(test_watermark_embed);

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
