/**
 * @file watermark.h
 * @brief Audio watermarking module
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Provides audio watermarking capabilities for:
 * - Invisible watermark embedding using spread spectrum techniques
 * - Watermark detection and extraction
 * - Robustness against common audio processing
 * - Support for different payload sizes
 */

#ifndef VOICE_WATERMARK_H
#define VOICE_WATERMARK_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Constants and Types
 * ============================================ */

/**
 * @brief Maximum watermark payload size in bytes
 */
#define VOICE_WATERMARK_MAX_PAYLOAD_SIZE 256

/**
 * @brief Watermark algorithm type
 */
typedef enum {
    VOICE_WATERMARK_SPREAD_SPECTRUM,  /**< Spread spectrum embedding */
    VOICE_WATERMARK_ECHO_HIDING,      /**< Echo-based hiding */
    VOICE_WATERMARK_PHASE_CODING,     /**< Phase coding method */
    VOICE_WATERMARK_QUANTIZATION      /**< Quantization index modulation */
} voice_watermark_algorithm_t;

/**
 * @brief Watermark strength/robustness level
 */
typedef enum {
    VOICE_WATERMARK_STRENGTH_LOW,     /**< Minimal embedding, high quality */
    VOICE_WATERMARK_STRENGTH_MEDIUM,  /**< Balanced embedding */
    VOICE_WATERMARK_STRENGTH_HIGH     /**< Strong embedding, more audible */
} voice_watermark_strength_t;

/**
 * @brief Watermark detection result
 */
typedef struct {
    bool detected;                    /**< Whether watermark was detected */
    float confidence;                 /**< Detection confidence (0-1) */
    uint8_t payload[VOICE_WATERMARK_MAX_PAYLOAD_SIZE];  /**< Extracted payload */
    size_t payload_size;              /**< Size of extracted payload */
    float correlation;                /**< Correlation value */
    float snr_estimate_db;            /**< Estimated watermark SNR */
} voice_watermark_result_t;

/* ============================================
 * Watermark Embedder
 * ============================================ */

/**
 * @brief Watermark embedder configuration
 */
typedef struct {
    uint32_t sample_rate;             /**< Sample rate */
    voice_watermark_algorithm_t algorithm;  /**< Embedding algorithm */
    voice_watermark_strength_t strength;    /**< Embedding strength */

    /* Payload */
    const uint8_t *payload;           /**< Watermark payload data */
    size_t payload_size;              /**< Payload size in bytes */

    /* Advanced settings */
    uint32_t seed;                    /**< PN sequence seed (secret key) */
    float embedding_depth;            /**< Embedding depth (0.001 - 0.1) */
    uint32_t chips_per_bit;           /**< Chips per bit for spread spectrum */
    float frequency_min;              /**< Minimum embedding frequency (Hz) */
    float frequency_max;              /**< Maximum embedding frequency (Hz) */
    bool sync_enabled;                /**< Enable synchronization markers */
} voice_watermark_embedder_config_t;

typedef struct voice_watermark_embedder_s voice_watermark_embedder_t;

/**
 * @brief Initialize embedder configuration with defaults
 */
VOICE_API void voice_watermark_embedder_config_init(voice_watermark_embedder_config_t *config);

/**
 * @brief Create watermark embedder
 */
VOICE_API voice_watermark_embedder_t *voice_watermark_embedder_create(
    const voice_watermark_embedder_config_t *config);

/**
 * @brief Destroy watermark embedder
 */
VOICE_API void voice_watermark_embedder_destroy(voice_watermark_embedder_t *embedder);

/**
 * @brief Embed watermark into audio samples (float)
 * @param embedder Embedder instance
 * @param samples Audio samples (modified in place)
 * @param count Number of samples
 */
VOICE_API voice_error_t voice_watermark_embed(voice_watermark_embedder_t *embedder,
                                     float *samples, size_t count);

/**
 * @brief Embed watermark into audio samples (int16)
 */
VOICE_API voice_error_t voice_watermark_embed_int16(voice_watermark_embedder_t *embedder,
                                           int16_t *samples, size_t count);

/**
 * @brief Set new payload for embedding
 */
VOICE_API voice_error_t voice_watermark_embedder_set_payload(voice_watermark_embedder_t *embedder,
                                                    const uint8_t *payload,
                                                    size_t payload_size);

