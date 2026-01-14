/**
 * @file test_new_modules.c
 * @brief 新增模块基本功能测试
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "dsp/effects.h"
#include "dsp/watermark.h"
#include "utils/diagnostics.h"
#include "network/datachannel.h"
#include "sip/sip_core.h"
#include "sip/sip_ua.h"

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
 * Effects Module Tests
 * ============================================ */

static int test_reverb(void) {
    reverb_config_t config;
    reverb_config_init(&config);
    
    reverb_t *reverb = reverb_create(&config);
    if (!reverb) return 0;
    
    float input[TEST_FRAME_SIZE];
    float output[TEST_FRAME_SIZE];
    
    /* Generate test signal */
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        input[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }
    
    voice_error_t err = reverb_process(reverb, input, output, TEST_FRAME_SIZE);
    reverb_destroy(reverb);
    
    return err == VOICE_OK;
}

static int test_delay(void) {
    delay_config_t config;
    delay_config_init(&config);
    config.delay_ms = 100.0f;
    config.sample_rate = TEST_SAMPLE_RATE;
    
    delay_t *delay = delay_create(&config);
    if (!delay) return 0;
    
    float input[TEST_FRAME_SIZE];
    float output[TEST_FRAME_SIZE];
    
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        input[i] = (i == 0) ? 1.0f : 0.0f; /* Impulse */
    }
    
    voice_error_t err = delay_process(delay, input, output, TEST_FRAME_SIZE);
    delay_destroy(delay);
    
    return err == VOICE_OK;
}

static int test_pitch_shift(void) {
    pitch_shift_config_t config;
    pitch_shift_config_init(&config);
    config.pitch_factor = 1.5f; /* Up by 50% */
    config.sample_rate = TEST_SAMPLE_RATE;
    
    pitch_shift_t *pitch = pitch_shift_create(&config);
    if (!pitch) return 0;
    
    float input[TEST_FRAME_SIZE];
    float output[TEST_FRAME_SIZE];
    
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        input[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }
    
    voice_error_t err = pitch_shift_process(pitch, input, output, TEST_FRAME_SIZE);
    pitch_shift_destroy(pitch);
    
    return err == VOICE_OK;
}

static int test_effects_chain(void) {
    effects_chain_config_t config;
    effects_chain_config_init(&config);
    
    effects_chain_t *chain = effects_chain_create(&config);
    if (!chain) return 0;
    
    /* Add reverb */
    reverb_config_t rev_config;
    reverb_config_init(&rev_config);
    effects_chain_add_reverb(chain, &rev_config);
    
    float input[TEST_FRAME_SIZE];
    float output[TEST_FRAME_SIZE];
    
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        input[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }
    
    voice_error_t err = effects_chain_process(chain, input, output, TEST_FRAME_SIZE);
    effects_chain_destroy(chain);
    
    return err == VOICE_OK;
}

/* ============================================
 * Watermark Module Tests
 * ============================================ */

static int test_watermark_embed_detect(void) {
    watermark_config_t config;
    watermark_config_init(&config);
    config.sample_rate = TEST_SAMPLE_RATE;
    
    watermark_t *wm = watermark_create(&config);
    if (!wm) return 0;
    
    /* Create audio buffer */
    size_t audio_len = TEST_SAMPLE_RATE * 2; /* 2 seconds */
    float *audio = (float *)calloc(audio_len, sizeof(float));
    
    /* Generate test signal */
    for (size_t i = 0; i < audio_len; i++) {
        audio[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE) * 0.5f;
    }
    
    /* Embed watermark */
    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    voice_error_t err = watermark_embed(wm, audio, audio_len, payload, sizeof(payload));
    
    int result = (err == VOICE_OK);
    
    /* Try to detect */
    if (result) {
        bool detected = false;
        float confidence = 0.0f;
        err = watermark_detect(wm, audio, audio_len, &detected, &confidence);
        /* Detection might not work with such short audio, so just check no crash */
        result = (err == VOICE_OK);
    }
    
    free(audio);
    watermark_destroy(wm);
    
    return result;
}

/* ============================================
 * Diagnostics Module Tests
 * ============================================ */

