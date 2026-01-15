/**
 * @file spatial.h
 * @brief Spatial audio processing (3D positioning, HRTF, panning)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Provides spatial audio rendering capabilities:
 * - 3D sound source positioning
 * - Distance-based attenuation
 * - Stereo panning (constant power law)
 * - Head-Related Transfer Function (HRTF) binaural rendering
 * - Doppler effect simulation
 */

#ifndef DSP_SPATIAL_H
#define DSP_SPATIAL_H

#include "voice/types.h"
#include "voice/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Constants
 * ============================================ */

/** Speed of sound in air (m/s) at 20°C */
#define VOICE_SPEED_OF_SOUND 343.0f

/** Maximum number of sound sources */
#define VOICE_SPATIAL_MAX_SOURCES 32

/* ============================================
 * Distance Attenuation Models
 * ============================================ */

/** Distance attenuation model */
typedef enum {
    VOICE_DISTANCE_NONE,        /**< No attenuation */
    VOICE_DISTANCE_INVERSE,     /**< 1 / distance (realistic) */
    VOICE_DISTANCE_LINEAR,      /**< Linear falloff */
    VOICE_DISTANCE_EXPONENTIAL  /**< Exponential falloff */
} voice_distance_model_t;

/* ============================================
 * Panning Law
 * ============================================ */

/** Stereo panning law */
typedef enum {
    VOICE_PAN_LINEAR,           /**< Linear panning */
    VOICE_PAN_CONSTANT_POWER,   /**< Constant power (-3dB center) */
    VOICE_PAN_SQRT              /**< Square root law */
} voice_pan_law_t;

/* ============================================
 * Spatial Source
 * ============================================ */

/** 3D position vector */
typedef struct {
    float x;    /**< X coordinate (right is positive) */
    float y;    /**< Y coordinate (up is positive) */
    float z;    /**< Z coordinate (forward is negative) */
} voice_vec3_t;

/** Sound source properties */
typedef struct {
    voice_vec3_t position;      /**< Position in 3D space (meters) */
    voice_vec3_t velocity;      /**< Velocity vector (for Doppler, m/s) */

    float gain;                 /**< Source gain (linear, 0.0-1.0+) */
    float min_distance;         /**< Reference distance (no attenuation, meters) */
    float max_distance;         /**< Maximum distance (full attenuation, meters) */
    float rolloff_factor;       /**< Rolloff factor (1.0 = realistic) */

    bool enable_doppler;        /**< Enable Doppler effect */
    float doppler_factor;       /**< Doppler intensity (1.0 = realistic) */

    float cone_inner_angle;     /**< Inner cone angle (degrees, full volume) */
    float cone_outer_angle;     /**< Outer cone angle (degrees, attenuated) */
    float cone_outer_gain;      /**< Gain outside outer cone */
    voice_vec3_t direction;     /**< Source direction (for directional sources) */
} voice_spatial_source_t;

/** Listener properties */
typedef struct {
    voice_vec3_t position;      /**< Listener position in 3D space */
    voice_vec3_t velocity;      /**< Listener velocity (for Doppler) */
    voice_vec3_t forward;       /**< Forward direction vector (normalized) */
    voice_vec3_t up;            /**< Up direction vector (normalized) */

    float master_gain;          /**< Master gain (linear) */
} voice_spatial_listener_t;

/* ============================================
 * Spatial Renderer Configuration
 * ============================================ */

/** Spatial renderer configuration */
typedef struct {
    uint32_t sample_rate;               /**< Sample rate (Hz) */
    size_t frame_size;                  /**< Frame size (samples) */

    voice_distance_model_t distance_model;  /**< Distance attenuation model */
    voice_pan_law_t pan_law;            /**< Stereo panning law */

    bool enable_hrtf;                   /**< Enable HRTF processing */
    bool enable_air_absorption;         /**< Enable high-frequency absorption */
    bool enable_doppler;                /**< Global Doppler enable */

    float speed_of_sound;               /**< Speed of sound (m/s) */
    float air_absorption_factor;        /**< Air absorption coefficient */
} voice_spatial_config_t;

/** Spatial renderer handle */
typedef struct voice_spatial_renderer_s voice_spatial_renderer_t;

/* ============================================
 * Initialization
 * ============================================ */

/**
 * @brief Initialize default source properties
 * @param source Source to initialize
 */
void voice_spatial_source_init(voice_spatial_source_t *source);

/**
 * @brief Initialize default listener properties
 * @param listener Listener to initialize
 */
void voice_spatial_listener_init(voice_spatial_listener_t *listener);

/**
 * @brief Initialize default renderer configuration
 * @param config Configuration to initialize
 */
void voice_spatial_config_init(voice_spatial_config_t *config);

/* ============================================
 * Renderer Lifecycle
 * ============================================ */

