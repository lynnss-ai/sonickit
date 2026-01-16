/**
 * @file resampler.h
 * @brief Audio resampler interface (SpeexDSP)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef DSP_RESAMPLER_H
#define DSP_RESAMPLER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Resampler
 * ============================================ */

/** Resampler handle */
typedef struct voice_resampler_s voice_resampler_t;

/** Resample quality */
typedef enum {
    VOICE_RESAMPLE_QUALITY_FAST = 0,        /**< Fastest (quality 0) */
    VOICE_RESAMPLE_QUALITY_LOW = 2,         /**< Low quality */
    VOICE_RESAMPLE_QUALITY_MEDIUM = 4,      /**< Medium quality */
    VOICE_RESAMPLE_QUALITY_DEFAULT = 5,     /**< Default (recommended) */
    VOICE_RESAMPLE_QUALITY_HIGH = 7,        /**< High quality */
    VOICE_RESAMPLE_QUALITY_BEST = 10        /**< Best quality */
} voice_resample_quality_t;

/**
 * @brief Create resampler
 * @param channels Number of channels
 * @param in_rate Input sample rate
 * @param out_rate Output sample rate
 * @param quality Quality level (0-10)
 * @return Resampler handle
 */
VOICE_API voice_resampler_t *voice_resampler_create(
    uint8_t channels,
    uint32_t in_rate,
    uint32_t out_rate,
    int quality
);

/**
 * @brief Destroy resampler
 * @param rs Resampler handle
 */
VOICE_API void voice_resampler_destroy(voice_resampler_t *rs);

/**
 * @brief Process int16 format audio
 * @param rs Resampler handle
 * @param in Input data
 * @param in_frames Input frame count (samples per channel)
 * @param out Output buffer
 * @param out_frames Output buffer capacity in frames
 * @return Actual output frame count, negative on error
 */
VOICE_API int voice_resampler_process_int16(
    voice_resampler_t *rs,
    const int16_t *in,
    size_t in_frames,
    int16_t *out,
    size_t out_frames
);

/**
 * @brief Process float format audio
 * @param rs Resampler handle
 * @param in Input data
 * @param in_frames Input frame count (samples per channel)
 * @param out Output buffer
 * @param out_frames Output buffer capacity in frames
 * @return Actual output frame count, negative on error
 */
VOICE_API int voice_resampler_process_float(
    voice_resampler_t *rs,
    const float *in,
    size_t in_frames,
    float *out,
    size_t out_frames
);

/**
 * @brief Set sample rate
 * @param rs Resampler handle
 * @param in_rate Input sample rate
 * @param out_rate Output sample rate
 * @return Error code
 */
VOICE_API voice_error_t voice_resampler_set_rate(
    voice_resampler_t *rs,
    uint32_t in_rate,
    uint32_t out_rate
);

/**
 * @brief Set quality
 * @param rs Resampler handle
 * @param quality Quality level (0-10)
 * @return Error code
 */
VOICE_API voice_error_t voice_resampler_set_quality(voice_resampler_t *rs, int quality);

/**
 * @brief Get input latency
 * @param rs Resampler handle
 * @return Input latency (samples)
 */
VOICE_API int voice_resampler_get_input_latency(voice_resampler_t *rs);

/**
 * @brief Get output latency
 * @param rs Resampler handle
 * @return Output latency (samples)
 */
VOICE_API int voice_resampler_get_output_latency(voice_resampler_t *rs);

/**
 * @brief Reset resampler state
 * @param rs Resampler handle
 */
VOICE_API void voice_resampler_reset(voice_resampler_t *rs);

/**
 * @brief Calculate output frame count
 * @param rs Resampler handle
 * @param in_frames Input frame count
 * @return Expected output frame count
 */
VOICE_API size_t voice_resampler_get_output_frames(voice_resampler_t *rs, size_t in_frames);

/**
 * @brief Calculate input frame count
 * @param rs Resampler handle
 * @param out_frames Required output frame count
 * @return Required input frame count
 */
VOICE_API size_t voice_resampler_get_input_frames(voice_resampler_t *rs, size_t out_frames);

#ifdef __cplusplus
}
#endif

#endif /* DSP_RESAMPLER_H */
