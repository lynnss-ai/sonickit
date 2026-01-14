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
 * Opus 编码器状态
 * ============================================ */

typedef struct {
    OpusEncoder *encoder;
    voice_opus_config_t config;
    uint32_t frame_size;
} opus_encoder_state_t;

/* ============================================
 * Opus 解码器状态
 * ============================================ */

typedef struct {
    OpusDecoder *decoder;
    voice_opus_config_t config;
    uint32_t frame_size;
} opus_decoder_state_t;

/* ============================================
 * Opus 编码器实现
 * ============================================ */

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
    
    /* Opus 编码 */
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
    
    /* 设置编码器参数 */
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
 * Opus 解码器实现
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
    
    /* Opus 解码 */
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
    
    /* PLC: 传入NULL数据 */
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
