/**
 * @file test_common.h
 * @brief Common test framework utilities
 */

#ifndef TEST_COMMON_H
#define TEST_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================
 * Test framework macros
 * ============================================ */

#define TEST_PASSED 0
#define TEST_FAILED 1

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s\n", msg); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        return TEST_FAILED; \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b, msg) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s (expected %d, got %d)\n", msg, (int)(b), (int)(a)); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        return TEST_FAILED; \
    } \
} while(0)

#define TEST_ASSERT_FLOAT_EQ(a, b, eps, msg) do { \
    if (fabs((double)(a) - (double)(b)) > (eps)) { \
        printf("  FAIL: %s (expected %.6f, got %.6f)\n", msg, (double)(b), (double)(a)); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        return TEST_FAILED; \
    } \
} while(0)

#define TEST_ASSERT_NOT_NULL(ptr, msg) do { \
    if ((ptr) == NULL) { \
        printf("  FAIL: %s (got NULL)\n", msg); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        return TEST_FAILED; \
    } \
} while(0)

#define TEST_ASSERT_NULL(ptr, msg) do { \
    if ((ptr) != NULL) { \
        printf("  FAIL: %s (expected NULL)\n", msg); \
        printf("        at %s:%d\n", __FILE__, __LINE__); \
        return TEST_FAILED; \
    } \
} while(0)

#define RUN_TEST(test_func) do { \
    g_tests_run++; \
    printf("Running: %s...\n", #test_func); \
    if (test_func() == TEST_PASSED) { \
        g_tests_passed++; \
        printf("  PASS\n"); \
    } else { \
        g_tests_failed++; \
    } \
} while(0)

#define TEST_SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Test Results: %d/%d passed", g_tests_passed, g_tests_run); \
    if (g_tests_failed > 0) { \
        printf(" (%d failed)", g_tests_failed); \
    } \
    printf("\n========================================\n"); \
    return g_tests_failed > 0 ? 1 : 0; \
} while(0)

/* ============================================
 * Test data generation
 * ============================================ */

/** Generate sine wave test signal */
static inline void generate_sine_wave(int16_t *buffer, size_t samples, 
                                       float frequency, uint32_t sample_rate)
{
    for (size_t i = 0; i < samples; i++) {
        double t = (double)i / sample_rate;
        double value = sin(2.0 * 3.14159265358979323846 * frequency * t);
        buffer[i] = (int16_t)(value * 32767.0);
    }
}

/** Generate silence */
static inline void generate_silence(int16_t *buffer, size_t samples)
{
    memset(buffer, 0, samples * sizeof(int16_t));
}

/** Generate white noise */
static inline void generate_noise(int16_t *buffer, size_t samples, int16_t amplitude)
{
    for (size_t i = 0; i < samples; i++) {
        buffer[i] = (int16_t)((rand() % (2 * amplitude)) - amplitude);
    }
}

/** Calculate RMS of signal */
static inline float calculate_rms(const int16_t *buffer, size_t samples)
{
    if (samples == 0) return 0.0f;
    
    double sum = 0.0;
    for (size_t i = 0; i < samples; i++) {
        sum += (double)buffer[i] * buffer[i];
    }
    return (float)sqrt(sum / samples);
}

/** Compare two buffers */
static inline int compare_buffers(const int16_t *a, const int16_t *b, 
                                  size_t samples, int16_t tolerance)
{
    for (size_t i = 0; i < samples; i++) {
        int diff = abs(a[i] - b[i]);
        if (diff > tolerance) {
            return 0;
        }
    }
    return 1;
}

#endif /* TEST_COMMON_H */
