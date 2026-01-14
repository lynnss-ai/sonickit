/**
 * @file test_datachannel.c
 * @brief 数据通道模块测试
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "network/datachannel.h"

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
 * DataChannel Connection Config Tests
 * ============================================ */

static int test_connection_config(void) {
    datachannel_connection_config_t config;
    datachannel_connection_config_init(&config);

    /* Verify defaults are set */
    int result = (config.heartbeat_interval_ms > 0);

    return result;
}

/* ============================================
 * DataChannel Config Tests
 * ============================================ */

static int test_channel_config(void) {
    datachannel_config_t config;
    datachannel_config_init(&config);

    /* Verify default type is reliable */
    return (config.type == DATACHANNEL_RELIABLE);
}

/* ============================================
 * DataChannel State String Tests
 * ============================================ */

static int test_state_to_string(void) {
    const char *s1 = datachannel_state_to_string(DATACHANNEL_STATE_OPEN);
    const char *s2 = datachannel_state_to_string(DATACHANNEL_STATE_CLOSED);
    const char *s3 = datachannel_state_to_string(DATACHANNEL_STATE_CONNECTING);

    /* Just verify we get valid strings */
    return (s1 != NULL) && (s2 != NULL) && (s3 != NULL);
}

static int test_type_to_string(void) {
    const char *s1 = datachannel_type_to_string(DATACHANNEL_RELIABLE);
    const char *s2 = datachannel_type_to_string(DATACHANNEL_UNRELIABLE);

    /* Just verify we get valid strings */
    return (s1 != NULL) && (s2 != NULL);
}

/* ============================================
 * Main
 * ============================================ */

int main(void) {
    printf("========================================\n");
    printf("Voice Library - DataChannel Test\n");
    printf("========================================\n\n");

    RUN_TEST(test_connection_config);
    RUN_TEST(test_channel_config);
    RUN_TEST(test_state_to_string);
    RUN_TEST(test_type_to_string);

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
