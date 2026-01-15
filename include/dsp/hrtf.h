/**
 * @file hrtf.h
 * @brief Head-Related Transfer Function (HRTF) for binaural 3D audio
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * HRTF enables true 3D audio perception through headphones by simulating
 * how sound waves interact with the human head, ears, and torso.
 *
 * Features:
 * - Built-in simplified HRTF dataset (based on MIT KEMAR measurements)
 * - Spherical interpolation between measured positions
 * - SIMD-optimized convolution
 * - Support for custom HRTF datasets
 */

#ifndef DSP_HRTF_H
#define DSP_HRTF_H

#include "voice/types.h"
#include "voice/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Constants
 * ============================================ */

/** Default HRIR length (samples at 48kHz) */
#define VOICE_HRTF_DEFAULT_LENGTH 128

/** Maximum supported HRIR length */
#define VOICE_HRTF_MAX_LENGTH 512

/** Number of azimuth positions in built-in dataset */
#define VOICE_HRTF_BUILTIN_AZIMUTHS 25

/** Number of elevation positions in built-in dataset */
#define VOICE_HRTF_BUILTIN_ELEVATIONS 7

/* ============================================
 * HRTF Data Structures
 * ============================================ */

/**
 * @brief Single HRIR (Head-Related Impulse Response) pair
 *
 * Contains the impulse responses for left and right ears
 * at a specific azimuth and elevation angle.
 */
typedef struct {
    float azimuth;      /**< Azimuth angle in degrees (-180 to 180) */
    float elevation;    /**< Elevation angle in degrees (-90 to 90) */
    float *left;        /**< Left ear impulse response */
    float *right;       /**< Right ear impulse response */
    size_t length;      /**< Number of samples in each IR */
} voice_hrir_t;

/**
 * @brief HRTF dataset containing multiple HRIR measurements
 */
typedef struct voice_hrtf_s voice_hrtf_t;

/**
 * @brief HRTF processor for real-time binaural rendering
 */
typedef struct voice_hrtf_processor_s voice_hrtf_processor_t;

/**
 * @brief HRTF processor configuration
 */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate (Hz) */
    size_t block_size;          /**< Processing block size */
    bool enable_crossfade;      /**< Smooth transitions when position changes */
    float crossfade_time_ms;    /**< Crossfade duration (ms) */
    bool enable_itd;            /**< Enable Interaural Time Difference */
} voice_hrtf_config_t;

/* ============================================
 * HRTF Dataset Management
 * ============================================ */

/**
 * @brief Load the built-in HRTF dataset
 *
 * Uses a simplified dataset based on MIT KEMAR measurements,
 * optimized for real-time processing.
 *
 * @return HRTF dataset, or NULL on failure
 */
voice_hrtf_t *voice_hrtf_load_builtin(void);

/**
 * @brief Create a custom HRTF dataset
 *
 * @param num_positions Number of measurement positions
 * @param hrir_length Length of each HRIR
 * @param sample_rate Sample rate of HRIR data
 * @return HRTF dataset, or NULL on failure
 */
voice_hrtf_t *voice_hrtf_create(size_t num_positions,
                                 size_t hrir_length,
                                 uint32_t sample_rate);

/**
 * @brief Add an HRIR measurement to the dataset
 *
 * @param hrtf HRTF dataset
 * @param azimuth Azimuth angle in degrees
 * @param elevation Elevation angle in degrees
 * @param left Left ear impulse response
 * @param right Right ear impulse response
 * @param length Number of samples
 * @return Error code
 */
voice_error_t voice_hrtf_add_hrir(voice_hrtf_t *hrtf,
                                   float azimuth,
                                   float elevation,
                                   const float *left,
                                   const float *right,
                                   size_t length);

/**
 * @brief Destroy an HRTF dataset
 */
void voice_hrtf_destroy(voice_hrtf_t *hrtf);

/**
 * @brief Get HRTF dataset info
 */
void voice_hrtf_get_info(const voice_hrtf_t *hrtf,
                          size_t *num_positions,
                          size_t *hrir_length,
                          uint32_t *sample_rate);

/* ============================================
 * HRTF Processor
 * ============================================ */

/**
 * @brief Initialize default HRTF processor config
 */
void voice_hrtf_config_init(voice_hrtf_config_t *config);

/**
 * @brief Create an HRTF processor
 *
 * @param hrtf HRTF dataset to use (ownership not transferred)
 * @param config Processor configuration
 * @return Processor handle, or NULL on failure
 */
voice_hrtf_processor_t *voice_hrtf_processor_create(
    const voice_hrtf_t *hrtf,
    const voice_hrtf_config_t *config);

/**
 * @brief Destroy an HRTF processor
 */
void voice_hrtf_processor_destroy(voice_hrtf_processor_t *processor);

/**
 * @brief Reset processor state
 */
void voice_hrtf_processor_reset(voice_hrtf_processor_t *processor);

/**
 * @brief Process mono audio to binaural stereo
 *
 * Applies HRTF convolution based on the specified azimuth and elevation.
 * Uses spherical interpolation for positions between measured HRIRs.
 *
 * @param processor HRTF processor
 * @param mono_input Mono input samples
 * @param binaural_output Stereo output (interleaved L/R)
 * @param num_samples Number of input samples
 * @param azimuth Azimuth angle in degrees (-180 to 180, 0 = front)
 * @param elevation Elevation angle in degrees (-90 to 90, 0 = horizontal)
 * @return Error code
 */
voice_error_t voice_hrtf_process(
    voice_hrtf_processor_t *processor,
    const float *mono_input,
    float *binaural_output,
    size_t num_samples,
    float azimuth,
    float elevation);

/**
 * @brief Process with int16 samples
 */
voice_error_t voice_hrtf_process_int16(
    voice_hrtf_processor_t *processor,
    const int16_t *mono_input,
    int16_t *binaural_output,
    size_t num_samples,
    float azimuth,
    float elevation);

/* ============================================
 * HRIR Interpolation
 * ============================================ */

/**
 * @brief Get interpolated HRIR for any position
 *
 * Performs spherical interpolation between the nearest measured positions.
 *
 * @param hrtf HRTF dataset
 * @param azimuth Target azimuth
 * @param elevation Target elevation
 * @param left_out Output left HRIR (must be pre-allocated)
 * @param right_out Output right HRIR (must be pre-allocated)
 * @param max_length Maximum output length
 * @return Actual HRIR length, or 0 on error
 */
size_t voice_hrtf_interpolate(
    const voice_hrtf_t *hrtf,
    float azimuth,
    float elevation,
    float *left_out,
    float *right_out,
    size_t max_length);

/**
 * @brief Calculate Interaural Time Difference (ITD)
 *
 * Returns the time delay between ears based on head geometry.
 * Positive = right ear leads, negative = left ear leads.
 *
 * @param azimuth Azimuth angle in degrees
 * @param head_radius Head radius in meters (default ~0.0875m)
 * @return ITD in seconds
 */
float voice_hrtf_calculate_itd(float azimuth, float head_radius);

#ifdef __cplusplus
}
#endif

#endif /* DSP_HRTF_H */
