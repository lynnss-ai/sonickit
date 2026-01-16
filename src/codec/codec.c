/**
 * @file codec.c
 * @brief Codec interface implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "codec/codec.h"
#include "codec/codec_common.h"
#include <string.h>
#include <stdlib.h>

/* ============================================
 * Default Configuration Initialization
 * ============================================ */

/**
 * @brief Initialize Opus codec configuration with default values
 * @details This function sets up an Opus encoder configuration with sensible defaults
 *          optimized for VoIP applications. The default settings provide a good balance
 *          between audio quality and bandwidth usage:
 *          - Sample rate: 48kHz (full bandwidth)
 *          - Channels: 1 (mono)
 *          - Bitrate: 32kbps (suitable for voice)
 *          - Application: VOIP mode (optimized for speech)
 *          - Complexity: 5 (medium CPU usage)
 *          - FEC: Enabled (Forward Error Correction for packet loss)
 *          - VBR: Enabled (Variable Bit Rate for better quality)
 *          - DTX: Disabled (Discontinuous Transmission)
 * @param config Pointer to the Opus configuration structure to initialize
 */
void voice_opus_config_init(voice_opus_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(voice_opus_config_t));
    config->sample_rate = 48000;
    config->channels = 1;
    config->bitrate = 32000;         /* 32kbps */
    config->application = 2048;       /* OPUS_APPLICATION_VOIP */
    config->complexity = 5;
    config->enable_fec = true;
    config->packet_loss_perc = 10;
    config->enable_dtx = false;
    config->enable_vbr = true;
    config->signal_type = -1000;      /* OPUS_AUTO */
}

/**
 * @brief Initialize G.711 codec configuration
 * @details G.711 is a simple 8kHz, 64kbps codec used in telephony. This function
 *          initializes the configuration for either A-law (Europe) or μ-law
 *          (North America/Japan) encoding.
 * @param config Pointer to the G.711 configuration structure to initialize
 * @param use_alaw true for A-law encoding, false for μ-law encoding
 */
void voice_g711_config_init(voice_g711_config_t *config, bool use_alaw)
{
    if (!config) return;

    memset(config, 0, sizeof(voice_g711_config_t));
    config->sample_rate = 8000;
    config->use_alaw = use_alaw;
}

/**
 * @brief Initialize G.722 codec configuration
 * @details G.722 is a wideband codec (50-7000 Hz) that operates at 16kHz sample rate
 *          but is declared as 8kHz in RTP for historical reasons. Default bitrate
 *          is 64kbps, with options for 56kbps and 48kbps.
 * @param config Pointer to the G.722 configuration structure to initialize
 */
void voice_g722_config_init(voice_g722_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(voice_g722_config_t));
    config->sample_rate = 16000;
    config->bitrate_mode = 0;         /* 64kbps */
}

/* ============================================
 * Encoder API Implementation
 * ============================================ */

/**
 * @brief Create an audio encoder instance
 * @details This factory function creates an encoder for the specified codec type.
 *          It dispatches to the appropriate codec-specific encoder creation function
 *          based on the codec_id in the configuration. The returned encoder provides
 *          a unified interface for all supported codecs through a vtable.
 * @param config Pointer to detailed codec configuration including codec type and parameters
 * @return Pointer to the created encoder instance, or NULL on failure
 * @note The caller must call voice_encoder_destroy() when finished to free resources.
 *       Supported codecs: OPUS, G.711 (A-law/μ-law), G.722
 */
voice_encoder_t *voice_encoder_create(const voice_codec_detail_config_t *config)
{
    if (!config) {
        return NULL;
    }

    switch (config->codec_id) {
#ifdef VOICE_HAVE_OPUS
    case VOICE_CODEC_OPUS:
        return voice_opus_encoder_create(&config->u.opus);
#endif

    case VOICE_CODEC_G711_ALAW:
    case VOICE_CODEC_G711_ULAW:
        return voice_g711_encoder_create(&config->u.g711);

#ifdef VOICE_HAVE_G722
    case VOICE_CODEC_G722:
        return voice_g722_encoder_create(&config->u.g722);
#endif

    default:
        VOICE_LOG_E("Unsupported codec: %d", config->codec_id);
        return NULL;
    }
}

/**
 * @brief Destroy an encoder instance and free all associated resources
 * @details This function releases all memory and resources allocated by the encoder,
 *          including codec-specific state and internal buffers. After calling this
 *          function, the encoder pointer becomes invalid.
 * @param encoder Pointer to the encoder to destroy (can be NULL)
 */
