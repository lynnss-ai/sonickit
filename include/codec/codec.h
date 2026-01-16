/**
 * @file codec.h
 * @brief Audio codec abstraction interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef CODEC_CODEC_H
#define CODEC_CODEC_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Codec Types - Using types defined in types.h
 * ============================================ */

/** Codec type alias (for backward compatibility) */
typedef voice_codec_type_t voice_codec_id_t;

#define VOICE_CODEC_UNKNOWN VOICE_CODEC_NONE
#define VOICE_CODEC_MAX VOICE_CODEC_COUNT

/* ============================================
 * Codec Handles
 * ============================================ */

typedef struct voice_encoder_s voice_encoder_t;
typedef struct voice_decoder_s voice_decoder_t;

/* ============================================
 * Codec Configuration
 * ============================================ */

/** Opus codec configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate: 8000/12000/16000/24000/48000 */
    uint8_t channels;           /**< Number of channels: 1 or 2 */
    uint32_t bitrate;           /**< Bitrate (bps): 6000-510000 */
    int application;            /**< Application type: VOIP/AUDIO/LOWDELAY */
    int complexity;             /**< Complexity (0-10) */
    bool enable_fec;            /**< Enable FEC */
    int packet_loss_perc;       /**< Expected packet loss percentage (0-100) */
    bool enable_dtx;            /**< Enable DTX */
    bool enable_vbr;            /**< Enable VBR */
    int signal_type;            /**< Signal type: AUTO/VOICE/MUSIC */
} voice_opus_config_t;

/** G.711 codec configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate (must be 8000) */
    bool use_alaw;              /**< true=A-law, false=μ-law */
} voice_g711_config_t;

/** G.722 codec configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate (must be 16000) */
    int bitrate_mode;           /**< Bitrate mode: 0=64kbps, 1=56kbps, 2=48kbps */
} voice_g722_config_t;

/** Detailed codec configuration (codec module specific) */
typedef struct {
    voice_codec_id_t codec_id;
    union {
        voice_opus_config_t opus;
        voice_g711_config_t g711;
        voice_g722_config_t g722;
    } u;
} voice_codec_detail_config_t;

/* ============================================
 * Codec Information
 * ============================================ */

typedef struct {
    voice_codec_id_t codec_id;
    const char *name;           /**< Codec name */
    uint8_t rtp_payload_type;   /**< RTP payload type */
    uint32_t sample_rate;       /**< Sample rate */
    uint8_t channels;           /**< Number of channels */
    uint32_t frame_duration_ms; /**< Frame duration (ms) */
    uint32_t frame_size;        /**< Frame size (samples) */
    uint32_t bitrate;           /**< Bitrate (bps) */
    bool is_vbr;                /**< Is VBR */
} voice_codec_info_t;

/* ============================================
 * Default Configuration Initialization
 * ============================================ */

/**
 * @brief Initialize default Opus configuration
 */
VOICE_API void voice_opus_config_init(voice_opus_config_t *config);

/**
 * @brief Initialize default G.711 configuration
 */
VOICE_API void voice_g711_config_init(voice_g711_config_t *config, bool use_alaw);

/**
 * @brief Initialize default G.722 configuration
 */
VOICE_API void voice_g722_config_init(voice_g722_config_t *config);

/* ============================================
 * Encoder API
 * ============================================ */

/**
 * @brief Create encoder
 * @param config Codec configuration
 * @return Encoder handle
 */
VOICE_API voice_encoder_t *voice_encoder_create(const voice_codec_detail_config_t *config);

/**
 * @brief Destroy encoder
 * @param encoder Encoder handle
 */
VOICE_API void voice_encoder_destroy(voice_encoder_t *encoder);

/**
 * @brief Encode audio frame
 * @param encoder Encoder handle
 * @param pcm_input Input PCM data
 * @param pcm_samples Number of input samples
 * @param output Output buffer
 * @param output_size Output buffer size (input) / actual output bytes (output)
 * @return Error code
 */
