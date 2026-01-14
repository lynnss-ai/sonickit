/**
 * @file codec_g722.c
 * @brief G.722 wideband codec implementation
 * 
 * G.722 is a wideband audio codec (50-7000 Hz) using SB-ADPCM at 64kbps.
 * Sample rate: 16kHz (但RTP中声明为8kHz，历史原因)
 * 
 * 此实现需要 SpanDSP 库或类似的 G.722 编解码器
 */

#include "codec/codec.h"
#include "codec/codec_common.h"

#ifdef VOICE_HAVE_G722

/* 使用 SpanDSP 的 G.722 实现 */
#include <spandsp/telephony.h>
#include <spandsp/g722.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * G.722 编解码器状态
 * ============================================ */

typedef struct {
    g722_encode_state_t *encoder;
    voice_g722_config_t config;
    uint32_t frame_size;
} g722_encoder_state_t;

typedef struct {
    g722_decode_state_t *decoder;
    voice_g722_config_t config;
    uint32_t frame_size;
    int16_t plc_buffer[320];  /* 20ms @ 16kHz for simple PLC */
} g722_decoder_state_t;

/* ============================================
 * G.722 编码器实现
 * ============================================ */

static voice_error_t g722_encode(
    void *state,
    const int16_t *pcm_input,
    size_t pcm_samples,
    uint8_t *output,
    size_t *output_size)
{
    g722_encoder_state_t *enc = (g722_encoder_state_t *)state;
    
    if (!enc || !enc->encoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!pcm_input || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    /* G.722 编码: 2个样本 -> 1字节 */
    size_t max_output = pcm_samples / 2;
    if (*output_size < max_output) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }
    
    int encoded = g722_encode(
        enc->encoder,
        output,
        pcm_input,
        (int)pcm_samples
    );
    
    if (encoded < 0) {
        return VOICE_ERROR_ENCODE_FAILED;
    }
    
    *output_size = (size_t)encoded;
    return VOICE_OK;
}

static void g722_encoder_reset_impl(void *state)
{
    /* SpanDSP 没有提供 reset API，需要重新初始化 */
    (void)state;
}

static void g722_encoder_destroy_impl(void *state)
{
    g722_encoder_state_t *enc = (g722_encoder_state_t *)state;
    if (enc) {
        if (enc->encoder) {
            g722_encode_free(enc->encoder);
        }
        free(enc);
    }
}

static voice_error_t g722_encoder_get_info_impl(
    void *state,
    voice_codec_info_t *info)
{
    g722_encoder_state_t *enc = (g722_encoder_state_t *)state;
    if (!enc || !info) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    memset(info, 0, sizeof(voice_codec_info_t));
    info->codec_id = VOICE_CODEC_G722;
    info->name = "G722";
    info->rtp_payload_type = 9;
    info->sample_rate = 16000;  /* 实际采样率 */
    info->channels = 1;
    info->frame_duration_ms = 20;
    info->frame_size = 320;  /* 20ms @ 16kHz */
    info->bitrate = 64000;   /* 默认模式 */
    info->is_vbr = false;
    
    return VOICE_OK;
}

static const encoder_vtable_t g_g722_encoder_vtable = {
    .encode = g722_encode,
    .reset = g722_encoder_reset_impl,
    .destroy = g722_encoder_destroy_impl,
    .get_info = g722_encoder_get_info_impl,
    .set_bitrate = NULL,
    .set_packet_loss = NULL,
};

voice_encoder_t *voice_g722_encoder_create(const voice_g722_config_t *config)
{
    if (!config) {
        return NULL;
    }
    
    if (config->sample_rate != 16000) {
        VOICE_LOG_E("G.722 only supports 16000Hz sample rate");
        return NULL;
    }
    
    voice_encoder_t *encoder = (voice_encoder_t *)calloc(1, sizeof(voice_encoder_t));
    if (!encoder) {
        return NULL;
    }
    
    g722_encoder_state_t *enc = (g722_encoder_state_t *)calloc(
        1, sizeof(g722_encoder_state_t));
    if (!enc) {
        free(encoder);
        return NULL;
    }
    
    enc->config = *config;
    enc->frame_size = 320;  /* 20ms @ 16kHz */
    
    /* 创建 G.722 编码器 */
    /* bitrate_mode: 0=64kbps, 1=56kbps, 2=48kbps */
    int rate;
    switch (config->bitrate_mode) {
    case 1:
        rate = 56000;
        break;
    case 2:
        rate = 48000;
        break;
    default:
        rate = 64000;
        break;
    }
    
    enc->encoder = g722_encode_init(NULL, rate, 0);
    if (!enc->encoder) {
        VOICE_LOG_E("Failed to create G.722 encoder");
        free(enc);
        free(encoder);
        return NULL;
    }
    
    encoder->codec_id = VOICE_CODEC_G722;
    encoder->vtable = &g_g722_encoder_vtable;
    encoder->state = enc;
    
    VOICE_LOG_I("G.722 encoder created: %dkbps", rate / 1000);
    
    return encoder;
}

