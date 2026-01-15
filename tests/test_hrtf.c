/**
 * @file test_hrtf.c
 * @brief Unit tests for HRTF binaural audio processing
 */

#include "test_common.h"
#include "dsp/hrtf.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Test: HRTF Load Builtin
 * ============================================ */
static int test_hrtf_load_builtin(void) {
    printf("  Testing HRTF load builtin...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load builtin HRTF");

    size_t num_positions, hrir_length;
    uint32_t sample_rate;
    voice_hrtf_get_info(hrtf, &num_positions, &hrir_length, &sample_rate);

    printf("    Positions: %zu, HRIR length: %zu, Sample rate: %u\n",
           num_positions, hrir_length, sample_rate);

    TEST_ASSERT(num_positions > 0, "No HRIR positions");
    TEST_ASSERT(hrir_length > 0, "HRIR length is zero");
    TEST_ASSERT(sample_rate == 48000, "Unexpected sample rate");

    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF Create Custom
 * ============================================ */
static int test_hrtf_create_custom(void) {
    printf("  Testing HRTF create custom...\n");

    voice_hrtf_t *hrtf = voice_hrtf_create(10, 64, 44100);
    TEST_ASSERT(hrtf != NULL, "Failed to create HRTF");

    /* Add some HRIRs */
    float left[64], right[64];
    for (int i = 0; i < 64; i++) {
        left[i] = (i == 5) ? 1.0f : 0.0f;  /* Impulse at sample 5 */
        right[i] = (i == 7) ? 1.0f : 0.0f; /* Impulse at sample 7 */
    }

    voice_error_t err = voice_hrtf_add_hrir(hrtf, 0.0f, 0.0f, left, right, 64);
    TEST_ASSERT(err == VOICE_SUCCESS, "Failed to add HRIR 1");

    err = voice_hrtf_add_hrir(hrtf, 90.0f, 0.0f, left, right, 64);
    TEST_ASSERT(err == VOICE_SUCCESS, "Failed to add HRIR 2");

    size_t num_positions;
    voice_hrtf_get_info(hrtf, &num_positions, NULL, NULL);
    TEST_ASSERT(num_positions == 2, "Wrong position count");

    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF Interpolation
 * ============================================ */
static int test_hrtf_interpolation(void) {
    printf("  Testing HRTF interpolation...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    float left[128], right[128];

    /* Test exact position (front) */
    size_t len = voice_hrtf_interpolate(hrtf, 0.0f, 0.0f, left, right, 128);
    TEST_ASSERT(len > 0, "Interpolation failed");

    /* Front should be symmetric */
    float diff_sum = 0.0f;
    for (size_t i = 0; i < len; i++) {
        diff_sum += fabsf(left[i] - right[i]);
    }
    printf("    Front (0,0) left-right diff: %.4f\n", diff_sum);
    TEST_ASSERT(diff_sum < 0.5f, "Front not symmetric");

    /* Test side position (90 degrees) */
    len = voice_hrtf_interpolate(hrtf, 90.0f, 0.0f, left, right, 128);
    TEST_ASSERT(len > 0, "Interpolation failed for 90 deg");

    /* Right side: right ear should have more energy */
    float left_energy = 0.0f, right_energy = 0.0f;
    for (size_t i = 0; i < len; i++) {
        left_energy += left[i] * left[i];
        right_energy += right[i] * right[i];
    }
    printf("    Right side (90,0) - Left: %.4f, Right: %.4f\n",
           left_energy, right_energy);
    TEST_ASSERT(right_energy > left_energy, "Right side energy wrong");

    /* Test left side (-90 degrees) */
    len = voice_hrtf_interpolate(hrtf, -90.0f, 0.0f, left, right, 128);
    TEST_ASSERT(len > 0, "Interpolation failed for -90 deg");

    left_energy = 0.0f;
    right_energy = 0.0f;
    for (size_t i = 0; i < len; i++) {
        left_energy += left[i] * left[i];
        right_energy += right[i] * right[i];
    }
    printf("    Left side (-90,0) - Left: %.4f, Right: %.4f\n",
           left_energy, right_energy);
    TEST_ASSERT(left_energy > right_energy, "Left side energy wrong");

    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF ITD Calculation
 * ============================================ */
static int test_hrtf_itd(void) {
    printf("  Testing HRTF ITD calculation...\n");

    float head_radius = 0.0875f;  /* Average head radius */

    /* Front: ITD should be zero */
    float itd = voice_hrtf_calculate_itd(0.0f, head_radius);
    printf("    Front ITD: %.6f ms\n", itd * 1000.0f);
    TEST_ASSERT(fabsf(itd) < 0.0001f, "Front ITD not zero");

    /* Right 90 degrees: max ITD */
    itd = voice_hrtf_calculate_itd(90.0f, head_radius);
    printf("    Right 90deg ITD: %.6f ms\n", itd * 1000.0f);
    TEST_ASSERT(itd > 0.0002f, "Right ITD too small");
    TEST_ASSERT(itd < 0.001f, "Right ITD too large");

    /* Left 90 degrees: negative ITD */
    itd = voice_hrtf_calculate_itd(-90.0f, head_radius);
    printf("    Left -90deg ITD: %.6f ms\n", itd * 1000.0f);
    TEST_ASSERT(itd < -0.0002f, "Left ITD wrong sign");

    /* Back: should be zero again */
    itd = voice_hrtf_calculate_itd(180.0f, head_radius);
    printf("    Back ITD: %.6f ms\n", itd * 1000.0f);
    TEST_ASSERT(fabsf(itd) < 0.0001f, "Back ITD not zero");

    return 0;
}

/* ============================================
 * Test: HRTF Processor Create/Destroy
 * ============================================ */
static int test_hrtf_processor_lifecycle(void) {
    printf("  Testing HRTF processor lifecycle...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    voice_hrtf_config_t config;
    voice_hrtf_config_init(&config);

    TEST_ASSERT(config.sample_rate == 48000, "Wrong default sample rate");
    TEST_ASSERT(config.block_size == 256, "Wrong default block size");
    TEST_ASSERT(config.enable_crossfade == true, "Crossfade not enabled");
    TEST_ASSERT(config.enable_itd == true, "ITD not enabled");

    voice_hrtf_processor_t *proc = voice_hrtf_processor_create(hrtf, &config);
    TEST_ASSERT(proc != NULL, "Failed to create processor");

    voice_hrtf_processor_reset(proc);

    voice_hrtf_processor_destroy(proc);
    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF Processing
 * ============================================ */
static int test_hrtf_processing(void) {
    printf("  Testing HRTF processing...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    voice_hrtf_config_t config;
    voice_hrtf_config_init(&config);
    config.block_size = 256;

    voice_hrtf_processor_t *proc = voice_hrtf_processor_create(hrtf, &config);
    TEST_ASSERT(proc != NULL, "Failed to create processor");

    /* Create mono input (sine wave) */
    const size_t num_samples = 256;
    float mono_input[256];
    float binaural_output[512];  /* Stereo */

    for (size_t i = 0; i < num_samples; i++) {
        mono_input[i] = 0.5f * sinf(2.0f * (float)M_PI * 440.0f * (float)i / 48000.0f);
    }

    /* Process at front position */
    voice_error_t err = voice_hrtf_process(proc, mono_input, binaural_output,
                                            num_samples, 0.0f, 0.0f);
    TEST_ASSERT(err == VOICE_SUCCESS, "Processing failed");

    /* Check output has content */
    float output_energy = 0.0f;
    for (size_t i = 0; i < num_samples * 2; i++) {
        output_energy += binaural_output[i] * binaural_output[i];
    }
    printf("    Front output energy: %.4f\n", output_energy);
    TEST_ASSERT(output_energy > 0.0f, "No output energy");

    /* Process at right position */
    err = voice_hrtf_process(proc, mono_input, binaural_output,
                             num_samples, 90.0f, 0.0f);
    TEST_ASSERT(err == VOICE_SUCCESS, "Processing at 90deg failed");

    /* Calculate left and right channel energy */
    float left_energy = 0.0f, right_energy = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        left_energy += binaural_output[i * 2] * binaural_output[i * 2];
        right_energy += binaural_output[i * 2 + 1] * binaural_output[i * 2 + 1];
    }
    printf("    Right (90deg) - L: %.4f, R: %.4f\n", left_energy, right_energy);

    voice_hrtf_processor_destroy(proc);
    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF int16 Processing
 * ============================================ */
static int test_hrtf_processing_int16(void) {
    printf("  Testing HRTF int16 processing...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    voice_hrtf_config_t config;
    voice_hrtf_config_init(&config);

    voice_hrtf_processor_t *proc = voice_hrtf_processor_create(hrtf, &config);
    TEST_ASSERT(proc != NULL, "Failed to create processor");

    /* Create mono input (sine wave) */
    const size_t num_samples = 256;
    int16_t mono_input[256];
    int16_t binaural_output[512];

    for (size_t i = 0; i < num_samples; i++) {
        mono_input[i] = (int16_t)(16384.0f *
            sinf(2.0f * (float)M_PI * 440.0f * (float)i / 48000.0f));
    }

    voice_error_t err = voice_hrtf_process_int16(proc, mono_input, binaural_output,
                                                  num_samples, 45.0f, 15.0f);
    TEST_ASSERT(err == VOICE_SUCCESS, "int16 processing failed");

    /* Check output has content */
    int has_nonzero = 0;
    for (size_t i = 0; i < num_samples * 2; i++) {
        if (binaural_output[i] != 0) {
            has_nonzero = 1;
            break;
        }
    }
    TEST_ASSERT(has_nonzero, "No output from int16 processing");

    voice_hrtf_processor_destroy(proc);
    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF Position Sweep
 * ============================================ */
static int test_hrtf_position_sweep(void) {
    printf("  Testing HRTF position sweep...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    voice_hrtf_config_t config;
    voice_hrtf_config_init(&config);
    config.enable_crossfade = true;
    config.crossfade_time_ms = 10.0f;

    voice_hrtf_processor_t *proc = voice_hrtf_processor_create(hrtf, &config);
    TEST_ASSERT(proc != NULL, "Failed to create processor");

    const size_t num_samples = 256;
    float mono_input[256];
    float binaural_output[512];

    /* Fill with white noise */
    for (size_t i = 0; i < num_samples; i++) {
        mono_input[i] = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * 0.5f;
    }

    /* Sweep azimuth from -180 to 180 */
    for (float az = -180.0f; az <= 180.0f; az += 30.0f) {
        voice_error_t err = voice_hrtf_process(proc, mono_input, binaural_output,
                                                num_samples, az, 0.0f);
        TEST_ASSERT(err == VOICE_SUCCESS, "Processing failed during sweep");
    }

    /* Sweep elevation */
    for (float el = -45.0f; el <= 90.0f; el += 15.0f) {
        voice_error_t err = voice_hrtf_process(proc, mono_input, binaural_output,
                                                num_samples, 0.0f, el);
        TEST_ASSERT(err == VOICE_SUCCESS, "Processing failed during elev sweep");
    }

    printf("    Sweep completed without errors\n");

    voice_hrtf_processor_destroy(proc);
    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF Elevation Effects
 * ============================================ */
static int test_hrtf_elevation(void) {
    printf("  Testing HRTF elevation effects...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    float left[128], right[128];

    /* Compare front at different elevations */
    size_t len = voice_hrtf_interpolate(hrtf, 0.0f, 0.0f, left, right, 128);
    float horizontal_energy = 0.0f;
    for (size_t i = 0; i < len; i++) {
        horizontal_energy += left[i] * left[i] + right[i] * right[i];
    }

    len = voice_hrtf_interpolate(hrtf, 0.0f, 90.0f, left, right, 128);
    float above_energy = 0.0f;
    for (size_t i = 0; i < len; i++) {
        above_energy += left[i] * left[i] + right[i] * right[i];
    }

    printf("    Horizontal energy: %.4f, Above energy: %.4f\n",
           horizontal_energy, above_energy);

    /* Above (90 degrees elevation) should have different characteristics */
    TEST_ASSERT(fabsf(horizontal_energy - above_energy) > 0.001f,
                "Elevation not affecting HRIR");

    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF Null Parameter Handling
 * ============================================ */
static int test_hrtf_null_params(void) {
    printf("  Testing HRTF null parameter handling...\n");

    /* Test null HRTF */
    voice_hrtf_config_t config;
    voice_hrtf_config_init(&config);

    voice_hrtf_processor_t *proc = voice_hrtf_processor_create(NULL, &config);
    TEST_ASSERT(proc == NULL, "Should fail with null HRTF");

    /* Test null config */
    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    proc = voice_hrtf_processor_create(hrtf, NULL);
    TEST_ASSERT(proc == NULL, "Should fail with null config");

    /* Test null input */
    proc = voice_hrtf_processor_create(hrtf, &config);
    TEST_ASSERT(proc != NULL, "Failed to create processor");

    float output[512];
    voice_error_t err = voice_hrtf_process(proc, NULL, output, 256, 0.0f, 0.0f);
    TEST_ASSERT(err == VOICE_ERROR_INVALID_PARAM, "Should fail with null input");

    float input[256];
    err = voice_hrtf_process(proc, input, NULL, 256, 0.0f, 0.0f);
    TEST_ASSERT(err == VOICE_ERROR_INVALID_PARAM, "Should fail with null output");

    /* Test null in add_hrir */
    err = voice_hrtf_add_hrir(NULL, 0.0f, 0.0f, input, input, 256);
    TEST_ASSERT(err == VOICE_ERROR_INVALID_PARAM, "Should fail with null HRTF");

    /* Test destroy with null (should not crash) */
    voice_hrtf_processor_destroy(NULL);
    voice_hrtf_destroy(NULL);

    voice_hrtf_processor_destroy(proc);
    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Test: HRTF FFT Convolution
 * ============================================ */
static int test_hrtf_fft_convolution(void) {
    printf("  Testing HRTF FFT convolution...\n");

    voice_hrtf_t *hrtf = voice_hrtf_load_builtin();
    TEST_ASSERT(hrtf != NULL, "Failed to load HRTF");

    /* Config with FFT enabled */
    voice_hrtf_config_t config;
    voice_hrtf_config_init(&config);
    config.enable_fft_convolution = true;
    config.block_size = 256;

    voice_hrtf_processor_t *proc = voice_hrtf_processor_create(hrtf, &config);
    TEST_ASSERT(proc != NULL, "Failed to create FFT processor");

    /* Create mono input (sine wave) */
    const size_t num_samples = 256;
    float mono_input[256];
    float binaural_output[512];

    for (size_t i = 0; i < num_samples; i++) {
        mono_input[i] = 0.5f * sinf(2.0f * (float)M_PI * 440.0f * (float)i / 48000.0f);
    }

    /* Process at right position */
    voice_error_t err = voice_hrtf_process(proc, mono_input, binaural_output,
                                            num_samples, 90.0f, 0.0f);
    TEST_ASSERT(err == VOICE_SUCCESS, "FFT processing failed");

    /* Calculate left and right channel energy */
    float left_energy = 0.0f, right_energy = 0.0f;
    for (size_t i = 0; i < num_samples; i++) {
        left_energy += binaural_output[i * 2] * binaural_output[i * 2];
        right_energy += binaural_output[i * 2 + 1] * binaural_output[i * 2 + 1];
    }
    printf("    FFT Right (90deg) - L: %.4f, R: %.4f\n", left_energy, right_energy);

    /* Right source should have more energy in right channel */
    TEST_ASSERT(right_energy > left_energy, "FFT: right ear should be louder");

    voice_hrtf_processor_destroy(proc);
    voice_hrtf_destroy(hrtf);

    return 0;
}

/* ============================================
 * Main
 * ============================================ */
int main(void) {
    printf("\n=== HRTF Binaural Audio Tests ===\n\n");

    RUN_TEST(test_hrtf_load_builtin);
    RUN_TEST(test_hrtf_create_custom);
    RUN_TEST(test_hrtf_interpolation);
    RUN_TEST(test_hrtf_itd);
    RUN_TEST(test_hrtf_processor_lifecycle);
    RUN_TEST(test_hrtf_processing);
    RUN_TEST(test_hrtf_processing_int16);
    RUN_TEST(test_hrtf_position_sweep);
    RUN_TEST(test_hrtf_elevation);
    RUN_TEST(test_hrtf_null_params);
    RUN_TEST(test_hrtf_fft_convolution);

    TEST_SUMMARY();
}
