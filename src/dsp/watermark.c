/**
 * @file watermark.c
 * @brief Audio watermarking implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Implements spread spectrum watermarking with:
 * - PN (pseudo-noise) sequence generation
 * - Direct sequence spread spectrum (DSSS) embedding
 * - Correlation-based detection
 */

#include "dsp/watermark.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * PN Sequence Generator (LFSR-based)
 * ============================================ */

typedef struct {
    uint32_t state;
    uint32_t polynomial;
} pn_generator_t;

static void pn_init(pn_generator_t *pn, uint32_t seed)
{
    pn->state = seed ? seed : 1;
    /* 32-bit LFSR polynomial: x^32 + x^22 + x^2 + x^1 + 1 */
    pn->polynomial = 0x80200003;
}

static int pn_next_bit(pn_generator_t *pn)
{
    uint32_t lsb = pn->state & 1;
    pn->state >>= 1;
    if (lsb) {
        pn->state ^= pn->polynomial;
    }
    return lsb ? 1 : -1;  /* Returns +1 or -1 for spreading */
}

static float pn_next_chip(pn_generator_t *pn)
{
    return (float)pn_next_bit(pn);
}

/* ============================================
 * Watermark Embedder Implementation
 * ============================================ */

struct voice_watermark_embedder_s {
    voice_watermark_embedder_config_t config;
    
    /* PN generator */
    pn_generator_t pn;
    uint32_t initial_seed;
    
    /* Payload buffer */
    uint8_t payload[VOICE_WATERMARK_MAX_PAYLOAD_SIZE];
    size_t payload_size;
    
    /* Embedding state */
    size_t current_bit;
    size_t current_chip;
    size_t total_bits;
    size_t samples_processed;
    
    /* Chip buffer for one bit */
    float *chip_sequence;
    size_t chips_per_bit;
};

void voice_watermark_embedder_config_init(voice_watermark_embedder_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_watermark_embedder_config_t));
    config->sample_rate = 48000;
    config->algorithm = VOICE_WATERMARK_SPREAD_SPECTRUM;
    config->strength = VOICE_WATERMARK_STRENGTH_MEDIUM;
    config->payload = NULL;
    config->payload_size = 0;
    config->seed = 0x12345678;
    config->embedding_depth = 0.01f;  /* 1% of signal */
    config->chips_per_bit = 1024;
    config->frequency_min = 1000.0f;
    config->frequency_max = 4000.0f;
    config->sync_enabled = true;
}

voice_watermark_embedder_t *voice_watermark_embedder_create(
    const voice_watermark_embedder_config_t *config)
{
    if (!config) return NULL;
    
    voice_watermark_embedder_t *embedder = (voice_watermark_embedder_t *)calloc(
        1, sizeof(voice_watermark_embedder_t));
    if (!embedder) return NULL;
    
    embedder->config = *config;
    embedder->initial_seed = config->seed;
    
    /* Adjust embedding depth based on strength */
    switch (config->strength) {
        case VOICE_WATERMARK_STRENGTH_LOW:
            embedder->config.embedding_depth = 0.005f;
            break;
        case VOICE_WATERMARK_STRENGTH_MEDIUM:
            embedder->config.embedding_depth = 0.015f;
            break;
        case VOICE_WATERMARK_STRENGTH_HIGH:
            embedder->config.embedding_depth = 0.03f;
            break;
    }
    
    /* Initialize PN generator */
    pn_init(&embedder->pn, config->seed);
    
    /* Allocate chip sequence buffer */
    embedder->chips_per_bit = config->chips_per_bit;
    embedder->chip_sequence = (float *)malloc(embedder->chips_per_bit * sizeof(float));
    if (!embedder->chip_sequence) {
        free(embedder);
        return NULL;
    }
    
    /* Copy payload */
    if (config->payload && config->payload_size > 0) {
        size_t size = config->payload_size;
        if (size > VOICE_WATERMARK_MAX_PAYLOAD_SIZE) {
            size = VOICE_WATERMARK_MAX_PAYLOAD_SIZE;
        }
        memcpy(embedder->payload, config->payload, size);
        embedder->payload_size = size;
    }
    
    embedder->total_bits = embedder->payload_size * 8;
    embedder->current_bit = 0;
    embedder->current_chip = 0;
    embedder->samples_processed = 0;
    
    return embedder;
}

void voice_watermark_embedder_destroy(voice_watermark_embedder_t *embedder)
{
    if (!embedder) return;
    if (embedder->chip_sequence) free(embedder->chip_sequence);
    free(embedder);
}

