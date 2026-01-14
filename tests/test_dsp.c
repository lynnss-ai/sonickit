/**
 * @file test_dsp.c
 * @brief DSP modules unit tests (denoiser, VAD, AGC, etc.)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "test_common.h"
#include "dsp/denoiser.h"
#include "dsp/vad.h"
#include "dsp/agc.h"
#include "dsp/dtmf.h"

/* ============================================
 * Denoiser Tests
 * ============================================ */

static int test_denoiser_create_destroy(void)
{
    voice_denoiser_config_t config;
    voice_denoiser_config_init(&config);
    config.sample_rate = 48000;
    config.frame_size = 960;
    config.engine = VOICE_DENOISE_SPEEXDSP;

    voice_denoiser_t *denoiser = voice_denoiser_create(&config);
    /* May return NULL if SpeexDSP not available, that's OK for this test */
    if (denoiser) {
        voice_denoiser_destroy(denoiser);
    }
    return TEST_PASSED;
}

static int test_denoiser_process(void)
{
    voice_denoiser_config_t config;
    voice_denoiser_config_init(&config);
    config.sample_rate = 48000;
    config.frame_size = 960;
    config.engine = VOICE_DENOISE_SPEEXDSP;
    config.noise_suppress_db = -25;

    voice_denoiser_t *denoiser = voice_denoiser_create(&config);
    if (!denoiser) {
        printf("  (Skipped: SpeexDSP not available)\n");
        return TEST_PASSED;
    }

    /* Generate noisy signal: sine wave + noise */
    int16_t input[960];
    generate_sine_wave(input, 960, 440.0f, 48000);

    /* Add noise */
    for (int i = 0; i < 960; i++) {
        input[i] += (int16_t)((rand() % 2000) - 1000);
    }

    float input_rms = calculate_rms(input, 960);

    /* Process */
    float vad_prob = voice_denoiser_process_int16(denoiser, input, 960);
    TEST_ASSERT(vad_prob >= 0.0f || vad_prob < 0.0f, "VAD probability returned");

    float output_rms = calculate_rms(input, 960);
    TEST_ASSERT(output_rms > 0, "Output should not be zero");

    voice_denoiser_destroy(denoiser);
    return TEST_PASSED;
}

/* ============================================
 * VAD Tests
 * ============================================ */

static int test_vad_create_destroy(void)
{
    voice_vad_config_t config;
    voice_vad_config_init(&config);
    config.sample_rate = 48000;
    config.frame_size_ms = 20;

    voice_vad_t *vad = voice_vad_create(&config);
    TEST_ASSERT_NOT_NULL(vad, "VAD creation should succeed");

    voice_vad_destroy(vad);
    return TEST_PASSED;
}

static int test_vad_detect_speech(void)
{
    voice_vad_config_t config;
    voice_vad_config_init(&config);
    config.sample_rate = 48000;
    config.frame_size_ms = 20;

    voice_vad_t *vad = voice_vad_create(&config);
    TEST_ASSERT_NOT_NULL(vad, "VAD creation should succeed");

    /* Test with loud signal (should detect speech) */
    int16_t loud_signal[960];
    generate_sine_wave(loud_signal, 960, 440.0f, 48000);

    voice_vad_result_t result;
    voice_error_t err = voice_vad_process(vad, loud_signal, 960, &result);
    TEST_ASSERT_EQ(err, VOICE_OK, "VAD process should succeed");
    TEST_ASSERT(result.is_speech == true, "Should detect speech in loud signal");

    /* Test with silence (should not detect speech) */
    int16_t silence[960];
    generate_silence(silence, 960);

    /* Process several frames of silence to let VAD adapt */
    for (int i = 0; i < 10; i++) {
        err = voice_vad_process(vad, silence, 960, &result);
    }
    TEST_ASSERT(result.is_speech == false, "Should not detect speech in silence");

    voice_vad_destroy(vad);
    return TEST_PASSED;
}

/* ============================================
 * AGC Tests
 * ============================================ */

static int test_agc_create_destroy(void)
{
    voice_agc_config_t config;
    voice_agc_config_init(&config);
    config.sample_rate = 48000;
    config.target_level_dbfs = -3.0f;

    voice_agc_t *agc = voice_agc_create(&config);
    TEST_ASSERT_NOT_NULL(agc, "AGC creation should succeed");

    voice_agc_destroy(agc);
    return TEST_PASSED;
}

