/**
 * @file codec_g711.c
 * @brief G.711 A-law/μ-law codec implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * G.711 is a simple logarithmic companding codec used in telephony.
 * - A-law: Used primarily in Europe (ITU-T G.711)
 * - μ-law: Used primarily in North America and Japan
 * 
 * Both provide 8-bit samples at 8kHz = 64kbps
 */

#include "codec/codec.h"
#include "codec/codec_common.h"
#include <stdlib.h>
#include <string.h>

/* ============================================
 * G.711 编码/解码表
 * ============================================ */

/* A-law 编码表 */
static const int16_t g_alaw_decode_table[256] = {
    -5504, -5248, -6016, -5760, -4480, -4224, -4992, -4736,
    -7552, -7296, -8064, -7808, -6528, -6272, -7040, -6784,
    -2752, -2624, -3008, -2880, -2240, -2112, -2496, -2368,
    -3776, -3648, -4032, -3904, -3264, -3136, -3520, -3392,
    -22016,-20992,-24064,-23040,-17920,-16896,-19968,-18944,
    -30208,-29184,-32256,-31232,-26112,-25088,-28160,-27136,
    -11008,-10496,-12032,-11520,-8960, -8448, -9984, -9472,
    -15104,-14592,-16128,-15616,-13056,-12544,-14080,-13568,
    -344,  -328,  -376,  -360,  -280,  -264,  -312,  -296,
    -472,  -456,  -504,  -488,  -408,  -392,  -440,  -424,
    -88,   -72,   -120,  -104,  -24,   -8,    -56,   -40,
    -216,  -200,  -248,  -232,  -152,  -136,  -184,  -168,
    -1376, -1312, -1504, -1440, -1120, -1056, -1248, -1184,
    -1888, -1824, -2016, -1952, -1632, -1568, -1760, -1696,
    -688,  -656,  -752,  -720,  -560,  -528,  -624,  -592,
    -944,  -912,  -1008, -976,  -816,  -784,  -880,  -848,
    5504,  5248,  6016,  5760,  4480,  4224,  4992,  4736,
    7552,  7296,  8064,  7808,  6528,  6272,  7040,  6784,
    2752,  2624,  3008,  2880,  2240,  2112,  2496,  2368,
    3776,  3648,  4032,  3904,  3264,  3136,  3520,  3392,
    22016, 20992, 24064, 23040, 17920, 16896, 19968, 18944,
    30208, 29184, 32256, 31232, 26112, 25088, 28160, 27136,
    11008, 10496, 12032, 11520, 8960,  8448,  9984,  9472,
    15104, 14592, 16128, 15616, 13056, 12544, 14080, 13568,
    344,   328,   376,   360,   280,   264,   312,   296,
    472,   456,   504,   488,   408,   392,   440,   424,
    88,    72,    120,   104,   24,    8,     56,    40,
    216,   200,   248,   232,   152,   136,   184,   168,
    1376,  1312,  1504,  1440,  1120,  1056,  1248,  1184,
    1888,  1824,  2016,  1952,  1632,  1568,  1760,  1696,
    688,   656,   752,   720,   560,   528,   624,   592,
    944,   912,   1008,  976,   816,   784,   880,   848
};

/* μ-law 编码表 */
static const int16_t g_ulaw_decode_table[256] = {
    -32124,-31100,-30076,-29052,-28028,-27004,-25980,-24956,
    -23932,-22908,-21884,-20860,-19836,-18812,-17788,-16764,
    -15996,-15484,-14972,-14460,-13948,-13436,-12924,-12412,
    -11900,-11388,-10876,-10364,-9852, -9340, -8828, -8316,
    -7932, -7676, -7420, -7164, -6908, -6652, -6396, -6140,
    -5884, -5628, -5372, -5116, -4860, -4604, -4348, -4092,
    -3900, -3772, -3644, -3516, -3388, -3260, -3132, -3004,
    -2876, -2748, -2620, -2492, -2364, -2236, -2108, -1980,
    -1884, -1820, -1756, -1692, -1628, -1564, -1500, -1436,
    -1372, -1308, -1244, -1180, -1116, -1052, -988,  -924,
    -876,  -844,  -812,  -780,  -748,  -716,  -684,  -652,
    -620,  -588,  -556,  -524,  -492,  -460,  -428,  -396,
    -372,  -356,  -340,  -324,  -308,  -292,  -276,  -260,
    -244,  -228,  -212,  -196,  -180,  -164,  -148,  -132,
    -120,  -112,  -104,  -96,   -88,   -80,   -72,   -64,
    -56,   -48,   -40,   -32,   -24,   -16,   -8,    0,
    32124, 31100, 30076, 29052, 28028, 27004, 25980, 24956,
    23932, 22908, 21884, 20860, 19836, 18812, 17788, 16764,
    15996, 15484, 14972, 14460, 13948, 13436, 12924, 12412,
    11900, 11388, 10876, 10364, 9852,  9340,  8828,  8316,
    7932,  7676,  7420,  7164,  6908,  6652,  6396,  6140,
    5884,  5628,  5372,  5116,  4860,  4604,  4348,  4092,
    3900,  3772,  3644,  3516,  3388,  3260,  3132,  3004,
    2876,  2748,  2620,  2492,  2364,  2236,  2108,  1980,
    1884,  1820,  1756,  1692,  1628,  1564,  1500,  1436,
    1372,  1308,  1244,  1180,  1116,  1052,  988,   924,
    876,   844,   812,   780,   748,   716,   684,   652,
    620,   588,   556,   524,   492,   460,   428,   396,
    372,   356,   340,   324,   308,   292,   276,   260,
    244,   228,   212,   196,   180,   164,   148,   132,
    120,   112,   104,   96,    88,    80,    72,    64,
    56,    48,    40,    32,    24,    16,    8,     0
};

