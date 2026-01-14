/**
 * @file test_buffer.c
 * @brief Audio buffer unit tests
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "test_common.h"
#include "audio/audio_buffer.h"

/* ============================================
 * Test Cases
 * ============================================ */

static int test_buffer_create_destroy(void)
{
    voice_ring_buffer_t *buffer = voice_ring_buffer_create(1024, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");

    TEST_ASSERT_EQ(voice_ring_buffer_available(buffer), 0, "Initially empty");
    TEST_ASSERT_EQ(voice_ring_buffer_free_space(buffer), 1024, "All space free");

    voice_ring_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_write_read(void)
{
    voice_ring_buffer_t *buffer = voice_ring_buffer_create(1024, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");

    /* Write test data */
    int16_t write_data[256];
    generate_sine_wave(write_data, 256, 440.0f, 48000);

    size_t written = voice_ring_buffer_write(buffer, write_data, 256 * sizeof(int16_t));
    TEST_ASSERT_EQ(written, 256 * sizeof(int16_t), "Should write all bytes");
    TEST_ASSERT_EQ(voice_ring_buffer_available(buffer), 256 * sizeof(int16_t), "Bytes available");

    /* Read back */
    int16_t read_data[256];
    size_t read_count = voice_ring_buffer_read(buffer, read_data, 256 * sizeof(int16_t));
    TEST_ASSERT_EQ(read_count, 256 * sizeof(int16_t), "Should read all bytes");
    TEST_ASSERT_EQ(voice_ring_buffer_available(buffer), 0, "Buffer empty after read");

    /* Verify data */
    TEST_ASSERT(compare_buffers(write_data, read_data, 256, 0),
                "Read data should match written data");

    voice_ring_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_wraparound(void)
{
    voice_ring_buffer_t *buffer = voice_ring_buffer_create(512, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");

    int16_t data[128];
    int16_t read_buf[128];

    /* Fill buffer partially */
    generate_sine_wave(data, 128, 440.0f, 48000);
    voice_ring_buffer_write(buffer, data, 128 * sizeof(int16_t));
    voice_ring_buffer_read(buffer, read_buf, 128 * sizeof(int16_t));

    /* Write again to cause wraparound */
    generate_sine_wave(data, 128, 880.0f, 48000);
    size_t written = voice_ring_buffer_write(buffer, data, 128 * sizeof(int16_t));
    TEST_ASSERT_EQ(written, 128 * sizeof(int16_t), "Should write with wraparound");

    /* Read and verify */
    size_t read = voice_ring_buffer_read(buffer, read_buf, 128 * sizeof(int16_t));
    TEST_ASSERT_EQ(read, 128 * sizeof(int16_t), "Should read with wraparound");

    voice_ring_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_overflow(void)
{
    voice_ring_buffer_t *buffer = voice_ring_buffer_create(256, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");

    int16_t data[256];
    generate_silence(data, 256);

    /* Try to write more than capacity */
    size_t written = voice_ring_buffer_write(buffer, data, 512);
    /* Ring buffer should only write up to capacity */
    TEST_ASSERT(written <= 256, "Should only write up to capacity");
    TEST_ASSERT_EQ(voice_ring_buffer_free_space(buffer), 256 - written, "Free space should decrease");

    voice_ring_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_clear(void)
{
    voice_ring_buffer_t *buffer = voice_ring_buffer_create(512, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");

    int16_t data[128];
    generate_silence(data, 128);
    voice_ring_buffer_write(buffer, data, 128 * sizeof(int16_t));

    TEST_ASSERT(voice_ring_buffer_available(buffer) > 0, "Buffer has data");

    voice_ring_buffer_clear(buffer);

    TEST_ASSERT_EQ(voice_ring_buffer_available(buffer), 0, "Buffer cleared");
    TEST_ASSERT_EQ(voice_ring_buffer_free_space(buffer), 512, "All space free");

    voice_ring_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_peek(void)
{
    voice_ring_buffer_t *buffer = voice_ring_buffer_create(512, sizeof(int16_t));
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");

    int16_t write_data[64];
    generate_sine_wave(write_data, 64, 440.0f, 48000);
    voice_ring_buffer_write(buffer, write_data, 64 * sizeof(int16_t));

    /* Peek should not consume data */
    int16_t peek_data[64];
    size_t peeked = voice_ring_buffer_peek(buffer, peek_data, 64 * sizeof(int16_t));
    TEST_ASSERT_EQ(peeked, 64 * sizeof(int16_t), "Peek should return all data");
    TEST_ASSERT_EQ(voice_ring_buffer_available(buffer), 64 * sizeof(int16_t),
                   "Data still available after peek");

    /* Verify peek data */
    TEST_ASSERT(compare_buffers(write_data, peek_data, 64, 0),
                "Peeked data should match written data");

    voice_ring_buffer_destroy(buffer);
    return TEST_PASSED;
}

/* ============================================
 * Main
 * ============================================ */

int main(void)
{
    printf("Audio Buffer Tests\n");
    printf("==================\n\n");

    RUN_TEST(test_buffer_create_destroy);
    RUN_TEST(test_buffer_write_read);
    RUN_TEST(test_buffer_wraparound);
    RUN_TEST(test_buffer_overflow);
    RUN_TEST(test_buffer_clear);
    RUN_TEST(test_buffer_peek);

    TEST_SUMMARY();
}
