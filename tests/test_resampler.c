/**
 * @file test_resampler.c
 * @brief Resampler unit tests
 */

#include "test_common.h"
#include "dsp/resampler.h"

/* ============================================
 * Test Cases
 * ============================================ */

static int test_resampler_create_destroy(void)
{
    voice_resampler_config_t config;
    voice_resampler_config_init(&config);
    config.input_rate = 48000;
    config.output_rate = 16000;
    config.channels = 1;
    config.quality = 5;
    
    voice_resampler_t *resampler = voice_resampler_create(&config);
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");
    
    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_downsample(void)
{
    voice_resampler_config_t config;
    voice_resampler_config_init(&config);
    config.input_rate = 48000;
    config.output_rate = 16000;
    config.channels = 1;
    config.quality = 5;
    
    voice_resampler_t *resampler = voice_resampler_create(&config);
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");
    
    /* Generate 48kHz input (10ms = 480 samples) */
    int16_t input[480];
    generate_sine_wave(input, 480, 1000.0f, 48000);
    
    /* Expected output: 16kHz (10ms = 160 samples) */
    int16_t output[256];
    size_t out_samples = 256;
    
    voice_error_t err = voice_resampler_process(resampler, input, 480, output, &out_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Resampling should succeed");
    TEST_ASSERT(out_samples >= 150 && out_samples <= 170, 
                "Output should be approximately 160 samples");
    
    /* Verify output is not silent */
    float rms = calculate_rms(output, out_samples);
    TEST_ASSERT(rms > 1000.0f, "Output should not be silent");
    
    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_upsample(void)
{
    voice_resampler_config_t config;
    voice_resampler_config_init(&config);
    config.input_rate = 16000;
    config.output_rate = 48000;
    config.channels = 1;
    config.quality = 5;
    
    voice_resampler_t *resampler = voice_resampler_create(&config);
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");
    
    /* Generate 16kHz input (10ms = 160 samples) */
    int16_t input[160];
    generate_sine_wave(input, 160, 1000.0f, 16000);
    
    /* Expected output: 48kHz (10ms = 480 samples) */
    int16_t output[640];
    size_t out_samples = 640;
    
    voice_error_t err = voice_resampler_process(resampler, input, 160, output, &out_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Resampling should succeed");
    TEST_ASSERT(out_samples >= 450 && out_samples <= 510, 
                "Output should be approximately 480 samples");
    
    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_same_rate(void)
{
    voice_resampler_config_t config;
    voice_resampler_config_init(&config);
    config.input_rate = 48000;
    config.output_rate = 48000;
    config.channels = 1;
    config.quality = 5;
    
    voice_resampler_t *resampler = voice_resampler_create(&config);
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");
    
    int16_t input[480];
    generate_sine_wave(input, 480, 1000.0f, 48000);
    
    int16_t output[512];
    size_t out_samples = 512;
    
    voice_error_t err = voice_resampler_process(resampler, input, 480, output, &out_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Same-rate processing should succeed");
    TEST_ASSERT_EQ(out_samples, 480, "Output should equal input for same rate");
    
    /* Data should be nearly identical */
    int match = compare_buffers(input, output, 480, 10);
    TEST_ASSERT(match, "Output should match input for same rate");
    
    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_stereo(void)
{
    voice_resampler_config_t config;
    voice_resampler_config_init(&config);
    config.input_rate = 48000;
    config.output_rate = 24000;
    config.channels = 2;
    config.quality = 5;
    
    voice_resampler_t *resampler = voice_resampler_create(&config);
    TEST_ASSERT_NOT_NULL(resampler, "Stereo resampler creation should succeed");
    
    /* 48kHz stereo: 480 frames = 960 samples */
    int16_t input[960];
    for (int i = 0; i < 480; i++) {
        double t = (double)i / 48000.0;
        int16_t left = (int16_t)(sin(2.0 * 3.14159 * 440.0 * t) * 32767);
        int16_t right = (int16_t)(sin(2.0 * 3.14159 * 880.0 * t) * 32767);
        input[i * 2] = left;
        input[i * 2 + 1] = right;
    }
    
    /* Expected: 24kHz, 240 frames = 480 samples */
    int16_t output[600];
    size_t out_samples = 600;
    
    voice_error_t err = voice_resampler_process(resampler, input, 960, output, &out_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Stereo resampling should succeed");
    TEST_ASSERT(out_samples >= 450 && out_samples <= 510, 
                "Output should be approximately 480 samples");
    
    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_reset(void)
{
    voice_resampler_config_t config;
    voice_resampler_config_init(&config);
    config.input_rate = 48000;
    config.output_rate = 16000;
    config.channels = 1;
    config.quality = 5;
    
    voice_resampler_t *resampler = voice_resampler_create(&config);
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");
    
    int16_t input[480];
    int16_t output[256];
    size_t out_samples;
    
    /* Process some data */
    generate_sine_wave(input, 480, 1000.0f, 48000);
    out_samples = 256;
    voice_resampler_process(resampler, input, 480, output, &out_samples);
    
    /* Reset */
    voice_resampler_reset(resampler);
    
    /* Process again - should work normally */
    out_samples = 256;
    voice_error_t err = voice_resampler_process(resampler, input, 480, output, &out_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Should work after reset");
    
    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

/* ============================================
 * Main
 * ============================================ */

int main(void)
{
    printf("Resampler Tests\n");
    printf("===============\n\n");
    
    RUN_TEST(test_resampler_create_destroy);
    RUN_TEST(test_resampler_downsample);
    RUN_TEST(test_resampler_upsample);
    RUN_TEST(test_resampler_same_rate);
    RUN_TEST(test_resampler_stereo);
    RUN_TEST(test_resampler_reset);
    
    TEST_SUMMARY();
}
