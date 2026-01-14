/**
 * @file test_sip.c
 * @brief SIP协议模块测试
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sip/sip_core.h"
#include "sip/sip_ua.h"

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
 * SIP URI Tests
 * ============================================ */

static int test_sip_uri_parse(void) {
    sip_uri_t uri;
    memset(&uri, 0, sizeof(uri));

    voice_error_t err = sip_uri_parse("sip:alice@example.com:5060", &uri);
    if (err != VOICE_OK) return 0;

    int result = (strcmp(uri.user, "alice") == 0) &&
                 (strcmp(uri.host, "example.com") == 0) &&
                 (uri.port == 5060);

    return result;
}

static int test_sip_uri_parse_simple(void) {
    sip_uri_t uri;
    memset(&uri, 0, sizeof(uri));

    voice_error_t err = sip_uri_parse("sip:bob@test.org", &uri);
    if (err != VOICE_OK) return 0;

    int result = (strcmp(uri.user, "bob") == 0) &&
                 (strcmp(uri.host, "test.org") == 0);

    return result;
}

/* ============================================
 * SIP Message Tests
 * ============================================ */

static int test_sip_message_create_invite(void) {
    sip_message_t *msg = sip_message_create();
    if (!msg) return 0;

    sip_uri_t to_uri = {0};
    to_uri.scheme = SIP_URI_SCHEME_SIP;
    strncpy(to_uri.user, "bob", sizeof(to_uri.user) - 1);
    strncpy(to_uri.host, "example.com", sizeof(to_uri.host) - 1);
    to_uri.port = 5060;

    sip_uri_t from_uri = {0};
    from_uri.scheme = SIP_URI_SCHEME_SIP;
    strncpy(from_uri.user, "alice", sizeof(from_uri.user) - 1);
    strncpy(from_uri.host, "example.com", sizeof(from_uri.host) - 1);
    from_uri.port = 5060;

    voice_error_t err = sip_message_create_invite(msg, &to_uri, &from_uri,
                                                   "call-id-123", 1, NULL);

    int result = (err == VOICE_OK) && (msg->method == SIP_METHOD_INVITE);

    sip_message_destroy(msg);

    return result;
}

/* ============================================
 * SIP UA Config Tests
 * ============================================ */

static int test_sip_ua_config(void) {
    sip_ua_config_t config;
    sip_ua_config_init(&config);

    config.local_port = 5060;
    strncpy(config.username, "testuser", sizeof(config.username) - 1);
    strncpy(config.domain, "example.com", sizeof(config.domain) - 1);

    return (config.local_port == 5060);
}

/* ============================================
 * SIP Helper Tests
 * ============================================ */

static int test_sip_method_to_string(void) {
    const char *invite = sip_method_to_string(SIP_METHOD_INVITE);
    const char *bye = sip_method_to_string(SIP_METHOD_BYE);
    const char *ack = sip_method_to_string(SIP_METHOD_ACK);

    return (invite != NULL) && (bye != NULL) && (ack != NULL);
}

static int test_sip_method_from_string(void) {
    sip_method_t invite = sip_method_from_string("INVITE");
    sip_method_t bye = sip_method_from_string("BYE");

    return (invite == SIP_METHOD_INVITE) && (bye == SIP_METHOD_BYE);
}

/* ============================================
 * Main
 * ============================================ */

int main(void) {
    printf("========================================\n");
    printf("Voice Library - SIP Protocol Test\n");
    printf("========================================\n\n");

    RUN_TEST(test_sip_uri_parse);
    RUN_TEST(test_sip_uri_parse_simple);
    RUN_TEST(test_sip_message_create_invite);
    RUN_TEST(test_sip_ua_config);
    RUN_TEST(test_sip_method_to_string);
    RUN_TEST(test_sip_method_from_string);

    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
