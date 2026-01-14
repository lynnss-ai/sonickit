/**
 * @file test_effects.c
 * @brief 音频效果模块测试
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp/effects.h"

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
 * Reverb Tests
 * ============================================ */

static int test_reverb(void) {
    voice_reverb_config_t config;
    voice_reverb_config_init(&config);

    voice_reverb_t *reverb = voice_reverb_create(&config);
    if (!reverb) return 0;

    float samples[TEST_FRAME_SIZE];

    /* Generate test signal */
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }

    voice_error_t err = voice_reverb_process(reverb, samples, TEST_FRAME_SIZE);
    voice_reverb_destroy(reverb);

    return err == VOICE_OK;
}

/* ============================================
 * Delay Tests
 * ============================================ */

static int test_delay(void) {
    voice_delay_config_t config;
    voice_delay_config_init(&config);
    config.delay_ms = 100.0f;
    config.sample_rate = TEST_SAMPLE_RATE;

    voice_delay_t *delay = voice_delay_create(&config);
    if (!delay) return 0;

    float samples[TEST_FRAME_SIZE];

    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = (i == 0) ? 1.0f : 0.0f; /* Impulse */
    }

    voice_error_t err = voice_delay_process(delay, samples, TEST_FRAME_SIZE);
    voice_delay_destroy(delay);

    return err == VOICE_OK;
}

/* ============================================
 * Pitch Shift Tests
 * ============================================ */

static int test_pitch_shift(void) {
    voice_pitch_shift_config_t config;
    voice_pitch_shift_config_init(&config);
    config.semitones = 5.0f; /* Up by 5 semitones */
    config.sample_rate = TEST_SAMPLE_RATE;

    voice_pitch_shift_t *pitch = voice_pitch_shift_create(&config);
    if (!pitch) return 0;

    float samples[TEST_FRAME_SIZE];

    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }

    voice_error_t err = voice_pitch_shift_process(pitch, samples, TEST_FRAME_SIZE);
    voice_pitch_shift_destroy(pitch);

    return err == VOICE_OK;
}

/* ============================================
 * Effects Chain Tests
 * ============================================ */

static int test_effects_chain(void) {
    voice_effect_chain_t *chain = voice_effect_chain_create(TEST_SAMPLE_RATE);
    if (!chain) return 0;

    /* Add reverb */
    voice_reverb_config_t rev_config;
    voice_reverb_config_init(&rev_config);
    voice_error_t err = voice_effect_chain_add(chain, VOICE_EFFECT_REVERB, &rev_config);
    if (err != VOICE_OK) {
        voice_effect_chain_destroy(chain);
        return 0;
    }

    float samples[TEST_FRAME_SIZE];

    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }

    err = voice_effect_chain_process(chain, samples, TEST_FRAME_SIZE);
    voice_effect_chain_destroy(chain);

    return err == VOICE_OK;
}

/* ============================================
 * Main
 * ============================================ */

int main(void) {
    printf("========================================\n");
    printf("Voice Library - Audio Effects Test\n");
    printf("========================================\n\n");

    RUN_TEST(test_reverb);
    RUN_TEST(test_delay);
    RUN_TEST(test_pitch_shift);
    RUN_TEST(test_effects_chain);

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