/* ============================================
 * G.711 编码函数
 * ============================================ */

/**
 * @brief Linear to A-law encoding
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
static uint8_t linear_to_alaw(int16_t pcm)
{
    int sign;
    int exponent;
    int mantissa;
    uint8_t alaw;
    
    sign = ((~pcm) >> 8) & 0x80;
    if (!sign) {
        pcm = -pcm;
    }
    if (pcm > 32635) {
        pcm = 32635;
    }
    
    if (pcm >= 256) {
        exponent = 7;
        for (mantissa = pcm >> 8; mantissa < 0x10; mantissa <<= 1) {
            exponent--;
        }
        mantissa = (pcm >> (exponent + 3)) & 0x0F;
    } else {
        exponent = 0;
        mantissa = pcm >> 4;
    }
    
    alaw = (uint8_t)(sign | (exponent << 4) | mantissa);
    return alaw ^ 0xD5;
}

/**
 * @brief Linear to μ-law encoding
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
static uint8_t linear_to_ulaw(int16_t pcm)
{
    int sign;
    int exponent;
    int mantissa;
    uint8_t ulaw;
    
    /* Get sign and make positive */
    sign = (pcm >> 8) & 0x80;
    if (sign) {
        pcm = -pcm;
    }
    
    /* Clip to 14 bits */
    if (pcm > 32635) {
        pcm = 32635;
    }
    
    /* Add bias */
    pcm += 0x84;
    
    /* Find exponent */
    exponent = 7;
    for (mantissa = pcm >> 7; mantissa < 0x10 && exponent > 0; mantissa <<= 1) {
        exponent--;
    }
    
    /* Get mantissa */
    mantissa = (pcm >> (exponent + 3)) & 0x0F;
    
    /* Combine and complement */
    ulaw = (uint8_t)(sign | (exponent << 4) | mantissa);
    return ~ulaw;
}

/* ============================================
 * G.711 编解码器状态
 * ============================================ */

typedef struct {
    voice_g711_config_t config;
    uint32_t frame_size;
} g711_state_t;

/* ============================================
 * G.711 编码器实现
 * ============================================ */

static voice_error_t g711_encode(
    void *state,
    const int16_t *pcm_input,
    size_t pcm_samples,
    uint8_t *output,
    size_t *output_size)
{
    g711_state_t *s = (g711_state_t *)state;
    
    if (!s) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!pcm_input || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    if (*output_size < pcm_samples) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }
    
    if (s->config.use_alaw) {
        for (size_t i = 0; i < pcm_samples; i++) {
            output[i] = linear_to_alaw(pcm_input[i]);
        }
    } else {
        for (size_t i = 0; i < pcm_samples; i++) {
            output[i] = linear_to_ulaw(pcm_input[i]);
        }
    }
    
    *output_size = pcm_samples;
    return VOICE_OK;
}

static void g711_encoder_reset_impl(void *state)
{
    /* G.711 is memoryless, nothing to reset */
    (void)state;
}

static void g711_encoder_destroy_impl(void *state)
{
    free(state);
}

static voice_error_t g711_encoder_get_info_impl(
    void *state,
    voice_codec_info_t *info)
{
    g711_state_t *s = (g711_state_t *)state;
    if (!s || !info) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    memset(info, 0, sizeof(voice_codec_info_t));
    info->codec_id = s->config.use_alaw ? VOICE_CODEC_G711_ALAW : VOICE_CODEC_G711_ULAW;
    info->name = s->config.use_alaw ? "PCMA" : "PCMU";
    info->rtp_payload_type = s->config.use_alaw ? 8 : 0;
    info->sample_rate = 8000;
    info->channels = 1;
    info->frame_duration_ms = 20;
    info->frame_size = 160;  /* 20ms @ 8kHz */
    info->bitrate = 64000;
    info->is_vbr = false;
    
    return VOICE_OK;
}