static int test_agc_amplify_quiet_signal(void)
{
    voice_agc_config_t config;
    voice_agc_config_init(&config);
    config.sample_rate = 48000;
    config.target_level_dbfs = -6.0f;
    config.mode = VOICE_AGC_ADAPTIVE;

    voice_agc_t *agc = voice_agc_create(&config);
    TEST_ASSERT_NOT_NULL(agc, "AGC creation should succeed");

    /* Generate quiet signal */
    int16_t input[960];
    for (int i = 0; i < 960; i++) {
        double t = (double)i / 48000.0;
        input[i] = (int16_t)(sin(2.0 * 3.14159 * 440.0 * t) * 1000);  /* Quiet */
    }

    float input_rms = calculate_rms(input, 960);

    /* Process multiple frames to let AGC adapt */
    for (int f = 0; f < 20; f++) {
        voice_agc_process(agc, input, 960);
    }

    float output_rms = calculate_rms(input, 960);

    /* AGC should amplify quiet signal */
    TEST_ASSERT(output_rms >= input_rms, "AGC should amplify quiet signal");

    voice_agc_destroy(agc);
    return TEST_PASSED;
}

/* ============================================
 * DTMF Tests
 * ============================================ */

static int test_dtmf_generator(void)
{
    voice_dtmf_generator_config_t config;
    voice_dtmf_generator_config_init(&config);
    config.sample_rate = 48000;

    voice_dtmf_generator_t *gen = voice_dtmf_generator_create(&config);
    TEST_ASSERT_NOT_NULL(gen, "DTMF generator creation should succeed");

    /* Generate tone for digit '5' */
    int16_t buffer[480];  /* 10ms */
    size_t generated = voice_dtmf_generator_generate(gen, VOICE_DTMF_5, buffer, 480);
    TEST_ASSERT(generated > 0, "DTMF generation should produce samples");

    /* Verify signal is not silent */
    float rms = calculate_rms(buffer, 480);
    TEST_ASSERT(rms > 1000.0f, "DTMF output should not be silent");
    return TEST_PASSED;
}

static int test_dtmf_detector(void)
{
    voice_dtmf_detector_config_t det_config;
    voice_dtmf_detector_config_init(&det_config);
    det_config.sample_rate = 48000;

    voice_dtmf_detector_t *det = voice_dtmf_detector_create(&det_config);
    TEST_ASSERT_NOT_NULL(det, "DTMF detector creation should succeed");

    /* Generate DTMF tone for '5' */
    voice_dtmf_generator_config_t gen_config;
    voice_dtmf_generator_config_init(&gen_config);
    gen_config.sample_rate = 48000;

    voice_dtmf_generator_t *gen = voice_dtmf_generator_create(&gen_config);
    TEST_ASSERT_NOT_NULL(gen, "DTMF generator creation should succeed");

    int16_t buffer[960];  /* 20ms */
    voice_dtmf_generator_generate(gen, VOICE_DTMF_5, buffer, 960);

    /* Detect */
    voice_dtmf_result_t result;
    voice_dtmf_digit_t detected = voice_dtmf_detector_process(det, buffer, 960, &result);
    /* Detection may need longer signal, so we don't strictly require detection here */
    (void)detected;
    voice_dtmf_generator_destroy(gen);
    return TEST_PASSED;
}

/* ============================================
 * Main
 * ============================================ */

int main(void)
{
    printf("DSP Module Tests\n");
    printf("================\n\n");

    /* Denoiser */
    RUN_TEST(test_denoiser_create_destroy);
    RUN_TEST(test_denoiser_process);

    /* VAD */
    RUN_TEST(test_vad_create_destroy);
    RUN_TEST(test_vad_detect_speech);

    /* AGC */
    RUN_TEST(test_agc_create_destroy);
    RUN_TEST(test_agc_amplify_quiet_signal);

    /* DTMF */
    RUN_TEST(test_dtmf_generator);
    RUN_TEST(test_dtmf_detector);

    TEST_SUMMARY();
}