/* Get bit from payload */
static int get_payload_bit(voice_watermark_embedder_t *embedder, size_t bit_index)
{
    if (bit_index >= embedder->total_bits) {
        /* Repeat pattern or return sync marker */
        bit_index = bit_index % embedder->total_bits;
    }
    
    size_t byte_idx = bit_index / 8;
    size_t bit_idx = bit_index % 8;
    
    return (embedder->payload[byte_idx] >> (7 - bit_idx)) & 1;
}

voice_error_t voice_watermark_embed(voice_watermark_embedder_t *embedder,
                                     float *samples, size_t count)
{
    if (!embedder || !samples) return VOICE_ERROR_INVALID_PARAM;
    if (embedder->payload_size == 0) return VOICE_OK;  /* Nothing to embed */
    
    float depth = embedder->config.embedding_depth;
    
    for (size_t i = 0; i < count; i++) {
        /* Generate PN chip for current position */
        float chip = pn_next_chip(&embedder->pn);
        
        /* Get current data bit */
        int data_bit = get_payload_bit(embedder, embedder->current_bit);
        float data_val = data_bit ? 1.0f : -1.0f;
        
        /* Embed: s' = s + d * chip * data */
        float watermark = depth * chip * data_val;
        samples[i] += samples[i] * watermark;
        
        /* Advance to next chip */
        embedder->current_chip++;
        if (embedder->current_chip >= embedder->chips_per_bit) {
            embedder->current_chip = 0;
            embedder->current_bit++;
            if (embedder->current_bit >= embedder->total_bits) {
                embedder->current_bit = 0;  /* Loop */
            }
        }
        
        embedder->samples_processed++;
    }
    
    return VOICE_OK;
}

