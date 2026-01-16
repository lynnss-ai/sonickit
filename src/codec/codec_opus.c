/**
 * @file codec_opus.c
 * @brief Opus codec implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "codec/codec.h"
#include "codec/codec_common.h"

#ifdef VOICE_HAVE_OPUS

#include <opus/opus.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * Opus Encoder State
 * ============================================ */

typedef struct {
    OpusEncoder *encoder;
    voice_opus_config_t config;
    uint32_t frame_size;
} opus_encoder_state_t;

/* ============================================
 * Opus Decoder State
 * ============================================ */

typedef struct {
    OpusDecoder *decoder;
    voice_opus_config_t config;
    uint32_t frame_size;
} opus_decoder_state_t;

/* ============================================
 * Opus Encoder Implementation
 * ============================================ */

/**
 * @brief Encode PCM samples to Opus format
 * @details This function compresses PCM audio samples using the Opus codec, which
 *          provides excellent audio quality at low bitrates. Opus is highly flexible,
 *          supporting bitrates from 6 kbps to 510 kbps, sample rates from 8 kHz to
 *          48 kHz, and both speech and music applications. It includes adaptive
 *          features like VBR, FEC, and DTX for optimal performance under varying
 *          network conditions.
 * @param state Pointer to the encoder state (internal use)
 * @param pcm_input Buffer containing 16-bit PCM samples
 * @param pcm_samples Number of PCM samples to encode (typically 20ms worth)
 * @param output Buffer to store encoded Opus packet
 * @param output_size Input: output buffer size; Output: actual encoded bytes
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if encoder state is invalid,
 *         VOICE_ERROR_NULL_POINTER if required pointers are NULL,
 *         VOICE_ERROR_ENCODE_FAILED if encoding fails
 * @note Opus packets are variable-length. The output buffer should be at least
 *       1500 bytes to accommodate maximum packet sizes.
 */