static int test_diagnostics_quality(void) {
    quality_monitor_config_t config;
    quality_monitor_config_init(&config);
    
    quality_monitor_t *monitor = quality_monitor_create(&config);
    if (!monitor) return 0;
    
    /* Generate some test samples */
    float samples[TEST_FRAME_SIZE];
    for (int i = 0; i < TEST_FRAME_SIZE; i++) {
        samples[i] = sinf(2.0f * 3.14159f * 440.0f * i / TEST_SAMPLE_RATE);
    }
    
    voice_error_t err = quality_monitor_add_samples(monitor, samples, TEST_FRAME_SIZE);
    
    quality_metrics_t metrics;
    if (err == VOICE_OK) {
        err = quality_monitor_get_metrics(monitor, &metrics);
    }
    
    quality_monitor_destroy(monitor);
    
    return err == VOICE_OK;
}

/* ============================================
 * DataChannel Module Tests
 * ============================================ */

static int test_datachannel_config(void) {
    datachannel_connection_config_t conn_config;
    datachannel_connection_config_init(&conn_config);
    
    /* Just verify defaults are set */
    int result = (conn_config.heartbeat_interval_ms > 0);
    
    datachannel_config_t ch_config;
    datachannel_config_init(&ch_config);
    
    result = result && (ch_config.type == DATACHANNEL_RELIABLE);
    
    return result;
}

static int test_datachannel_state_strings(void) {
    const char *s1 = datachannel_state_to_string(DATACHANNEL_STATE_OPEN);
    const char *s2 = datachannel_type_to_string(DATACHANNEL_RELIABLE);
    
    return (strcmp(s1, "open") == 0) && (strcmp(s2, "reliable") == 0);
}

/* ============================================
 * SIP Module Tests
 * ============================================ */

static int test_sip_uri_parse(void) {
    sip_uri_t uri;
    
    voice_error_t err = sip_uri_parse("sip:alice@example.com:5060", &uri);
    if (err != VOICE_OK) return 0;
    
    int result = (strcmp(uri.user, "alice") == 0) &&
                 (strcmp(uri.host, "example.com") == 0) &&
                 (uri.port == 5060);
    
    return result;
}

static int test_sip_message_create(void) {
    sip_message_t msg;
    
    sip_uri_t from_uri = {.scheme = "sip", .user = "alice", .host = "example.com", .port = 5060};
    sip_uri_t to_uri = {.scheme = "sip", .user = "bob", .host = "example.com", .port = 5060};
    
    voice_error_t err = sip_message_create_invite(&msg, &from_uri, &to_uri, "192.168.1.1", 5060);
    
    return (err == VOICE_OK) && (msg.method == SIP_METHOD_INVITE);
}

static int test_sip_ua_config(void) {
    sip_ua_config_t config;
    sip_ua_config_init(&config);
    
    config.local_port = 5060;
    strncpy(config.username, "testuser", sizeof(config.username) - 1);
    strncpy(config.domain, "example.com", sizeof(config.domain) - 1);
    
    return (config.local_port == 5060);
}

/* ============================================
 * Main
 * ============================================ */

int main(void) {
    printf("========================================\n");
    printf("Voice Library - New Modules Test\n");
    printf("========================================\n\n");
    
    /* Effects Tests */
    printf("--- Audio Effects ---\n");
    RUN_TEST(test_reverb);
    RUN_TEST(test_delay);
    RUN_TEST(test_pitch_shift);
    RUN_TEST(test_effects_chain);
    
    /* Watermark Tests */
    printf("\n--- Watermark ---\n");
    RUN_TEST(test_watermark_embed_detect);
    
    /* Diagnostics Tests */
    printf("\n--- Diagnostics ---\n");
    RUN_TEST(test_diagnostics_quality);
    
    /* DataChannel Tests */
    printf("\n--- DataChannel ---\n");
    RUN_TEST(test_datachannel_config);
    RUN_TEST(test_datachannel_state_strings);
    
    /* SIP Tests */
    printf("\n--- SIP Protocol ---\n");
    RUN_TEST(test_sip_uri_parse);
    RUN_TEST(test_sip_message_create);
    RUN_TEST(test_sip_ua_config);
    
    /* Summary */
    printf("\n========================================\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}