voice_error_t voice_watermark_embed_int16(voice_watermark_embedder_t *embedder,
                                           int16_t *samples, size_t count)
{
    if (!embedder || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    /* Convert to float, embed, convert back */
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_watermark_embed(embedder, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

voice_error_t voice_watermark_embedder_set_payload(voice_watermark_embedder_t *embedder,
                                                    const uint8_t *payload,
                                                    size_t payload_size)
{
    if (!embedder) return VOICE_ERROR_INVALID_PARAM;
    
    if (payload && payload_size > 0) {
        size_t size = payload_size;
        if (size > VOICE_WATERMARK_MAX_PAYLOAD_SIZE) {
            size = VOICE_WATERMARK_MAX_PAYLOAD_SIZE;
        }
        memcpy(embedder->payload, payload, size);
        embedder->payload_size = size;
        embedder->total_bits = size * 8;
    } else {
        embedder->payload_size = 0;
        embedder->total_bits = 0;
    }
    
    embedder->current_bit = 0;
    embedder->current_chip = 0;
    
    return VOICE_OK;
}

void voice_watermark_embedder_reset(voice_watermark_embedder_t *embedder)
{
    if (!embedder) return;
    pn_init(&embedder->pn, embedder->initial_seed);
    embedder->current_bit = 0;
    embedder->current_chip = 0;
    embedder->samples_processed = 0;
}

size_t voice_watermark_embedder_get_bits_embedded(voice_watermark_embedder_t *embedder)
{
    if (!embedder) return 0;
    return embedder->samples_processed / embedder->chips_per_bit;
}

/* ============================================
 * Watermark Detector Implementation
 * ============================================ */

struct voice_watermark_detector_s {
    voice_watermark_detector_config_t config;
    
    /* PN generator (must match embedder) */
    pn_generator_t pn;
    uint32_t initial_seed;
    
    /* Detection buffer */
    float *buffer;
    size_t buffer_size;
    size_t buffer_pos;
    
    /* Correlation state */
    float *correlation_buffer;
    size_t chips_per_bit;
    
    /* Detection result */
    voice_watermark_result_t result;
    bool detecting;
    
    /* Bit accumulation */
    float *bit_correlations;
    size_t bits_detected;
    size_t max_bits;
};

void voice_watermark_detector_config_init(voice_watermark_detector_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_watermark_detector_config_t));
    config->sample_rate = 48000;
    config->algorithm = VOICE_WATERMARK_SPREAD_SPECTRUM;
    config->seed = 0x12345678;
    config->detection_threshold = 0.5f;
    config->expected_payload_size = 0;
    config->chips_per_bit = 1024;
    config->frequency_min = 1000.0f;
    config->frequency_max = 4000.0f;
    config->buffer_size = 4096;
    config->overlap = 2048;
}

voice_watermark_detector_t *voice_watermark_detector_create(
    const voice_watermark_detector_config_t *config)
{
    if (!config) return NULL;
    
    voice_watermark_detector_t *detector = (voice_watermark_detector_t *)calloc(
        1, sizeof(voice_watermark_detector_t));
    if (!detector) return NULL;
    
    detector->config = *config;
    detector->initial_seed = config->seed;
    detector->chips_per_bit = config->chips_per_bit;
    
    /* Initialize PN generator */
    pn_init(&detector->pn, config->seed);
    
    /* Allocate buffers */
    detector->buffer_size = config->buffer_size;
    detector->buffer = (float *)calloc(detector->buffer_size, sizeof(float));
    detector->correlation_buffer = (float *)calloc(config->chips_per_bit, sizeof(float));
    
    /* Max bits we can detect */
    detector->max_bits = VOICE_WATERMARK_MAX_PAYLOAD_SIZE * 8;
    detector->bit_correlations = (float *)calloc(detector->max_bits, sizeof(float));
    
    if (!detector->buffer || !detector->correlation_buffer || !detector->bit_correlations) {
        if (detector->buffer) free(detector->buffer);
        if (detector->correlation_buffer) free(detector->correlation_buffer);
        if (detector->bit_correlations) free(detector->bit_correlations);
        free(detector);
        return NULL;
    }
    
    detector->buffer_pos = 0;
    detector->detecting = false;
    detector->bits_detected = 0;
    
    return detector;
}

void voice_watermark_detector_destroy(voice_watermark_detector_t *detector)
{
    if (!detector) return;
    if (detector->buffer) free(detector->buffer);
    if (detector->correlation_buffer) free(detector->correlation_buffer);
    if (detector->bit_correlations) free(detector->bit_correlations);
    free(detector);
}

voice_error_t voice_watermark_detect(voice_watermark_detector_t *detector,
                                      const float *samples, size_t count,
                                      voice_watermark_result_t *result)
{
    if (!detector || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    /* Add samples to buffer */
    for (size_t i = 0; i < count; i++) {
        detector->buffer[detector->buffer_pos] = samples[i];
        detector->buffer_pos++;
        
        /* Process when we have enough for one bit */
        if (detector->buffer_pos >= detector->chips_per_bit) {
            /* Correlate with PN sequence */
            float correlation = 0.0f;
            float signal_power = 0.0f;
            
            pn_generator_t pn_copy;
            pn_init(&pn_copy, detector->initial_seed);
            
            /* Skip to current bit position */
            for (size_t b = 0; b < detector->bits_detected * detector->chips_per_bit; b++) {
                pn_next_bit(&pn_copy);
            }
            
            /* Correlate */
            for (size_t j = 0; j < detector->chips_per_bit; j++) {
                float chip = pn_next_chip(&pn_copy);
                float sample = detector->buffer[j];
                correlation += sample * chip;
                signal_power += sample * sample;
            }
            
            correlation /= detector->chips_per_bit;
            signal_power = sqrtf(signal_power / detector->chips_per_bit);
            
            /* Normalize */
            if (signal_power > 0.0001f) {
                correlation /= signal_power;
            }
            
            /* Store bit correlation */
            if (detector->bits_detected < detector->max_bits) {
                detector->bit_correlations[detector->bits_detected] = correlation;
                detector->bits_detected++;
            }
            
            /* Shift buffer */
            size_t overlap = detector->config.overlap;
            if (overlap > 0 && overlap < detector->buffer_pos) {
                memmove(detector->buffer, 
                       detector->buffer + detector->buffer_pos - overlap,
                       overlap * sizeof(float));
                detector->buffer_pos = overlap;
            } else {
                detector->buffer_pos = 0;
            }
        }
    }
    
    /* Check if we have enough bits for detection */
    size_t expected_bits = detector->config.expected_payload_size * 8;
    if (expected_bits == 0) expected_bits = 64;  /* Default minimum */
    
    if (detector->bits_detected >= expected_bits) {
        /* Decode bits from correlations */
        float avg_confidence = 0.0f;
        
        memset(detector->result.payload, 0, sizeof(detector->result.payload));
        
        for (size_t b = 0; b < expected_bits && b < detector->bits_detected; b++) {
            float corr = detector->bit_correlations[b];
            int bit = (corr > 0) ? 1 : 0;
            
            size_t byte_idx = b / 8;
            size_t bit_idx = b % 8;
            
            if (bit) {
                detector->result.payload[byte_idx] |= (1 << (7 - bit_idx));
            }
            
            avg_confidence += fabsf(corr);
        }
        
        avg_confidence /= expected_bits;
        
        detector->result.payload_size = (expected_bits + 7) / 8;
        detector->result.confidence = avg_confidence;
        detector->result.detected = (avg_confidence > detector->config.detection_threshold);
        detector->result.correlation = avg_confidence;
        detector->result.snr_estimate_db = 20.0f * log10f(avg_confidence + 0.0001f);
        
        detector->detecting = detector->result.detected;
    }
    
    if (result) {
        *result = detector->result;
    }
    
    return VOICE_OK;
}

voice_error_t voice_watermark_detect_int16(voice_watermark_detector_t *detector,
                                            const int16_t *samples, size_t count,
                                            voice_watermark_result_t *result)
{
    if (!detector || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_watermark_detect(detector, temp, count, result);
    
    free(temp);
    return err;
}

voice_error_t voice_watermark_detector_get_result(voice_watermark_detector_t *detector,
                                                   voice_watermark_result_t *result)
{
    if (!detector || !result) return VOICE_ERROR_INVALID_PARAM;
    *result = detector->result;
    return VOICE_OK;
}

void voice_watermark_detector_reset(voice_watermark_detector_t *detector)
{
    if (!detector) return;
    
    pn_init(&detector->pn, detector->initial_seed);
    memset(detector->buffer, 0, detector->buffer_size * sizeof(float));
    memset(detector->bit_correlations, 0, detector->max_bits * sizeof(float));
    detector->buffer_pos = 0;
    detector->bits_detected = 0;
    detector->detecting = false;
    memset(&detector->result, 0, sizeof(voice_watermark_result_t));
}

bool voice_watermark_detector_is_detecting(voice_watermark_detector_t *detector)
{
    if (!detector) return false;
    return detector->detecting;
}

/* ============================================
 * Utility Functions
 * ============================================ */

voice_error_t voice_watermark_quick_embed(float *samples, size_t count,
                                           uint32_t sample_rate,
                                           const uint8_t *payload, size_t payload_size,
                                           uint32_t seed)
{
    voice_watermark_embedder_config_t config;
    voice_watermark_embedder_config_init(&config);
    config.sample_rate = sample_rate;
    config.payload = payload;
    config.payload_size = payload_size;
    config.seed = seed;
    
    voice_watermark_embedder_t *embedder = voice_watermark_embedder_create(&config);
    if (!embedder) return VOICE_ERROR_OUT_OF_MEMORY;
    
    voice_error_t err = voice_watermark_embed(embedder, samples, count);
    voice_watermark_embedder_destroy(embedder);
    
    return err;
}

voice_error_t voice_watermark_quick_detect(const float *samples, size_t count,
                                            uint32_t sample_rate,
                                            uint32_t seed,
                                            voice_watermark_result_t *result)
{
    voice_watermark_detector_config_t config;
    voice_watermark_detector_config_init(&config);
    config.sample_rate = sample_rate;
    config.seed = seed;
    
    voice_watermark_detector_t *detector = voice_watermark_detector_create(&config);
    if (!detector) return VOICE_ERROR_OUT_OF_MEMORY;
    
    voice_error_t err = voice_watermark_detect(detector, samples, count, result);
    voice_watermark_detector_destroy(detector);
    
    return err;
}

size_t voice_watermark_min_samples_for_payload(size_t payload_size,
                                                uint32_t sample_rate,
                                                uint32_t chips_per_bit)
{
    (void)sample_rate;  /* Not directly used in this simple calculation */
    size_t total_bits = payload_size * 8;
    return total_bits * chips_per_bit;
}

float voice_watermark_estimate_snr(const float *original, const float *watermarked,
                                    size_t count)
{
    if (!original || !watermarked || count == 0) return 0.0f;
    
    float signal_power = 0.0f;
    float watermark_power = 0.0f;
    
    for (size_t i = 0; i < count; i++) {
        signal_power += original[i] * original[i];
        float diff = watermarked[i] - original[i];
        watermark_power += diff * diff;
    }
    
    signal_power /= count;
    watermark_power /= count;
    
    if (watermark_power < 0.0000001f) return 100.0f;  /* Effectively no watermark */
    
    return 10.0f * log10f(signal_power / watermark_power);
}

const char *voice_watermark_algorithm_to_string(voice_watermark_algorithm_t algorithm)
{
    switch (algorithm) {
        case VOICE_WATERMARK_SPREAD_SPECTRUM: return "Spread Spectrum";
        case VOICE_WATERMARK_ECHO_HIDING: return "Echo Hiding";
        case VOICE_WATERMARK_PHASE_CODING: return "Phase Coding";
        case VOICE_WATERMARK_QUANTIZATION: return "Quantization Index Modulation";
        default: return "Unknown";
    }
}