/* ============================================
 * G.722 解码器实现
 * ============================================ */

static voice_error_t g722_decode(
    void *state,
    const uint8_t *input,
    size_t input_size,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    g722_decoder_state_t *dec = (g722_decoder_state_t *)state;
    
    if (!dec || !dec->decoder) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!input || !pcm_output || !pcm_samples) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    /* G.722 解码: 1字节 -> 2个样本 */
    size_t max_samples = input_size * 2;
    if (*pcm_samples < max_samples) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }
    
    int decoded = g722_decode(
        dec->decoder,
        pcm_output,
        input,
        (int)input_size
    );
    
    if (decoded < 0) {
        return VOICE_ERROR_DECODE_FAILED;
    }
    
    *pcm_samples = (size_t)decoded;
    
    /* 保存最后一帧用于 PLC */
    if (decoded >= 320) {
        memcpy(dec->plc_buffer, pcm_output + decoded - 320, 
            320 * sizeof(int16_t));
    }
    
    return VOICE_OK;
}

static voice_error_t g722_plc(
    void *state,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    g722_decoder_state_t *dec = (g722_decoder_state_t *)state;
    
    if (!dec) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!pcm_output || !pcm_samples) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    /* 简单 PLC: 重复上一帧并衰减 */
    size_t samples = *pcm_samples > 320 ? 320 : *pcm_samples;
    
    for (size_t i = 0; i < samples; i++) {
        /* 衰减系数随时间增加 */
        int32_t sample = dec->plc_buffer[i] * 3 / 4;
        pcm_output[i] = (int16_t)sample;
        dec->plc_buffer[i] = (int16_t)sample;
    }
    
    *pcm_samples = samples;
    return VOICE_OK;
}

static void g722_decoder_reset_impl(void *state)
{
    g722_decoder_state_t *dec = (g722_decoder_state_t *)state;
    if (dec) {
        memset(dec->plc_buffer, 0, sizeof(dec->plc_buffer));
    }
}

static void g722_decoder_destroy_impl(void *state)
{
    g722_decoder_state_t *dec = (g722_decoder_state_t *)state;
    if (dec) {
        if (dec->decoder) {
            g722_decode_free(dec->decoder);
        }
        free(dec);
    }
}

static voice_error_t g722_decoder_get_info_impl(
    void *state,
    voice_codec_info_t *info)
{
    g722_decoder_state_t *dec = (g722_decoder_state_t *)state;
    if (!dec || !info) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    memset(info, 0, sizeof(voice_codec_info_t));
    info->codec_id = VOICE_CODEC_G722;
    info->name = "G722";
    info->rtp_payload_type = 9;
    info->sample_rate = 16000;
    info->channels = 1;
    info->frame_duration_ms = 20;
    info->frame_size = 320;
    info->bitrate = 64000;
    
    return VOICE_OK;
}

static const decoder_vtable_t g_g722_decoder_vtable = {
    .decode = g722_decode,
    .plc = g722_plc,
    .reset = g722_decoder_reset_impl,
    .destroy = g722_decoder_destroy_impl,
    .get_info = g722_decoder_get_info_impl,
};

voice_decoder_t *voice_g722_decoder_create(const voice_g722_config_t *config)
{
    if (!config) {
        return NULL;
    }
    
    if (config->sample_rate != 16000) {
        VOICE_LOG_E("G.722 only supports 16000Hz sample rate");
        return NULL;
    }
    
    voice_decoder_t *decoder = (voice_decoder_t *)calloc(1, sizeof(voice_decoder_t));
    if (!decoder) {
        return NULL;
    }
    
    g722_decoder_state_t *dec = (g722_decoder_state_t *)calloc(
        1, sizeof(g722_decoder_state_t));
    if (!dec) {
        free(decoder);
        return NULL;
    }
    
    dec->config = *config;
    dec->frame_size = 320;
    
    int rate;
    switch (config->bitrate_mode) {
    case 1:
        rate = 56000;
        break;
    case 2:
        rate = 48000;
        break;
    default:
        rate = 64000;
        break;
    }
    
    dec->decoder = g722_decode_init(NULL, rate, 0);
    if (!dec->decoder) {
        VOICE_LOG_E("Failed to create G.722 decoder");
        free(dec);
        free(decoder);
        return NULL;
    }
    
    decoder->codec_id = VOICE_CODEC_G722;
    decoder->vtable = &g_g722_decoder_vtable;
    decoder->state = dec;
    
    VOICE_LOG_I("G.722 decoder created: %dkbps", rate / 1000);
    
    return decoder;
}

#endif /* VOICE_HAVE_G722 */