void voice_encoder_destroy(voice_encoder_t *encoder)
{
    if (!encoder) return;

    if (encoder->vtable && encoder->vtable->destroy) {
        encoder->vtable->destroy(encoder->state);
    }

    free(encoder);
}

/**
 * @brief Encode PCM audio samples
 * @details This function compresses raw 16-bit PCM audio samples into the codec's
 *          encoded format. The encoding process is stateful - internal codec state
 *          is updated with each call. The output buffer must be large enough to hold
 *          the maximum possible encoded size for the given number of input samples.
 * @param encoder Pointer to the encoder instance
 * @param pcm_input Buffer containing 16-bit PCM samples to encode
 * @param pcm_samples Number of PCM samples to encode (not bytes)
 * @param output Buffer to store the encoded data
 * @param output_size Input: size of output buffer; Output: actual encoded bytes
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if encoder is invalid,
 *         VOICE_ERROR_NULL_POINTER if pointers are NULL,
 *         VOICE_ERROR_BUFFER_TOO_SMALL if output buffer is insufficient,
 *         VOICE_ERROR_ENCODE_FAILED if encoding fails
 * @note For frame-based codecs, pcm_samples should match the expected frame size
 */
voice_error_t voice_encoder_encode(
    voice_encoder_t *encoder,
    const int16_t *pcm_input,
    size_t pcm_samples,
    uint8_t *output,
    size_t *output_size)
{
    if (!encoder || !encoder->vtable || !encoder->vtable->encode) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    return encoder->vtable->encode(
        encoder->state,
        pcm_input,
        pcm_samples,
        output,
        output_size
    );
}

/**
 * @brief Reset encoder state to initial conditions
 * @details This function resets the encoder's internal state, clearing any history
 *          or adaptive parameters. This is useful when starting a new encoding session
 *          or after a long silence period. Note that some codecs (like G.711) are
 *          memoryless and don't require resetting.
 * @param encoder Pointer to the encoder instance
 */
void voice_encoder_reset(voice_encoder_t *encoder)
{
    if (!encoder || !encoder->vtable || !encoder->vtable->reset) {
        return;
    }
    encoder->vtable->reset(encoder->state);
}

voice_error_t voice_encoder_get_info(
    voice_encoder_t *encoder,
    voice_codec_info_t *info)
{
    if (!encoder || !encoder->vtable || !encoder->vtable->get_info) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    return encoder->vtable->get_info(encoder->state, info);
}

/**
 * @brief Dynamically change the encoder's target bitrate
 * @details This function adjusts the encoder's bitrate setting at runtime, allowing
 *          for adaptive bitrate control based on network conditions. Not all codecs
 *          support dynamic bitrate changes (e.g., G.711 is fixed at 64kbps).
 * @param encoder Pointer to the encoder instance
 * @param bitrate New target bitrate in bits per second
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if encoder is invalid,
 *         VOICE_ERROR_NOT_SUPPORTED if codec doesn't support bitrate changes,
 *         VOICE_ERROR_INVALID_PARAM if bitrate is out of valid range
 */
voice_error_t voice_encoder_set_bitrate(
    voice_encoder_t *encoder,
    uint32_t bitrate)
{
    if (!encoder || !encoder->vtable) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!encoder->vtable->set_bitrate) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }

    return encoder->vtable->set_bitrate(encoder->state, bitrate);
}

/**
 * @brief Set expected packet loss percentage for FEC optimization
 * @details This function informs the encoder about expected network packet loss,
 *          allowing it to adjust Forward Error Correction (FEC) parameters for better
 *          resilience. Higher values increase redundancy but also bandwidth usage.
 * @param encoder Pointer to the encoder instance
 * @param packet_loss_perc Expected packet loss percentage (0-100)
 * @return VOICE_OK on success,
 *         VOICE_ERROR_NOT_INITIALIZED if encoder is invalid,
 *         VOICE_ERROR_NOT_SUPPORTED if codec doesn't support FEC,
 *         VOICE_ERROR_INVALID_PARAM if percentage is out of range
 * @note Only codecs with FEC support (like Opus) will use this parameter
 */
voice_error_t voice_encoder_set_packet_loss(
    voice_encoder_t *encoder,
    int packet_loss_perc)
{
    if (!encoder || !encoder->vtable) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!encoder->vtable->set_packet_loss) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }

    return encoder->vtable->set_packet_loss(encoder->state, packet_loss_perc);
}

/* ============================================
 * Decoder API Implementation
 * ============================================ */