/**
 * @brief Reset embedder state (for new audio stream)
 */
VOICE_API void voice_watermark_embedder_reset(voice_watermark_embedder_t *embedder);

/**
 * @brief Get bits embedded so far
 */
VOICE_API size_t voice_watermark_embedder_get_bits_embedded(voice_watermark_embedder_t *embedder);

/* ============================================
 * Watermark Detector
 * ============================================ */

/**
 * @brief Watermark detector configuration
 */
typedef struct {
    uint32_t sample_rate;             /**< Sample rate */
    voice_watermark_algorithm_t algorithm;  /**< Detection algorithm */

    /* Detection settings */
    uint32_t seed;                    /**< PN sequence seed (must match embedder) */
    float detection_threshold;        /**< Detection threshold (0.3 - 0.9) */
    size_t expected_payload_size;     /**< Expected payload size (0 = auto) */
    uint32_t chips_per_bit;           /**< Chips per bit (must match embedder) */
    float frequency_min;              /**< Minimum frequency to search */
    float frequency_max;              /**< Maximum frequency to search */

    /* Buffer settings */
    size_t buffer_size;               /**< Analysis buffer size */
    size_t overlap;                   /**< Buffer overlap for detection */
} voice_watermark_detector_config_t;

typedef struct voice_watermark_detector_s voice_watermark_detector_t;

/**
 * @brief Initialize detector configuration with defaults
 */
VOICE_API void voice_watermark_detector_config_init(voice_watermark_detector_config_t *config);

/**
 * @brief Create watermark detector
 */
VOICE_API voice_watermark_detector_t *voice_watermark_detector_create(
    const voice_watermark_detector_config_t *config);

/**
 * @brief Destroy watermark detector
 */
VOICE_API void voice_watermark_detector_destroy(voice_watermark_detector_t *detector);

/**
 * @brief Process audio samples for watermark detection (float)
 * @param detector Detector instance
 * @param samples Audio samples
 * @param count Number of samples
 * @param result Detection result (updated if watermark found)
 */
VOICE_API voice_error_t voice_watermark_detect(voice_watermark_detector_t *detector,
                                      const float *samples, size_t count,
                                      voice_watermark_result_t *result);

/**
 * @brief Process audio samples for watermark detection (int16)
 */
VOICE_API voice_error_t voice_watermark_detect_int16(voice_watermark_detector_t *detector,
                                            const int16_t *samples, size_t count,
                                            voice_watermark_result_t *result);

/**
 * @brief Get current detection result
 */
VOICE_API voice_error_t voice_watermark_detector_get_result(voice_watermark_detector_t *detector,
                                                   voice_watermark_result_t *result);

/**
 * @brief Reset detector state
 */
VOICE_API void voice_watermark_detector_reset(voice_watermark_detector_t *detector);

/**
 * @brief Check if watermark is currently being detected
 */
bool voice_watermark_detector_is_detecting(voice_watermark_detector_t *detector);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Quick watermark embedding (convenience function)
 * @param samples Audio samples to watermark
 * @param count Number of samples
 * @param sample_rate Sample rate
 * @param payload Payload data
 * @param payload_size Payload size
 * @param seed Secret key/seed
 */
voice_error_t voice_watermark_quick_embed(float *samples, size_t count,
                                           uint32_t sample_rate,
                                           const uint8_t *payload, size_t payload_size,
                                           uint32_t seed);

/**
 * @brief Quick watermark detection (convenience function)
 */
voice_error_t voice_watermark_quick_detect(const float *samples, size_t count,
                                            uint32_t sample_rate,
                                            uint32_t seed,
                                            voice_watermark_result_t *result);

/**
 * @brief Calculate minimum samples needed for payload
 */
size_t voice_watermark_min_samples_for_payload(size_t payload_size,
                                                uint32_t sample_rate,
                                                uint32_t chips_per_bit);

/**
 * @brief Estimate watermark SNR (signal-to-noise ratio of watermark)
 */
float voice_watermark_estimate_snr(const float *original, const float *watermarked,
                                    size_t count);

/**
 * @brief Get algorithm name as string
 */
const char *voice_watermark_algorithm_to_string(voice_watermark_algorithm_t algorithm);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_WATERMARK_H */
