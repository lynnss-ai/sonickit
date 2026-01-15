/**
 * @file test_spatial.c
 * @brief Spatial audio module unit tests
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "test_common.h"
#include "dsp/spatial.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Vector Math Tests
 * ============================================ */

static int test_vec3_distance(void)
{
    voice_vec3_t a = {0.0f, 0.0f, 0.0f};
    voice_vec3_t b = {3.0f, 4.0f, 0.0f};

    float dist = voice_vec3_distance(&a, &b);

    /* Should be 5.0 (3-4-5 triangle) */
    TEST_ASSERT(fabsf(dist - 5.0f) < 0.001f,
                "Distance should be 5.0");

    return TEST_PASSED;
}

/* ============================================
 * Distance Attenuation Tests
 * ============================================ */

static int test_distance_attenuation_none(void)
{
    float atten = voice_spatial_distance_attenuation(
        100.0f, 1.0f, 1000.0f, 1.0f, VOICE_DISTANCE_NONE);

    TEST_ASSERT(fabsf(atten - 1.0f) < 0.001f,
                "No attenuation should return 1.0");

    return TEST_PASSED;
}

static int test_distance_attenuation_inverse(void)
{
    /* At min_distance, should be 1.0 */
    float atten1 = voice_spatial_distance_attenuation(
        1.0f, 1.0f, 100.0f, 1.0f, VOICE_DISTANCE_INVERSE);
    TEST_ASSERT(fabsf(atten1 - 1.0f) < 0.001f,
                "At min_distance, attenuation should be 1.0");

    /* At 2x min_distance with rolloff=1, should be 0.5 */
    float atten2 = voice_spatial_distance_attenuation(
        2.0f, 1.0f, 100.0f, 1.0f, VOICE_DISTANCE_INVERSE);
    TEST_ASSERT(fabsf(atten2 - 0.5f) < 0.001f,
                "At 2x min_distance, attenuation should be 0.5");

    return TEST_PASSED;
}

static int test_distance_attenuation_linear(void)
{
    /* At min_distance, should be 1.0 */
    float atten1 = voice_spatial_distance_attenuation(
        0.0f, 0.0f, 10.0f, 1.0f, VOICE_DISTANCE_LINEAR);
    TEST_ASSERT(fabsf(atten1 - 1.0f) < 0.001f,
                "At min_distance, attenuation should be 1.0");

    /* At max_distance, should be 0.0 */
    float atten2 = voice_spatial_distance_attenuation(
        10.0f, 0.0f, 10.0f, 1.0f, VOICE_DISTANCE_LINEAR);
    TEST_ASSERT(fabsf(atten2 - 0.0f) < 0.001f,
                "At max_distance, attenuation should be 0.0");

    /* At halfway, should be 0.5 */
    float atten3 = voice_spatial_distance_attenuation(
        5.0f, 0.0f, 10.0f, 1.0f, VOICE_DISTANCE_LINEAR);
    TEST_ASSERT(fabsf(atten3 - 0.5f) < 0.001f,
                "At halfway, attenuation should be 0.5");

    return TEST_PASSED;
}

/* ============================================
 * Stereo Panning Tests
 * ============================================ */

static int test_pan_center(void)
{
    float mono[4] = {1.0f, 0.5f, -0.5f, -1.0f};
    float stereo[8] = {0};

    /* Center pan with constant power law */
    voice_spatial_pan_mono(mono, stereo, 4, 0.0f, VOICE_PAN_CONSTANT_POWER);

    /* Both channels should have equal amplitude (~0.707 of original) */
    for (int i = 0; i < 4; i++) {
        float expected = mono[i] * 0.707f;
        TEST_ASSERT(fabsf(stereo[i * 2] - expected) < 0.01f,
                    "Left channel should be ~0.707x at center pan");
        TEST_ASSERT(fabsf(stereo[i * 2 + 1] - expected) < 0.01f,
                    "Right channel should be ~0.707x at center pan");
    }

    return TEST_PASSED;
}

static int test_pan_hard_left(void)
{
    float mono[4] = {1.0f, 0.5f, -0.5f, -1.0f};
    float stereo[8] = {0};

    /* Hard left pan */
    voice_spatial_pan_mono(mono, stereo, 4, -1.0f, VOICE_PAN_CONSTANT_POWER);

    /* Left channel should be full, right should be near zero */
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(fabsf(stereo[i * 2] - mono[i]) < 0.01f,
                    "Left channel should be full at hard left pan");
        TEST_ASSERT(fabsf(stereo[i * 2 + 1]) < 0.01f,
                    "Right channel should be ~0 at hard left pan");
    }

    return TEST_PASSED;
}