static voice_error_t opus_encode(
    void *state,
    const int16_t *pcm_input,
    size_t pcm_samples,
    uint8_t *output,
    size_t *output_size)
{
    opus_encoder_state_t *enc = (opus_encoder_state_t *)state;

    if (!enc || !enc->encoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!pcm_input || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* Opus encoding */
    int len = opus_encode(
        enc->encoder,
        pcm_input,
        (int)pcm_samples,
        output,
        (opus_int32)*output_size
    );

    if (len < 0) {
        VOICE_LOG_E("Opus encode error: %s", opus_strerror(len));
        return VOICE_ERROR_ENCODE_FAILED;
    }

    *output_size = (size_t)len;
    return VOICE_OK;
}

/**
 * @brief Reset Opus encoder state to initial conditions
 * @details This function clears all internal encoder state, including adaptive
 *          parameters, history buffers, and FEC data. This is useful when starting
 *          a new audio stream or after a long period of silence to ensure clean
 *          encoding without artifacts from previous audio.
 * @param state Pointer to the encoder state (internal use)
 */
static void opus_encoder_reset_impl(void *state)
{
    opus_encoder_state_t *enc = (opus_encoder_state_t *)state;
    if (enc && enc->encoder) {
        opus_encoder_ctl(enc->encoder, OPUS_RESET_STATE);
    }
}

static void opus_encoder_destroy_impl(void *state)
{
    opus_encoder_state_t *enc = (opus_encoder_state_t *)state;
    if (enc) {
        if (enc->encoder) {
            opus_encoder_destroy(enc->encoder);
        }
        free(enc);
    }
}

static voice_error_t opus_encoder_get_info_impl(
    void *state,
    voice_codec_info_t *info)
{
    opus_encoder_state_t *enc = (opus_encoder_state_t *)state;
    if (!enc || !info) {
        return VOICE_ERROR_NULL_POINTER;
    }

    memset(info, 0, sizeof(voice_codec_info_t));
    info->codec_id = VOICE_CODEC_OPUS;
    info->name = "Opus";
    info->rtp_payload_type = 111;
    info->sample_rate = enc->config.sample_rate;
    info->channels = enc->config.channels;
    info->frame_duration_ms = 20;
    info->frame_size = enc->frame_size;
    info->bitrate = enc->config.bitrate;
    info->is_vbr = enc->config.enable_vbr;

    return VOICE_OK;
}

/**
 * @brief Dynamically adjust Opus encoder bitrate
 * @details This function changes the target bitrate at runtime, enabling adaptive
 *          bitrate control for varying network conditions. Opus supports a wide range
 *          from 6 kbps (very low quality) to 510 kbps (transparent quality). Common
 *          values: 24-32 kbps for narrowband speech, 32-48 kbps for wideband speech,
 *          64-128 kbps for fullband speech or music.
 * @param state Pointer to the encoder state (internal use)
 * @param bitrate New target bitrate in bits per second (6000-510000)
 * @return VOICE_OK on success, VOICE_ERROR_INVALID_PARAM if bitrate is out of range
 */
static voice_error_t opus_encoder_set_bitrate_impl(
    void *state,
    uint32_t bitrate)
{
    opus_encoder_state_t *enc = (opus_encoder_state_t *)state;
    if (!enc || !enc->encoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    int ret = opus_encoder_ctl(enc->encoder, OPUS_SET_BITRATE(bitrate));
    if (ret != OPUS_OK) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    enc->config.bitrate = bitrate;
    return VOICE_OK;
}

/**
 * @brief Configure Opus FEC for expected packet loss
 * @details This function sets the expected packet loss percentage, which the encoder
 *          uses to adjust its Forward Error Correction (FEC) redundancy. Higher values
 *          increase resilience to packet loss but also increase bitrate. FEC allows
 *          the decoder to recover lost packets using data from adjacent packets.
 * @param state Pointer to the encoder state (internal use)
 * @param packet_loss_perc Expected packet loss percentage (0-100)
 * @return VOICE_OK on success, VOICE_ERROR_INVALID_PARAM if percentage is invalid
 * @note FEC must be enabled in the encoder configuration for this to have effect.
 *       Typical values: 0-5% for good networks, 10-20% for poor networks.
 */
static voice_error_t opus_encoder_set_packet_loss_impl(
    void *state,
    int packet_loss_perc)
{
    opus_encoder_state_t *enc = (opus_encoder_state_t *)state;
    if (!enc || !enc->encoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    int ret = opus_encoder_ctl(
        enc->encoder,
        OPUS_SET_PACKET_LOSS_PERC(packet_loss_perc)
    );

    if (ret != OPUS_OK) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    enc->config.packet_loss_perc = packet_loss_perc;
    return VOICE_OK;
}

static const encoder_vtable_t g_opus_encoder_vtable = {
    .encode = opus_encode,
    .reset = opus_encoder_reset_impl,
    .destroy = opus_encoder_destroy_impl,
    .get_info = opus_encoder_get_info_impl,
    .set_bitrate = opus_encoder_set_bitrate_impl,
    .set_packet_loss = opus_encoder_set_packet_loss_impl,
};

/**
 * @brief Create an Opus encoder instance
 * @details This function initializes a new Opus encoder with the specified configuration.
 *          Opus is a versatile codec supporting:
 *          - Sample rates: 8, 12, 16, 24, 48 kHz
 *          - Channels: 1 (mono) or 2 (stereo)
 *          - Applications: VOIP (speech optimization), Audio (general), or Restricted (lowest latency)
 *          - Bitrates: 6-510 kbps with VBR or CBR
 *          - Features: FEC for packet loss, DTX for bandwidth saving
 *          The encoder is configured with all settings from the config structure,
 *          including complexity (0-10), VBR/CBR mode, FEC, DTX, and signal type hints.
 * @param config Pointer to Opus configuration structure with all settings
 * @return Pointer to the created encoder instance, or NULL on failure
 * @note The caller must call voice_encoder_destroy() when finished.
 *       Higher complexity values provide better quality but use more CPU.
 */
voice_encoder_t *voice_opus_encoder_create(const voice_opus_config_t *config)
{
    if (!config) {
        return NULL;
    }

    voice_encoder_t *encoder = (voice_encoder_t *)calloc(1, sizeof(voice_encoder_t));
    if (!encoder) {
        return NULL;
    }

    opus_encoder_state_t *enc = (opus_encoder_state_t *)calloc(
        1, sizeof(opus_encoder_state_t));
    if (!enc) {
        free(encoder);
        return NULL;
    }

    enc->config = *config;
    enc->frame_size = config->sample_rate * 20 / 1000;  /* 20ms */

    int error;
    enc->encoder = opus_encoder_create(
        config->sample_rate,
        config->channels,
        config->application,
        &error
    );

    if (error != OPUS_OK || !enc->encoder) {
        VOICE_LOG_E("Failed to create Opus encoder: %s", opus_strerror(error));
        free(enc);
        free(encoder);
        return NULL;
    }

    /* Set encoder parameters */
    opus_encoder_ctl(enc->encoder, OPUS_SET_BITRATE(config->bitrate));
    opus_encoder_ctl(enc->encoder, OPUS_SET_COMPLEXITY(config->complexity));
    opus_encoder_ctl(enc->encoder, OPUS_SET_VBR(config->enable_vbr ? 1 : 0));
    opus_encoder_ctl(enc->encoder, OPUS_SET_DTX(config->enable_dtx ? 1 : 0));
    opus_encoder_ctl(enc->encoder,
        OPUS_SET_INBAND_FEC(config->enable_fec ? 1 : 0));
    opus_encoder_ctl(enc->encoder,
        OPUS_SET_PACKET_LOSS_PERC(config->packet_loss_perc));

    if (config->signal_type != -1000) {
        opus_encoder_ctl(enc->encoder, OPUS_SET_SIGNAL(config->signal_type));
    }

    encoder->codec_id = VOICE_CODEC_OPUS;
    encoder->vtable = &g_opus_encoder_vtable;
    encoder->state = enc;

    VOICE_LOG_I("Opus encoder created: %uHz, %uch, %ubps",
        config->sample_rate, config->channels, config->bitrate);

    return encoder;
}

/* ============================================
 * Opus Decoder Implementation
 * ============================================ */

static voice_error_t opus_decode(
    void *state,
    const uint8_t *input,
    size_t input_size,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    opus_decoder_state_t *dec = (opus_decoder_state_t *)state;

    if (!dec || !dec->decoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!pcm_output || !pcm_samples) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* Opus decoding */
    int samples = opus_decode(
        dec->decoder,
        input,
        (opus_int32)input_size,
        pcm_output,
        (int)*pcm_samples,
        0  /* decode_fec */
    );

    if (samples < 0) {
        VOICE_LOG_E("Opus decode error: %s", opus_strerror(samples));
        return VOICE_ERROR_DECODE_FAILED;
    }

    *pcm_samples = (size_t)samples;
    return VOICE_OK;
}

static voice_error_t opus_plc(
    void *state,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    opus_decoder_state_t *dec = (opus_decoder_state_t *)state;

    if (!dec || !dec->decoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    /* PLC: Pass NULL data to trigger packet loss concealment */
    int samples = opus_decode(
        dec->decoder,
        NULL,
        0,
        pcm_output,
        (int)*pcm_samples,
        0
    );

    if (samples < 0) {
        return VOICE_ERROR_DECODE_FAILED;
    }

    *pcm_samples = (size_t)samples;
    return VOICE_OK;
}

static void opus_decoder_reset_impl(void *state)
{
    opus_decoder_state_t *dec = (opus_decoder_state_t *)state;
    if (dec && dec->decoder) {
        opus_decoder_ctl(dec->decoder, OPUS_RESET_STATE);
    }
}

static void opus_decoder_destroy_impl(void *state)
{
    opus_decoder_state_t *dec = (opus_decoder_state_t *)state;
    if (dec) {
        if (dec->decoder) {
            opus_decoder_destroy(dec->decoder);
        }
        free(dec);
    }
}

static voice_error_t opus_decoder_get_info_impl(
    void *state,
    voice_codec_info_t *info)
{
    opus_decoder_state_t *dec = (opus_decoder_state_t *)state;
    if (!dec || !info) {
        return VOICE_ERROR_NULL_POINTER;
    }

    memset(info, 0, sizeof(voice_codec_info_t));
    info->codec_id = VOICE_CODEC_OPUS;
    info->name = "Opus";
    info->rtp_payload_type = 111;
    info->sample_rate = dec->config.sample_rate;
    info->channels = dec->config.channels;
    info->frame_duration_ms = 20;
    info->frame_size = dec->frame_size;
    info->bitrate = dec->config.bitrate;

    return VOICE_OK;
}

static const decoder_vtable_t g_opus_decoder_vtable = {
    .decode = opus_decode,
    .plc = opus_plc,
    .reset = opus_decoder_reset_impl,
    .destroy = opus_decoder_destroy_impl,
    .get_info = opus_decoder_get_info_impl,
};

voice_decoder_t *voice_opus_decoder_create(const voice_opus_config_t *config)
{
    if (!config) {
        return NULL;
    }

    voice_decoder_t *decoder = (voice_decoder_t *)calloc(1, sizeof(voice_decoder_t));
    if (!decoder) {
        return NULL;
    }

    opus_decoder_state_t *dec = (opus_decoder_state_t *)calloc(
        1, sizeof(opus_decoder_state_t));
    if (!dec) {
        free(decoder);
        return NULL;
    }

    dec->config = *config;
    dec->frame_size = config->sample_rate * 20 / 1000;

    int error;
    dec->decoder = opus_decoder_create(
        config->sample_rate,
        config->channels,
        &error
    );

    if (error != OPUS_OK || !dec->decoder) {
        VOICE_LOG_E("Failed to create Opus decoder: %s", opus_strerror(error));
        free(dec);
        free(decoder);
        return NULL;
    }

    decoder->codec_id = VOICE_CODEC_OPUS;
    decoder->vtable = &g_opus_decoder_vtable;
    decoder->state = dec;

    VOICE_LOG_I("Opus decoder created: %uHz, %uch",
        config->sample_rate, config->channels);

    return decoder;
}

#endif /* VOICE_HAVE_OPUS */