/**
 * @brief Create a spatial audio renderer
 * @param config Renderer configuration
 * @return Renderer handle, or NULL on failure
 */
voice_spatial_renderer_t *voice_spatial_renderer_create(
    const voice_spatial_config_t *config);

/**
 * @brief Destroy a spatial renderer
 * @param renderer Renderer to destroy
 */
void voice_spatial_renderer_destroy(voice_spatial_renderer_t *renderer);

/**
 * @brief Reset renderer state
 * @param renderer Renderer to reset
 */
void voice_spatial_renderer_reset(voice_spatial_renderer_t *renderer);

/* ============================================
 * Listener Control
 * ============================================ */

/**
 * @brief Set listener properties
 * @param renderer Renderer handle
 * @param listener Listener properties
 * @return Error code
 */
voice_error_t voice_spatial_set_listener(
    voice_spatial_renderer_t *renderer,
    const voice_spatial_listener_t *listener);

/**
 * @brief Get current listener properties
 * @param renderer Renderer handle
 * @param listener Output listener properties
 * @return Error code
 */
voice_error_t voice_spatial_get_listener(
    const voice_spatial_renderer_t *renderer,
    voice_spatial_listener_t *listener);

/* ============================================
 * Source Processing
 * ============================================ */

/**
 * @brief Render a mono source to stereo output
 *
 * Applies spatial processing based on source position relative to listener:
 * - Distance attenuation
 * - Stereo panning
 * - Optional HRTF (if enabled)
 * - Optional Doppler (if enabled)
 *
 * @param renderer Renderer handle
 * @param source Source properties
 * @param mono_input Mono input samples
 * @param stereo_output Stereo output buffer (interleaved L/R)
 * @param num_samples Number of mono samples
 * @return Error code
 */
voice_error_t voice_spatial_render_source(
    voice_spatial_renderer_t *renderer,
    const voice_spatial_source_t *source,
    const float *mono_input,
    float *stereo_output,
    size_t num_samples);

/**
 * @brief Render a mono source to stereo output (int16)
 */
voice_error_t voice_spatial_render_source_int16(
    voice_spatial_renderer_t *renderer,
    const voice_spatial_source_t *source,
    const int16_t *mono_input,
    int16_t *stereo_output,
    size_t num_samples);

/* ============================================
 * Simple Stereo Panning (Stateless)
 * ============================================ */

/**
 * @brief Apply stereo panning to a mono signal
 *
 * @param mono_input Mono input samples
 * @param stereo_output Stereo output buffer (interleaved L/R)
 * @param num_samples Number of mono samples
 * @param pan Pan position (-1.0 = left, 0.0 = center, 1.0 = right)
 * @param law Panning law to use
 */
void voice_spatial_pan_mono(
    const float *mono_input,
    float *stereo_output,
    size_t num_samples,
    float pan,
    voice_pan_law_t law);

/**
 * @brief Apply stereo panning (int16 version)
 */
void voice_spatial_pan_mono_int16(
    const int16_t *mono_input,
    int16_t *stereo_output,
    size_t num_samples,
    float pan,
    voice_pan_law_t law);

/* ============================================
 * Distance Attenuation (Stateless)
 * ============================================ */

/**
 * @brief Calculate distance attenuation factor
 *
 * @param distance Distance from source to listener (meters)
 * @param min_distance Reference distance (no attenuation)
 * @param max_distance Maximum distance (full attenuation)
 * @param rolloff Rolloff factor (1.0 = realistic)
 * @param model Attenuation model
 * @return Attenuation factor (0.0 - 1.0)
 */
float voice_spatial_distance_attenuation(
    float distance,
    float min_distance,
    float max_distance,
    float rolloff,
    voice_distance_model_t model);

/* ============================================
 * 3D Utilities
 * ============================================ */

/**
 * @brief Calculate distance between two points
 */
float voice_vec3_distance(const voice_vec3_t *a, const voice_vec3_t *b);

/**
 * @brief Calculate azimuth angle (horizontal angle from forward direction)
 * @param listener Listener properties
 * @param source_pos Source position
 * @return Azimuth in degrees (-180 to 180, positive = right)
 */
float voice_spatial_azimuth(
    const voice_spatial_listener_t *listener,
    const voice_vec3_t *source_pos);

/**
 * @brief Calculate elevation angle (vertical angle from horizontal plane)
 * @param listener Listener properties
 * @param source_pos Source position
 * @return Elevation in degrees (-90 to 90, positive = up)
 */
float voice_spatial_elevation(
    const voice_spatial_listener_t *listener,
    const voice_vec3_t *source_pos);

/**
 * @brief Convert azimuth to stereo pan value
 * @param azimuth Azimuth in degrees
 * @return Pan value (-1.0 to 1.0)
 */
float voice_spatial_azimuth_to_pan(float azimuth);

#ifdef __cplusplus
}
#endif

#endif /* DSP_SPATIAL_H */