static int test_pan_hard_right(void)
{
    float mono[4] = {1.0f, 0.5f, -0.5f, -1.0f};
    float stereo[8] = {0};

    /* Hard right pan */
    voice_spatial_pan_mono(mono, stereo, 4, 1.0f, VOICE_PAN_CONSTANT_POWER);

    /* Right channel should be full, left should be near zero */
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT(fabsf(stereo[i * 2]) < 0.01f,
                    "Left channel should be ~0 at hard right pan");
        TEST_ASSERT(fabsf(stereo[i * 2 + 1] - mono[i]) < 0.01f,
                    "Right channel should be full at hard right pan");
    }

    return TEST_PASSED;
}

/* ============================================
 * Azimuth Calculation Tests
 * ============================================ */

static int test_azimuth_front(void)
{
    voice_spatial_listener_t listener;
    voice_spatial_listener_init(&listener);

    /* Source directly in front */
    voice_vec3_t source_pos = {0.0f, 0.0f, -5.0f};

    float azimuth = voice_spatial_azimuth(&listener, &source_pos);

    /* Should be 0 degrees */
    TEST_ASSERT(fabsf(azimuth) < 1.0f,
                "Source in front should have ~0 azimuth");

    return TEST_PASSED;
}

static int test_azimuth_right(void)
{
    voice_spatial_listener_t listener;
    voice_spatial_listener_init(&listener);

    /* Source to the right */
    voice_vec3_t source_pos = {5.0f, 0.0f, 0.0f};

    float azimuth = voice_spatial_azimuth(&listener, &source_pos);

    /* Should be ~90 degrees */
    TEST_ASSERT(fabsf(azimuth - 90.0f) < 1.0f,
                "Source to right should have ~90 azimuth");

    return TEST_PASSED;
}

static int test_azimuth_left(void)
{
    voice_spatial_listener_t listener;
    voice_spatial_listener_init(&listener);

    /* Source to the left */
    voice_vec3_t source_pos = {-5.0f, 0.0f, 0.0f};

    float azimuth = voice_spatial_azimuth(&listener, &source_pos);

    /* Should be ~-90 degrees */
    TEST_ASSERT(fabsf(azimuth + 90.0f) < 1.0f,
                "Source to left should have ~-90 azimuth");

    return TEST_PASSED;
}

/* ============================================
 * Renderer Tests
 * ============================================ */

static int test_renderer_create_destroy(void)
{
    voice_spatial_config_t config;
    voice_spatial_config_init(&config);

    voice_spatial_renderer_t *renderer = voice_spatial_renderer_create(&config);
    TEST_ASSERT_NOT_NULL(renderer, "Renderer creation should succeed");

    voice_spatial_renderer_destroy(renderer);
    return TEST_PASSED;
}

static int test_renderer_process(void)
{
    voice_spatial_config_t config;
    voice_spatial_config_init(&config);

    voice_spatial_renderer_t *renderer = voice_spatial_renderer_create(&config);
    TEST_ASSERT_NOT_NULL(renderer, "Renderer creation should succeed");

    /* Create source to the right */
    voice_spatial_source_t source;
    voice_spatial_source_init(&source);
    source.position.x = 5.0f;
    source.position.y = 0.0f;
    source.position.z = 0.0f;

    /* Generate mono test signal */
    float mono[480];
    for (int i = 0; i < 480; i++) {
        mono[i] = sinf(2.0f * (float)M_PI * 440.0f * i / 48000.0f);
    }

    /* Render to stereo */
    float stereo[960];
    voice_error_t err = voice_spatial_render_source(
        renderer, &source, mono, stereo, 480);

    TEST_ASSERT(err == VOICE_SUCCESS, "Render should succeed");

    /* Right channel should be louder than left (source on right) */
    float sum_left = 0.0f, sum_right = 0.0f;
    for (int i = 0; i < 480; i++) {
        sum_left += fabsf(stereo[i * 2]);
        sum_right += fabsf(stereo[i * 2 + 1]);
    }

    TEST_ASSERT(sum_right > sum_left,
                "Right channel should be louder for source on right");

    voice_spatial_renderer_destroy(renderer);
    return TEST_PASSED;
}

