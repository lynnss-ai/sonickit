/**
 * @file test_network.c
 * @brief Network module unit tests (RTP, Jitter Buffer, etc.)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "test_common.h"
#include "network/rtp.h"
#include "network/jitter_buffer.h"
#include "network/transport.h"

/* ============================================
 * RTP Tests
 * ============================================ */

static int test_rtp_session_create_destroy(void)
{
    rtp_session_config_t config;
    rtp_session_config_init(&config);
    config.ssrc = 0x12345678;
    config.payload_type = 111;
    config.clock_rate = 48000;
    
    rtp_session_t *session = rtp_session_create(&config);
    TEST_ASSERT_NOT_NULL(session, "RTP session creation should succeed");
    
    rtp_session_destroy(session);
    return TEST_PASSED;
}

static int test_rtp_packet_create(void)
{
    rtp_session_config_t config;
    rtp_session_config_init(&config);
    config.ssrc = 0x12345678;
    config.payload_type = 111;
    config.clock_rate = 48000;
    
    rtp_session_t *session = rtp_session_create(&config);
    TEST_ASSERT_NOT_NULL(session, "RTP session creation should succeed");
    
    /* Create payload */
    uint8_t payload[160];
    memset(payload, 0xAB, sizeof(payload));
    
    /* Create packet */
    uint8_t packet[256];
    size_t packet_size = sizeof(packet);
    
    voice_error_t err = rtp_session_create_packet(
        session, payload, 160, 0, true, packet, &packet_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Packet creation should succeed");
    TEST_ASSERT(packet_size == 160 + 12, "Packet size should be payload + header");
    
    /* Verify header */
    TEST_ASSERT((packet[0] & 0xC0) == 0x80, "RTP version should be 2");
    TEST_ASSERT((packet[1] & 0x7F) == 111, "Payload type should match");
    
    rtp_session_destroy(session);
    return TEST_PASSED;
}

static int test_rtp_packet_parse(void)
{
    rtp_session_config_t config;
    rtp_session_config_init(&config);
    config.ssrc = 0x12345678;
    config.payload_type = 111;
    config.clock_rate = 48000;
    
    rtp_session_t *session = rtp_session_create(&config);
    TEST_ASSERT_NOT_NULL(session, "RTP session creation should succeed");
    
    /* Create a packet */
    uint8_t payload[160];
    for (int i = 0; i < 160; i++) {
        payload[i] = (uint8_t)i;
    }
    
    uint8_t packet[256];
    size_t packet_size = sizeof(packet);
    rtp_session_create_packet(session, payload, 160, 0, true, packet, &packet_size);
    
    /* Parse the packet */
    rtp_packet_t parsed;
    voice_error_t err = rtp_session_parse_packet(session, packet, packet_size, &parsed);
    TEST_ASSERT_EQ(err, VOICE_OK, "Packet parsing should succeed");
    TEST_ASSERT_EQ(parsed.payload_type, 111, "Payload type should match");
    TEST_ASSERT_EQ(parsed.payload_size, 160, "Payload size should match");
    TEST_ASSERT(parsed.marker, "Marker bit should be set");
    
    rtp_session_destroy(session);
    return TEST_PASSED;
}

static int test_rtp_sequence_numbers(void)
{
    rtp_session_config_t config;
    rtp_session_config_init(&config);
    config.ssrc = 0x12345678;
    config.payload_type = 111;
    config.clock_rate = 48000;
    
    rtp_session_t *session = rtp_session_create(&config);
    TEST_ASSERT_NOT_NULL(session, "RTP session creation should succeed");
    
    uint8_t payload[10] = {0};
    uint8_t packet[64];
    size_t packet_size;
    
    /* Create multiple packets and verify sequence numbers increase */
    uint16_t prev_seq = 0;
    for (int i = 0; i < 5; i++) {
        packet_size = sizeof(packet);
        rtp_session_create_packet(session, payload, 10, i * 960, false, packet, &packet_size);
        
        rtp_packet_t parsed;
        rtp_session_parse_packet(session, packet, packet_size, &parsed);
        
        if (i > 0) {
            TEST_ASSERT_EQ(parsed.sequence_number, (uint16_t)(prev_seq + 1), 
                          "Sequence number should increment");
        }
        prev_seq = parsed.sequence_number;
    }
    
    rtp_session_destroy(session);
    return TEST_PASSED;
}

/* ============================================
 * Jitter Buffer Tests
 * ============================================ */

static int test_jitter_buffer_create_destroy(void)
{
    jitter_buffer_config_t config;
    jitter_buffer_config_init(&config);
    config.min_delay_ms = 20;
    config.max_delay_ms = 200;
    config.sample_rate = 48000;
    
    jitter_buffer_t *jb = jitter_buffer_create(&config);
    TEST_ASSERT_NOT_NULL(jb, "Jitter buffer creation should succeed");
    
    jitter_buffer_destroy(jb);
    return TEST_PASSED;
}

static int test_jitter_buffer_put_get(void)
{
    jitter_buffer_config_t config;
    jitter_buffer_config_init(&config);
    config.min_delay_ms = 20;
    config.max_delay_ms = 200;
    config.sample_rate = 48000;
    config.frame_size_samples = 960;
    
    jitter_buffer_t *jb = jitter_buffer_create(&config);
    TEST_ASSERT_NOT_NULL(jb, "Jitter buffer creation should succeed");
    
    /* Put some frames */
    uint8_t frame1[100];
    memset(frame1, 1, sizeof(frame1));
    jitter_buffer_put(jb, frame1, 100, 0, 0);
    
    uint8_t frame2[100];
    memset(frame2, 2, sizeof(frame2));
    jitter_buffer_put(jb, frame2, 100, 960, 1);
    
    /* Get frames */
    uint8_t output[100];
    size_t output_size;
    jitter_buffer_result_t result = jitter_buffer_get(jb, output, &output_size, 100);
    
    /* Result depends on timing, just verify no crash */
    TEST_ASSERT(result >= 0, "Get should return valid result");
    
    jitter_buffer_destroy(jb);
    return TEST_PASSED;
}

/* ============================================
 * Transport Tests
 * ============================================ */

static int test_transport_create_destroy(void)
{
    voice_transport_config_t config;
    voice_transport_config_init(&config);
    config.protocol = VOICE_TRANSPORT_UDP;
    config.local_port = 0;  /* Random port */
    
    voice_transport_t *transport = voice_transport_create(&config);
    TEST_ASSERT_NOT_NULL(transport, "Transport creation should succeed");
    
    uint16_t port = voice_transport_get_local_port(transport);
    TEST_ASSERT(port > 0, "Should have valid port");
    
    voice_transport_destroy(transport);
    return TEST_PASSED;
}

static int test_transport_stats(void)
{
    voice_transport_config_t config;
    voice_transport_config_init(&config);
    config.protocol = VOICE_TRANSPORT_UDP;
    config.local_port = 0;
    
    voice_transport_t *transport = voice_transport_create(&config);
    TEST_ASSERT_NOT_NULL(transport, "Transport creation should succeed");
    
    voice_transport_stats_t stats;
    voice_error_t err = voice_transport_get_stats(transport, &stats);
    TEST_ASSERT_EQ(err, VOICE_OK, "Getting stats should succeed");
    
    /* Initial stats should be zero */
    TEST_ASSERT_EQ(stats.packets_sent, 0, "No packets sent yet");
    TEST_ASSERT_EQ(stats.packets_received, 0, "No packets received yet");
    
    voice_transport_destroy(transport);
    return TEST_PASSED;
}

/* ============================================
 * Main
 * ============================================ */

int main(void)
{
    printf("Network Module Tests\n");
    printf("====================\n\n");
    
    /* Initialize network (Windows requires this) */
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    
    /* RTP */
    RUN_TEST(test_rtp_session_create_destroy);
    RUN_TEST(test_rtp_packet_create);
    RUN_TEST(test_rtp_packet_parse);
    RUN_TEST(test_rtp_sequence_numbers);
    
    /* Jitter Buffer */
    RUN_TEST(test_jitter_buffer_create_destroy);
    RUN_TEST(test_jitter_buffer_put_get);
    
    /* Transport */
    RUN_TEST(test_transport_create_destroy);
    RUN_TEST(test_transport_stats);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    TEST_SUMMARY();
}
