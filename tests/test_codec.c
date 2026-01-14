/**
 * @file test_codec.c
 * @brief Codec unit tests (G.711, G.722, Opus)
 */

#include "test_common.h"
#include "codec/codec.h"

/* ============================================
 * G.711 Tests
 * ============================================ */

static int test_g711_ulaw_encode_decode(void)
{
    voice_codec_config_t config;
    voice_codec_config_init(&config);
    config.type = VOICE_CODEC_G711_ULAW;
    config.sample_rate = 8000;
    config.channels = 1;
    
    voice_encoder_t *encoder = voice_encoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "G.711 ulaw encoder creation should succeed");
    
    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(decoder, "G.711 ulaw decoder creation should succeed");
    
    /* Generate test signal */
    int16_t input[160];  /* 20ms @ 8kHz */
    generate_sine_wave(input, 160, 440.0f, 8000);
    
    /* Encode */
    uint8_t encoded[256];
    size_t encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, input, 160, encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoding should succeed");
    TEST_ASSERT_EQ(encoded_size, 160, "G.711 should produce 1 byte per sample");
    
    /* Decode */
    int16_t output[160];
    size_t output_samples = 160;
    err = voice_decoder_decode(decoder, encoded, encoded_size, output, &output_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Decoding should succeed");
    TEST_ASSERT_EQ(output_samples, 160, "Should decode same number of samples");
    
    /* Verify signal is similar (G.711 is lossy) */
    float input_rms = calculate_rms(input, 160);
    float output_rms = calculate_rms(output, 160);
    float ratio = output_rms / input_rms;
    TEST_ASSERT(ratio > 0.8f && ratio < 1.2f, "RMS should be similar after encode/decode");
    
    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

static int test_g711_alaw_encode_decode(void)
{
    voice_codec_config_t config;
    voice_codec_config_init(&config);
    config.type = VOICE_CODEC_G711_ALAW;
    config.sample_rate = 8000;
    config.channels = 1;
    
    voice_encoder_t *encoder = voice_encoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "G.711 alaw encoder creation should succeed");
    
    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(decoder, "G.711 alaw decoder creation should succeed");
    
    int16_t input[160];
    generate_sine_wave(input, 160, 440.0f, 8000);
    
    uint8_t encoded[256];
    size_t encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, input, 160, encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoding should succeed");
    
    int16_t output[160];
    size_t output_samples = 160;
    err = voice_decoder_decode(decoder, encoded, encoded_size, output, &output_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Decoding should succeed");
    
    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

/* ============================================
 * Codec Info Tests
 * ============================================ */

static int test_codec_info(void)
{
    const voice_codec_info_t *info;
    
    /* G.711 ulaw */
    info = voice_codec_get_info(VOICE_CODEC_G711_ULAW);
    TEST_ASSERT_NOT_NULL(info, "G.711 ulaw info should exist");
    TEST_ASSERT_EQ(info->type, VOICE_CODEC_G711_ULAW, "Type should match");
    TEST_ASSERT(info->sample_rates[0] == 8000, "G.711 should support 8kHz");
    
    /* G.711 alaw */
    info = voice_codec_get_info(VOICE_CODEC_G711_ALAW);
    TEST_ASSERT_NOT_NULL(info, "G.711 alaw info should exist");
    
    /* PCM */
    info = voice_codec_get_info(VOICE_CODEC_PCM);
    TEST_ASSERT_NOT_NULL(info, "PCM info should exist");
    
    return TEST_PASSED;
}

static int test_codec_silence(void)
{
    voice_codec_config_t config;
    voice_codec_config_init(&config);
    config.type = VOICE_CODEC_G711_ULAW;
    config.sample_rate = 8000;
    config.channels = 1;
    
    voice_encoder_t *encoder = voice_encoder_create(&config);
    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder should be created");
    TEST_ASSERT_NOT_NULL(decoder, "Decoder should be created");
    
    /* Encode silence */
    int16_t silence[160];
    generate_silence(silence, 160);
    
    uint8_t encoded[256];
    size_t encoded_size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, silence, 160, encoded, &encoded_size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Silence encoding should succeed");
    
    /* Decode and verify it's still silence */
    int16_t output[160];
    size_t output_samples = 160;
    err = voice_decoder_decode(decoder, encoded, encoded_size, output, &output_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Silence decoding should succeed");
    
    float output_rms = calculate_rms(output, 160);
    TEST_ASSERT(output_rms < 100.0f, "Decoded silence should have low RMS");
    
    voice_encoder_destroy(encoder);
    voice_decoder_destroy(decoder);
    return TEST_PASSED;
}

static int test_codec_reset(void)
{
    voice_codec_config_t config;
    voice_codec_config_init(&config);
    config.type = VOICE_CODEC_G711_ULAW;
    config.sample_rate = 8000;
    config.channels = 1;
    
    voice_encoder_t *encoder = voice_encoder_create(&config);
    voice_decoder_t *decoder = voice_decoder_create(&config);
    TEST_ASSERT_NOT_NULL(encoder, "Encoder should be created");
    TEST_ASSERT_NOT_NULL(decoder, "Decoder should be created");
    
    /* Process some data */
    int16_t input[160];
    generate_sine_wave(input, 160, 440.0f, 8000);
    uint8_t encoded[256];
    size_t size = sizeof(encoded);
    voice_encoder_encode(encoder, input, 160, encoded, &size);
    
    /* Reset */
    voice_encoder_reset(encoder);
    voice_decoder_reset(decoder);
    
    /* Should still work after reset */
    size = sizeof(encoded);
    voice_error_t err = voice_encoder_encode(encoder, input, 160, encoded, &size);
    TEST_ASSERT_EQ(err, VOICE_OK, "Encoder should work after reset");
    
    int16_t output[160];
    size_t out_samples = 160;
    err = voice_decoder_decode(decoder, encoded, size, output, &out_samples);
    TEST_ASSERT_EQ(err, VOICE_OK, "Decoder should work after reset");
    
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
