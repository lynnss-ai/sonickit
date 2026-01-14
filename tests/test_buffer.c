/**
 * @file test_buffer.c
 * @brief Audio buffer unit tests
 */

#include "test_common.h"
#include "audio/audio_buffer.h"

/* ============================================
 * Test Cases
 * ============================================ */

static int test_buffer_create_destroy(void)
{
    audio_buffer_t *buffer = audio_buffer_create(1024);
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");
    
    TEST_ASSERT_EQ(audio_buffer_capacity(buffer), 1024, "Capacity should match");
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 0, "Initially empty");
    TEST_ASSERT_EQ(audio_buffer_free_space(buffer), 1024, "All space free");
    
    audio_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_write_read(void)
{
    audio_buffer_t *buffer = audio_buffer_create(1024);
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");
    
    /* Write test data */
    int16_t write_data[256];
    generate_sine_wave(write_data, 256, 440.0f, 48000);
    
    size_t written = audio_buffer_write(buffer, write_data, 256);
    TEST_ASSERT_EQ(written, 256, "Should write all samples");
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 256, "256 samples available");
    
    /* Read back */
    int16_t read_data[256];
    size_t read_count = audio_buffer_read(buffer, read_data, 256);
    TEST_ASSERT_EQ(read_count, 256, "Should read all samples");
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 0, "Buffer empty after read");
    
    /* Verify data */
    TEST_ASSERT(compare_buffers(write_data, read_data, 256, 0), 
                "Read data should match written data");
    
    audio_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_wraparound(void)
{
    audio_buffer_t *buffer = audio_buffer_create(256);
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");
    
    int16_t data[128];
    int16_t read_buf[128];
    
    /* Fill buffer partially */
    generate_sine_wave(data, 128, 440.0f, 48000);
    audio_buffer_write(buffer, data, 128);
    audio_buffer_read(buffer, read_buf, 128);
    
    /* Write again to cause wraparound */
    generate_sine_wave(data, 128, 880.0f, 48000);
    size_t written = audio_buffer_write(buffer, data, 128);
    TEST_ASSERT_EQ(written, 128, "Should write with wraparound");
    
    /* Write more to test full wraparound */
    generate_sine_wave(data, 128, 1000.0f, 48000);
    written = audio_buffer_write(buffer, data, 128);
    TEST_ASSERT_EQ(written, 128, "Should handle full wraparound");
    
    audio_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_overflow(void)
{
    audio_buffer_t *buffer = audio_buffer_create(128);
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");
    
    int16_t data[256];
    generate_silence(data, 256);
    
    /* Try to write more than capacity */
    size_t written = audio_buffer_write(buffer, data, 256);
    TEST_ASSERT_EQ(written, 128, "Should only write up to capacity");
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 128, "Buffer should be full");
    TEST_ASSERT_EQ(audio_buffer_free_space(buffer), 0, "No free space");
    
    audio_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_clear(void)
{
    audio_buffer_t *buffer = audio_buffer_create(256);
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");
    
    int16_t data[128];
    generate_silence(data, 128);
    audio_buffer_write(buffer, data, 128);
    
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 128, "Buffer has data");
    
    audio_buffer_clear(buffer);
    
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 0, "Buffer cleared");
    TEST_ASSERT_EQ(audio_buffer_free_space(buffer), 256, "All space free");
    
    audio_buffer_destroy(buffer);
    return TEST_PASSED;
}

static int test_buffer_peek(void)
{
    audio_buffer_t *buffer = audio_buffer_create(256);
    TEST_ASSERT_NOT_NULL(buffer, "Buffer creation should succeed");
    
    int16_t write_data[64];
    generate_sine_wave(write_data, 64, 440.0f, 48000);
    audio_buffer_write(buffer, write_data, 64);
    
    /* Peek should not consume data */
    int16_t peek_data[64];
    size_t peeked = audio_buffer_peek(buffer, peek_data, 64);
    TEST_ASSERT_EQ(peeked, 64, "Peek should return all data");
    TEST_ASSERT_EQ(audio_buffer_available(buffer), 64, "Data still available after peek");
    
    /* Verify peek data */
    TEST_ASSERT(compare_buffers(write_data, peek_data, 64, 0), 
                "Peeked data should match written data");
    
    audio_buffer_destroy(buffer);
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