static const encoder_vtable_t g_g711_encoder_vtable = {
    .encode = g711_encode,
    .reset = g711_encoder_reset_impl,
    .destroy = g711_encoder_destroy_impl,
    .get_info = g711_encoder_get_info_impl,
    .set_bitrate = NULL,  /* Not supported */
    .set_packet_loss = NULL,
};

voice_encoder_t *voice_g711_encoder_create(const voice_g711_config_t *config)
{
    if (!config) {
        return NULL;
    }
    
    /* G.711 only supports 8kHz */
    if (config->sample_rate != 8000) {
        VOICE_LOG_E("G.711 only supports 8000Hz sample rate");
        return NULL;
    }
    
    voice_encoder_t *encoder = (voice_encoder_t *)calloc(1, sizeof(voice_encoder_t));
    if (!encoder) {
        return NULL;
    }
    
    g711_state_t *s = (g711_state_t *)calloc(1, sizeof(g711_state_t));
    if (!s) {
        free(encoder);
        return NULL;
    }
    
    s->config = *config;
    s->frame_size = 160;  /* 20ms @ 8kHz */
    
    encoder->codec_id = config->use_alaw ? VOICE_CODEC_G711_ALAW : VOICE_CODEC_G711_ULAW;
    encoder->vtable = &g_g711_encoder_vtable;
    encoder->state = s;
    
    VOICE_LOG_I("G.711 %s encoder created", config->use_alaw ? "A-law" : "μ-law");
    
    return encoder;
}

/* ============================================
 * G.711 解码器实现
 * ============================================ */

static voice_error_t g711_decode(
    void *state,
    const uint8_t *input,
    size_t input_size,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    g711_state_t *s = (g711_state_t *)state;
    
    if (!s) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (!input || !pcm_output || !pcm_samples) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    if (*pcm_samples < input_size) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }
    
    const int16_t *decode_table = s->config.use_alaw ? 
        g_alaw_decode_table : g_ulaw_decode_table;
    
    for (size_t i = 0; i < input_size; i++) {
        pcm_output[i] = decode_table[input[i]];
    }
    
    *pcm_samples = input_size;
    return VOICE_OK;
}

static voice_error_t g711_plc(
    void *state,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    /* G.711 has no built-in PLC, just output silence */
    (void)state;
    
    if (!pcm_output || !pcm_samples) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    memset(pcm_output, 0, *pcm_samples * sizeof(int16_t));
    return VOICE_OK;
}

static void g711_decoder_reset_impl(void *state)
{
    (void)state;
}

static void g711_decoder_destroy_impl(void *state)
{
    free(state);
}

static voice_error_t g711_decoder_get_info_impl(
    void *state,
    voice_codec_info_t *info)
{
    g711_state_t *s = (g711_state_t *)state;
    if (!s || !info) {
        return VOICE_ERROR_NULL_POINTER;
    }
    
    memset(info, 0, sizeof(voice_codec_info_t));
    info->codec_id = s->config.use_alaw ? VOICE_CODEC_G711_ALAW : VOICE_CODEC_G711_ULAW;
    info->name = s->config.use_alaw ? "PCMA" : "PCMU";
    info->rtp_payload_type = s->config.use_alaw ? 8 : 0;
    info->sample_rate = 8000;
    info->channels = 1;
    info->frame_duration_ms = 20;
    info->frame_size = 160;
    info->bitrate = 64000;
    
    return VOICE_OK;
}

static const decoder_vtable_t g_g711_decoder_vtable = {
    .decode = g711_decode,
    .plc = g711_plc,
    .reset = g711_decoder_reset_impl,
    .destroy = g711_decoder_destroy_impl,
    .get_info = g711_decoder_get_info_impl,
};

voice_decoder_t *voice_g711_decoder_create(const voice_g711_config_t *config)
{
    if (!config) {
        return NULL;
    }
    
    if (config->sample_rate != 8000) {
        VOICE_LOG_E("G.711 only supports 8000Hz sample rate");
        return NULL;
    }
    
    voice_decoder_t *decoder = (voice_decoder_t *)calloc(1, sizeof(voice_decoder_t));
    if (!decoder) {
        return NULL;
    }
    
    g711_state_t *s = (g711_state_t *)calloc(1, sizeof(g711_state_t));
    if (!s) {
        free(decoder);
        return NULL;
    }
    
    s->config = *config;
    s->frame_size = 160;
    
    decoder->codec_id = config->use_alaw ? VOICE_CODEC_G711_ALAW : VOICE_CODEC_G711_ULAW;
    decoder->vtable = &g_g711_decoder_vtable;
    decoder->state = s;
    
    VOICE_LOG_I("G.711 %s decoder created", config->use_alaw ? "A-law" : "μ-law");
    
    return decoder;
}