static int test_renderer_distance_attenuation(void)
{
    voice_spatial_config_t config;
    voice_spatial_config_init(&config);
    config.distance_model = VOICE_DISTANCE_INVERSE;

    voice_spatial_renderer_t *renderer = voice_spatial_renderer_create(&config);
    TEST_ASSERT_NOT_NULL(renderer, "Renderer creation should succeed");

    /* Source near */
    voice_spatial_source_t source_near;
    voice_spatial_source_init(&source_near);
    source_near.position.z = -1.0f;  /* 1 meter in front */

    /* Source far */
    voice_spatial_source_t source_far;
    voice_spatial_source_init(&source_far);
    source_far.position.z = -10.0f;  /* 10 meters in front */

    /* Test signal */
    float mono[480];
    for (int i = 0; i < 480; i++) {
        mono[i] = 1.0f;  /* Constant signal for easy comparison */
    }

    /* Render near source */
    float stereo_near[960];
    voice_spatial_render_source(renderer, &source_near, mono, stereo_near, 480);

    /* Render far source */
    float stereo_far[960];
    voice_spatial_render_source(renderer, &source_far, mono, stereo_far, 480);

    /* Near source should be louder */
    TEST_ASSERT(fabsf(stereo_near[0]) > fabsf(stereo_far[0]),
                "Near source should be louder than far source");

    voice_spatial_renderer_destroy(renderer);
    return TEST_PASSED;
}

/* ============================================
 * HRTF Integration Test
 * ============================================ */

static int test_renderer_hrtf_integration(void)
{
    /* Test with HRTF enabled */
    voice_spatial_config_t config;
    voice_spatial_config_init(&config);
    config.enable_hrtf = true;  /* Enable HRTF */
    config.sample_rate = 48000;
    config.frame_size = 256;

    voice_spatial_renderer_t *renderer = voice_spatial_renderer_create(&config);
    TEST_ASSERT(renderer != NULL, "Failed to create renderer with HRTF");

    /* Set listener */
    voice_spatial_listener_t listener;
    voice_spatial_listener_init(&listener);
    voice_spatial_set_listener(renderer, &listener);

    /* Set source to the right */
    voice_spatial_source_t source;
    voice_spatial_source_init(&source);
    source.position.x = 2.0f;   /* Right */
    source.position.y = 0.0f;
    source.position.z = 0.0f;

    /* Create test signal */
    const size_t num_samples = 256;
    float mono_input[256];
    float stereo_output[512];

    for (size_t i = 0; i < num_samples; i++) {
        mono_input[i] = 0.5f * sinf(2.0f * 3.14159f * 440.0f * (float)i / 48000.0f);
    }

    /* Render with HRTF */
    voice_error_t err = voice_spatial_render_source(renderer, &source,
                                                     mono_input, stereo_output,
                                                     num_samples);
    TEST_ASSERT(err == VOICE_SUCCESS, "HRTF render failed");

    /* Check output has content */
    float left_energy = 0.0f, right_energy = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        left_energy += stereo_output[i * 2] * stereo_output[i * 2];
        right_energy += stereo_output[i * 2 + 1] * stereo_output[i * 2 + 1];
    }

    printf("    HRTF right source - L: %.4f, R: %.4f\n", left_energy, right_energy);

    /* Right source should have more energy in right channel */
    TEST_ASSERT(right_energy > left_energy, "HRTF right ear should be louder");

    /* Reset and test again */
    voice_spatial_renderer_reset(renderer);

    voice_spatial_renderer_destroy(renderer);
    return TEST_PASSED;
}

/* ============================================
 * Main
 * ============================================ */

int main(void)
{
    printf("Spatial Audio Tests\n");
    printf("==================\n\n");

    /* Vector tests */
    RUN_TEST(test_vec3_distance);

    /* Distance attenuation tests */
    RUN_TEST(test_distance_attenuation_none);
    RUN_TEST(test_distance_attenuation_inverse);
    RUN_TEST(test_distance_attenuation_linear);

    /* Panning tests */
    RUN_TEST(test_pan_center);
    RUN_TEST(test_pan_hard_left);
    RUN_TEST(test_pan_hard_right);

    /* Azimuth tests */
    RUN_TEST(test_azimuth_front);
    RUN_TEST(test_azimuth_right);
    RUN_TEST(test_azimuth_left);

    /* Renderer tests */
    RUN_TEST(test_renderer_create_destroy);
    RUN_TEST(test_renderer_process);
    RUN_TEST(test_renderer_distance_attenuation);

    /* HRTF integration test */
    RUN_TEST(test_renderer_hrtf_integration);

    TEST_SUMMARY();
}