VOICE_API voice_error_t voice_encoder_encode(
    voice_encoder_t *encoder,
    const int16_t *pcm_input,
    size_t pcm_samples,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief Reset encoder state
 * @param encoder Encoder handle
 */
VOICE_API void voice_encoder_reset(voice_encoder_t *encoder);

/**
 * @brief Get codec information
 * @param encoder Encoder handle
 * @param info Output info structure
 * @return Error code
 */
VOICE_API voice_error_t voice_encoder_get_info(
    voice_encoder_t *encoder,
    voice_codec_info_t *info
);

/**
 * @brief Set bitrate (Opus only)
 * @param encoder Encoder handle
 * @param bitrate Bitrate (bps)
 * @return Error code
 */
VOICE_API voice_error_t voice_encoder_set_bitrate(
    voice_encoder_t *encoder,
    uint32_t bitrate
);

/**
 * @brief Set packet loss percentage (Opus only)
 * @param encoder Encoder handle
 * @param packet_loss_perc Packet loss percentage (0-100)
 * @return Error code
 */
VOICE_API voice_error_t voice_encoder_set_packet_loss(
    voice_encoder_t *encoder,
    int packet_loss_perc
);

/* ============================================
 * Decoder API
 * ============================================ */

/**
 * @brief Create decoder
 * @param config Codec configuration
 * @return Decoder handle
 */
VOICE_API voice_decoder_t *voice_decoder_create(const voice_codec_detail_config_t *config);

/**
 * @brief Destroy decoder
 * @param decoder Decoder handle
 */
VOICE_API void voice_decoder_destroy(voice_decoder_t *decoder);

/**
 * @brief Decode audio frame
 * @param decoder Decoder handle
 * @param input Encoded data
 * @param input_size Encoded data size in bytes
 * @param pcm_output Output PCM buffer
 * @param pcm_samples Buffer size (input) / output sample count (output)
 * @return Error code
 */
VOICE_API voice_error_t voice_decoder_decode(
    voice_decoder_t *decoder,
    const uint8_t *input,
    size_t input_size,
    int16_t *pcm_output,
    size_t *pcm_samples
);

/**
 * @brief Packet Loss Concealment (PLC)
 * @param decoder Decoder handle
 * @param pcm_output Output PCM buffer
 * @param pcm_samples Requested sample count (input) / output sample count (output)
 * @return Error code
 */
VOICE_API voice_error_t voice_decoder_plc(
    voice_decoder_t *decoder,
    int16_t *pcm_output,
    size_t *pcm_samples
);

/**
 * @brief Reset decoder state
 * @param decoder Decoder handle
 */
VOICE_API void voice_decoder_reset(voice_decoder_t *decoder);

/**
 * @brief Get codec information
 * @param decoder Decoder handle
 * @param info Output info structure
 * @return Error code
 */
VOICE_API voice_error_t voice_decoder_get_info(
    voice_decoder_t *decoder,
    voice_codec_info_t *info
);

/* ============================================
 * Codec Utility Functions
 * ============================================ */

/**
 * @brief Get codec name
 * @param codec_id Codec ID
 * @return Name string
 */
VOICE_API const char *voice_codec_get_name(voice_codec_id_t codec_id);

/**
 * @brief Get RTP payload type
 * @param codec_id Codec ID
 * @return RTP payload type (0-127)
 */
VOICE_API uint8_t voice_codec_get_rtp_payload_type(voice_codec_id_t codec_id);

/**
 * @brief Get codec ID from RTP payload type
 * @param payload_type RTP payload type
 * @return Codec ID
 */
VOICE_API voice_codec_id_t voice_codec_from_rtp_payload_type(uint8_t payload_type);

/**
 * @brief Calculate maximum encoded size in bytes
 * @param codec_id Codec ID
 * @param samples Number of samples
 * @return Maximum bytes
 */
VOICE_API size_t voice_codec_get_max_encoded_size(
    voice_codec_id_t codec_id,
    size_t samples
);

#ifdef __cplusplus
}
#endif

#endif /* CODEC_CODEC_H */