voice_decoder_t *voice_decoder_create(const voice_codec_detail_config_t *config)
{
    if (!config) {
        return NULL;
    }

    switch (config->codec_id) {
#ifdef VOICE_HAVE_OPUS
    case VOICE_CODEC_OPUS:
        return voice_opus_decoder_create(&config->u.opus);
#endif

    case VOICE_CODEC_G711_ALAW:
    case VOICE_CODEC_G711_ULAW:
        return voice_g711_decoder_create(&config->u.g711);

#ifdef VOICE_HAVE_G722
    case VOICE_CODEC_G722:
        return voice_g722_decoder_create(&config->u.g722);
#endif

    default:
        VOICE_LOG_E("Unsupported codec: %d", config->codec_id);
        return NULL;
    }
}

void voice_decoder_destroy(voice_decoder_t *decoder)
{
    if (!decoder) return;

    if (decoder->vtable && decoder->vtable->destroy) {
        decoder->vtable->destroy(decoder->state);
    }

    free(decoder);
}

voice_error_t voice_decoder_decode(
    voice_decoder_t *decoder,
    const uint8_t *input,
    size_t input_size,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    if (!decoder || !decoder->vtable || !decoder->vtable->decode) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    return decoder->vtable->decode(
        decoder->state,
        input,
        input_size,
        pcm_output,
        pcm_samples
    );
}

voice_error_t voice_decoder_plc(
    voice_decoder_t *decoder,
    int16_t *pcm_output,
    size_t *pcm_samples)
{
    if (!decoder || !decoder->vtable) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!decoder->vtable->plc) {
        return VOICE_ERROR_NOT_SUPPORTED;
    }

    return decoder->vtable->plc(decoder->state, pcm_output, pcm_samples);
}

void voice_decoder_reset(voice_decoder_t *decoder)
{
    if (!decoder || !decoder->vtable || !decoder->vtable->reset) {
        return;
    }
    decoder->vtable->reset(decoder->state);
}

voice_error_t voice_decoder_get_info(
    voice_decoder_t *decoder,
    voice_codec_info_t *info)
{
    if (!decoder || !decoder->vtable || !decoder->vtable->get_info) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    return decoder->vtable->get_info(decoder->state, info);
}

/* ============================================
 * Utility Functions Implementation
 * ============================================ */

static const struct {
    voice_codec_id_t id;
    const char *name;
    uint8_t rtp_pt;
} g_codec_info[] = {
    { VOICE_CODEC_PCM,       "PCM",       96 },   /* Dynamic */
    { VOICE_CODEC_OPUS,      "Opus",      111 },  /* Dynamic */
    { VOICE_CODEC_G711_ALAW, "PCMA",      8 },    /* Static */
    { VOICE_CODEC_G711_ULAW, "PCMU",      0 },    /* Static */
    { VOICE_CODEC_G722,      "G722",      9 },    /* Static */
};

const char *voice_codec_get_name(voice_codec_id_t codec_id)
{
    for (size_t i = 0; i < sizeof(g_codec_info) / sizeof(g_codec_info[0]); i++) {
        if (g_codec_info[i].id == codec_id) {
            return g_codec_info[i].name;
        }
    }
    return "Unknown";
}

uint8_t voice_codec_get_rtp_payload_type(voice_codec_id_t codec_id)
{
    for (size_t i = 0; i < sizeof(g_codec_info) / sizeof(g_codec_info[0]); i++) {
        if (g_codec_info[i].id == codec_id) {
            return g_codec_info[i].rtp_pt;
        }
    }
    return 96;  /* Default dynamic */
}

voice_codec_id_t voice_codec_from_rtp_payload_type(uint8_t payload_type)
{
    for (size_t i = 0; i < sizeof(g_codec_info) / sizeof(g_codec_info[0]); i++) {
        if (g_codec_info[i].rtp_pt == payload_type) {
            return g_codec_info[i].id;
        }
    }
    return VOICE_CODEC_UNKNOWN;
}

size_t voice_codec_get_max_encoded_size(
    voice_codec_id_t codec_id,
    size_t samples)
{
    switch (codec_id) {
    case VOICE_CODEC_PCM:
        return samples * sizeof(int16_t);

    case VOICE_CODEC_OPUS:
        /* Opus max: 1275 bytes for 20ms @ 48kHz */
        return 1500;

    case VOICE_CODEC_G711_ALAW:
    case VOICE_CODEC_G711_ULAW:
        return samples;  /* 1 byte per sample */

    case VOICE_CODEC_G722:
        return samples / 2;  /* 4 bits per sample */

    default:
        return samples * sizeof(int16_t);
    }
}
