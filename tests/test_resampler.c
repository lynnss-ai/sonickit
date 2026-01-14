/**
 * @file test_resampler.c
 * @brief Resampler unit tests
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "test_common.h"
#include "dsp/resampler.h"

/* ============================================
 * Test Cases
 * ============================================ */

static int test_resampler_create_destroy(void)
{
    voice_resampler_t *resampler = voice_resampler_create(
        1,      /* channels */
        48000,  /* in_rate */
        16000,  /* out_rate */
        VOICE_RESAMPLE_QUALITY_DEFAULT
    );
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");

    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_downsample(void)
{
    voice_resampler_t *resampler = voice_resampler_create(
        1, 48000, 16000, VOICE_RESAMPLE_QUALITY_DEFAULT
    );
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");

    /* 48kHz -> 16kHz: 3:1 ratio */
    int16_t input[480];
    int16_t output[160];

    generate_sine_wave(input, 480, 440.0f, 48000);

    int out_frames = voice_resampler_process_int16(resampler, input, 480, output, 160);
    TEST_ASSERT(out_frames > 0, "Should produce output frames");
    TEST_ASSERT(out_frames <= 160, "Should not exceed output buffer");

    /* Expected: ~160 frames for 3:1 downsample */
    TEST_ASSERT(out_frames >= 150 && out_frames <= 170, "Output frame count reasonable");

    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_upsample(void)
{
    voice_resampler_t *resampler = voice_resampler_create(
        1, 16000, 48000, VOICE_RESAMPLE_QUALITY_DEFAULT
    );
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");

    /* 16kHz -> 48kHz: 1:3 ratio */
    int16_t input[160];
    int16_t output[480];

    generate_sine_wave(input, 160, 440.0f, 16000);

    int out_frames = voice_resampler_process_int16(resampler, input, 160, output, 480);
    TEST_ASSERT(out_frames > 0, "Should produce output frames");
    TEST_ASSERT(out_frames <= 480, "Should not exceed output buffer");

    /* Expected: ~480 frames for 1:3 upsample */
    TEST_ASSERT(out_frames >= 450 && out_frames <= 490, "Output frame count reasonable");

    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_same_rate(void)
{
    voice_resampler_t *resampler = voice_resampler_create(
        1, 48000, 48000, VOICE_RESAMPLE_QUALITY_DEFAULT
    );
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");

    /* Same rate: 1:1 passthrough */
    int16_t input[480];
    int16_t output[480];

    generate_sine_wave(input, 480, 440.0f, 48000);

    int out_frames = voice_resampler_process_int16(resampler, input, 480, output, 480);
    TEST_ASSERT(out_frames > 0, "Should produce output frames");

    /* For 1:1 ratio, output should match input count */
    TEST_ASSERT_EQ(out_frames, 480, "Should pass through same number of frames");

    /* Verify data is similar (allowing for processing artifacts) */
    TEST_ASSERT(compare_buffers(input, output, 480, 100),
                "Passthrough should preserve data approximately");

    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_stereo(void)
{
    voice_resampler_t *resampler = voice_resampler_create(
        2, 48000, 24000, VOICE_RESAMPLE_QUALITY_DEFAULT
    );
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");

    /* 48kHz -> 24kHz stereo: 2:1 ratio */
    int16_t input[960];   /* 480 frames * 2 channels */
    int16_t output[480];  /* 240 frames * 2 channels */

    /* Generate interleaved stereo */
    for (int i = 0; i < 480; i++) {
        float sample = sinf(2.0f * 3.14159f * 440.0f * i / 48000.0f);
        input[i * 2] = (int16_t)(sample * 16000);      /* Left */
        input[i * 2 + 1] = (int16_t)(sample * 8000);   /* Right (quieter) */
    }

    int out_frames = voice_resampler_process_int16(resampler, input, 480, output, 240);
    TEST_ASSERT(out_frames > 0, "Should produce output frames");
    TEST_ASSERT(out_frames <= 240, "Should not exceed output buffer");

    voice_resampler_destroy(resampler);
    return TEST_PASSED;
}

static int test_resampler_reset(void)
{
    voice_resampler_t *resampler = voice_resampler_create(
        1, 48000, 16000, VOICE_RESAMPLE_QUALITY_DEFAULT
    );
    TEST_ASSERT_NOT_NULL(resampler, "Resampler creation should succeed");

    int16_t input[480];
    int16_t output[160];
    generate_sine_wave(input, 480, 440.0f, 48000);

    /* Process some data */
    voice_resampler_process_int16(resampler, input, 480, output, 160);

    /* Reset should clear internal state */
    voice_resampler_reset(resampler);

    /* Process again after reset */
    int out_frames = voice_resampler_process_int16(resampler, input, 480, output, 160);
    TEST_ASSERT(out_frames > 0, "Should work after reset");

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
