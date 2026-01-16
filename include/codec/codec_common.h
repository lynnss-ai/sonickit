/**
 * @file codec_common.h
 * @brief Codec internal common definitions
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef CODEC_COMMON_H
#define CODEC_COMMON_H

#include "codec/codec.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Encoder Internal Structures
 * ============================================ */

/** Encoder virtual function table */
typedef struct {
    voice_error_t (*encode)(
        void *state,
        const int16_t *pcm_input,
        size_t pcm_samples,
        uint8_t *output,
        size_t *output_size
    );
    void (*reset)(void *state);
    void (*destroy)(void *state);
    voice_error_t (*get_info)(void *state, voice_codec_info_t *info);
    voice_error_t (*set_bitrate)(void *state, uint32_t bitrate);
    voice_error_t (*set_packet_loss)(void *state, int packet_loss_perc);
} encoder_vtable_t;

/** Decoder virtual function table */
typedef struct {
    voice_error_t (*decode)(
        void *state,
        const uint8_t *input,
        size_t input_size,
        int16_t *pcm_output,
        size_t *pcm_samples
    );
    voice_error_t (*plc)(
        void *state,
        int16_t *pcm_output,
        size_t *pcm_samples
    );
    void (*reset)(void *state);
    void (*destroy)(void *state);
    voice_error_t (*get_info)(void *state, voice_codec_info_t *info);
} decoder_vtable_t;

/* ============================================
 * Codec Base Classes
 * ============================================ */

struct voice_encoder_s {
    voice_codec_id_t codec_id;
    const encoder_vtable_t *vtable;
    void *state;
};

struct voice_decoder_s {
    voice_codec_id_t codec_id;
    const decoder_vtable_t *vtable;
    void *state;
};

/* ============================================
 * Codec Creation Function Declarations
 * ============================================ */

#ifdef VOICE_HAVE_OPUS
VOICE_API voice_encoder_t *voice_opus_encoder_create(const voice_opus_config_t *config);
VOICE_API voice_decoder_t *voice_opus_decoder_create(const voice_opus_config_t *config);
#endif

VOICE_API voice_encoder_t *voice_g711_encoder_create(const voice_g711_config_t *config);
VOICE_API voice_decoder_t *voice_g711_decoder_create(const voice_g711_config_t *config);

#ifdef VOICE_HAVE_G722
VOICE_API voice_encoder_t *voice_g722_encoder_create(const voice_g722_config_t *config);
VOICE_API voice_decoder_t *voice_g722_decoder_create(const voice_g722_config_t *config);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CODEC_COMMON_H */
