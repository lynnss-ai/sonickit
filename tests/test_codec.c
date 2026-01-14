/**
 * @file test_codec.c
 * @brief Codec unit tests
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "test_common.h"
#include "codec/codec.h"

/* ============================================
 * Test Cases
 * ============================================ */

static int test_g711_ulaw_encode_decode(void)
{
    voice_codec_detail_config_t config;
    memset(&config, 0, sizeof(config));
    config.codec_id = VOICE_CODEC_G711_ULAW;
    voice_g711_config_init(&config.u.g711, false);  /* μ-law */

    voice_encoder_t *encoder = voice_encoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder creation should succeed");

    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(decoder, "Decoder creation should succeed");

    /* Encode test data */
    int16_t pcm_input[160];  /* 20ms at 8kHz */
    uint8_t encoded[160];
    int16_t pcm_output[160];

    generate_sine_wave(pcm_input, 160, 440.0f, 8000);

    size_t encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, pcm_input, 160,
                                             encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoding should succeed");
    TEST_ASSERT(encoded_size > 0, "Should produce encoded output");

    /* Decode */
    size_t decoded_samples = 160;
    err = voice_decoder_decode(decoder, encoded, encoded_size,
                               pcm_output, &decoded_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Decoding should succeed");
    TEST_ASSERT_EQ(decoded_samples, 160, "Should decode same number of samples");

    /* G.711 should preserve signal reasonably well */
    TEST_ASSERT(compare_buffers(pcm_input, pcm_output, 160, 100),
                "Decoded should be close to original");

    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

static int test_g711_alaw_encode_decode(void)
{
    voice_codec_detail_config_t config;
    memset(&config, 0, sizeof(config));
    config.codec_id = VOICE_CODEC_G711_ALAW;
    voice_g711_config_init(&config.u.g711, true);  /* A-law */

    voice_encoder_t *encoder = voice_encoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder creation should succeed");

    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(decoder, "Decoder creation should succeed");

    /* Encode test data */
    int16_t pcm_input[160];
    uint8_t encoded[160];
    int16_t pcm_output[160];

    generate_sine_wave(pcm_input, 160, 880.0f, 8000);

    size_t encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, pcm_input, 160,
                                             encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoding should succeed");

    size_t decoded_samples = 160;
    err = voice_decoder_decode(decoder, encoded, encoded_size,
                               pcm_output, &decoded_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Decoding should succeed");

    TEST_ASSERT(compare_buffers(pcm_input, pcm_output, 160, 100),
                "Decoded should be close to original");

    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

static int test_codec_info(void)
{
    voice_codec_detail_config_t config;
    memset(&config, 0, sizeof(config));
    config.codec_id = VOICE_CODEC_G711_ULAW;
    voice_g711_config_init(&config.u.g711, false);

    voice_encoder_t *encoder = voice_encoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder creation should succeed");

    voice_codec_info_t info;
    voice_error_t err = voice_encoder_get_info(encoder, &info);
    TEST_ASSERT_EQ(err, VOICE_OK, "Should get codec info");
    TEST_ASSERT_EQ(info.codec_id, VOICE_CODEC_G711_ULAW, "Codec ID should match");
    TEST_ASSERT_EQ(info.sample_rate, 8000, "Sample rate should be 8kHz");
}

static int test_codec_silence(void)
{
    voice_codec_detail_config_t config;
    memset(&config, 0, sizeof(config));
    config.codec_id = VOICE_CODEC_G711_ULAW;
    voice_g711_config_init(&config.u.g711, false);

    voice_encoder_t *encoder = voice_encoder_create(&config);
    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder creation should succeed");
    TEST_ASSERT_NOT_NULL(decoder, "Decoder creation should succeed");

    /* Encode silence */
    int16_t silence[160];
    uint8_t encoded[160];
    int16_t decoded[160];

    generate_silence(silence, 160);

    size_t encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, silence, 160,
                                             encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoding silence should succeed");

    size_t decoded_samples = 160;
    err = voice_decoder_decode(decoder, encoded, encoded_size,
                               decoded, &decoded_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Decoding silence should succeed");

    /* Decoded silence should be near-zero */
    float rms = calculate_rms(decoded, 160);
    TEST_ASSERT(rms < 500.0f, "Decoded silence should have low energy");

    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

static int test_codec_reset(void)
{
    voice_codec_detail_config_t config;
    memset(&config, 0, sizeof(config));
    config.codec_id = VOICE_CODEC_G711_ULAW;
    voice_g711_config_init(&config.u.g711, false);

    voice_encoder_t *encoder = voice_encoder_create(&config);
    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder creation should succeed");
    TEST_ASSERT_NOT_NULL(decoder, "Decoder creation should succeed");

    /* Process some data */
    int16_t pcm[160];
    uint8_t encoded[160];
    generate_sine_wave(pcm, 160, 440.0f, 8000);

    size_t encoded_size = sizeof(encoded);
    voice_encoder_encode(encoder, pcm, 160, encoded, &encoded_size);

    /* Reset encoder */
    voice_encoder_reset(encoder);

    /* Reset decoder */
    voice_decoder_reset(decoder);

    /* Should still work after reset */
    encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, pcm, 160, encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoding after reset should work");

    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

/* ============================================
 * Main
 * ============================================ */

int main(void)
{
    printf("Codec Tests\n");
    printf("===========\n\n");

    RUN_TEST(test_g711_ulaw_encode_decode);
    RUN_TEST(test_g711_alaw_encode_decode);
    RUN_TEST(test_codec_info);
    RUN_TEST(test_codec_silence);
    RUN_TEST(test_codec_reset);

    TEST_SUMMARY();
}
