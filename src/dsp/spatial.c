/**
 * @file spatial.c
 * @brief Spatial audio processing implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Implements 3D audio positioning with:
 * - Stereo panning (linear, constant power, sqrt)
 * - Distance-based attenuation (inverse, linear, exponential)
 * - 3D to stereo rendering
 * - Air absorption simulation
 * - Doppler effect (future)
 */

#include "dsp/spatial.h"
#include "utils/simd_utils.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Internal Structures
 * ============================================ */

struct voice_spatial_renderer_s {
    voice_spatial_config_t config;
    voice_spatial_listener_t listener;

    /* Air absorption filter state */
    float air_absorption_state_l;
    float air_absorption_state_r;

    /* Doppler state */
    float doppler_ratio;

    /* Work buffers */
    float *work_buffer;
    size_t work_buffer_size;
};

/* ============================================
 * Vector Math Utilities
 * ============================================ */

static inline float vec3_dot(const voice_vec3_t *a, const voice_vec3_t *b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline float vec3_length(const voice_vec3_t *v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static inline void vec3_normalize(voice_vec3_t *v) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        v->x /= len;
        v->y /= len;
        v->z /= len;
    }
}

static inline void vec3_sub(voice_vec3_t *result,
                            const voice_vec3_t *a,
                            const voice_vec3_t *b) {
    result->x = a->x - b->x;
    result->y = a->y - b->y;
    result->z = a->z - b->z;
}

static inline void vec3_cross(voice_vec3_t *result,
                              const voice_vec3_t *a,
                              const voice_vec3_t *b) {
    result->x = a->y * b->z - a->z * b->y;
    result->y = a->z * b->x - a->x * b->z;
    result->z = a->x * b->y - a->y * b->x;
}

/* ============================================
 * Distance Calculation
 * ============================================ */

float voice_vec3_distance(const voice_vec3_t *a, const voice_vec3_t *b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* ============================================
 * Distance Attenuation
 * ============================================ */

float voice_spatial_distance_attenuation(
    float distance,
    float min_distance,
    float max_distance,
    float rolloff,
    voice_distance_model_t model)
{
    /* Clamp distance to valid range */
    if (distance < min_distance) {
        return 1.0f;
    }
    if (distance > max_distance && max_distance > min_distance) {
        return 0.0f;
    }

    float attenuation = 1.0f;

    switch (model) {
        case VOICE_DISTANCE_NONE:
            attenuation = 1.0f;
            break;

        case VOICE_DISTANCE_INVERSE:
            /* OpenAL-style inverse distance:
             * gain = min_dist / (min_dist + rolloff * (distance - min_dist)) */
            attenuation = min_distance /
                (min_distance + rolloff * (distance - min_distance));
            break;

        case VOICE_DISTANCE_LINEAR:
            /* Linear falloff between min and max distance */
            if (max_distance > min_distance) {
                float range = max_distance - min_distance;
                float d = distance - min_distance;
                attenuation = 1.0f - rolloff * (d / range);
                if (attenuation < 0.0f) attenuation = 0.0f;
            }
            break;

        case VOICE_DISTANCE_EXPONENTIAL:
            /* Exponential falloff:
             * gain = pow(distance / min_dist, -rolloff) */
            if (distance > 0.0f && min_distance > 0.0f) {
                attenuation = powf(distance / min_distance, -rolloff);
            }
            break;
    }

    /* Clamp result */
    if (attenuation < 0.0f) attenuation = 0.0f;
    if (attenuation > 1.0f) attenuation = 1.0f;

    return attenuation;
}

/* ============================================
 * Azimuth and Elevation Calculation
 * ============================================ */

float voice_spatial_azimuth(
    const voice_spatial_listener_t *listener,
    const voice_vec3_t *source_pos)
{
    /* Calculate direction from listener to source */
    voice_vec3_t dir;
    vec3_sub(&dir, source_pos, &listener->position);

    float len = vec3_length(&dir);
    if (len < 1e-6f) {
        return 0.0f;  /* Source at listener position */
    }

    /* Normalize direction */
    dir.x /= len;
    dir.y /= len;
    dir.z /= len;

    /* Calculate right vector from forward and up */
    voice_vec3_t right;
    vec3_cross(&right, &listener->forward, &listener->up);
    vec3_normalize(&right);

    /* Project onto horizontal plane (forward-right plane) */
    float front = vec3_dot(&dir, &listener->forward);
    float side = vec3_dot(&dir, &right);

    /* Calculate azimuth angle */
    float azimuth = atan2f(side, front) * 180.0f / (float)M_PI;

    return azimuth;
}

float voice_spatial_elevation(
    const voice_spatial_listener_t *listener,
    const voice_vec3_t *source_pos)
{
    /* Calculate direction from listener to source */
    voice_vec3_t dir;
    vec3_sub(&dir, source_pos, &listener->position);

    float len = vec3_length(&dir);
    if (len < 1e-6f) {
        return 0.0f;
    }

    /* Normalize direction */
    dir.x /= len;
    dir.y /= len;
    dir.z /= len;

    /* Calculate elevation from dot product with up vector */
    float up_component = vec3_dot(&dir, &listener->up);

    /* Clamp to valid range for asin */
    if (up_component > 1.0f) up_component = 1.0f;
    if (up_component < -1.0f) up_component = -1.0f;

    float elevation = asinf(up_component) * 180.0f / (float)M_PI;

    return elevation;
}

float voice_spatial_azimuth_to_pan(float azimuth) {
    /* Convert azimuth (-180 to 180) to pan (-1 to 1) */
    /* At 90 degrees right, pan = 1.0 */
    /* At 90 degrees left, pan = -1.0 */
    float pan = azimuth / 90.0f;

    /* Clamp to valid range */
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    return pan;
}

/* ============================================
 * Stereo Panning
 * ============================================ */

static inline void calculate_pan_gains(float pan, voice_pan_law_t law,
                                       float *gain_l, float *gain_r) {
    /* Clamp pan to valid range */
    if (pan < -1.0f) pan = -1.0f;
    if (pan > 1.0f) pan = 1.0f;

    /* Convert pan from [-1, 1] to [0, 1] for calculations */
    float pan_normalized = (pan + 1.0f) * 0.5f;

    switch (law) {
        case VOICE_PAN_LINEAR:
            /* Linear crossfade */
            *gain_l = 1.0f - pan_normalized;
            *gain_r = pan_normalized;
            break;

        case VOICE_PAN_CONSTANT_POWER:
            /* Constant power panning (preserves perceived loudness) */
            /* Uses sine/cosine for smooth -3dB center */
            *gain_l = cosf(pan_normalized * (float)M_PI * 0.5f);
            *gain_r = sinf(pan_normalized * (float)M_PI * 0.5f);
            break;

        case VOICE_PAN_SQRT:
            /* Square root law (compromise between linear and constant power) */
            *gain_l = sqrtf(1.0f - pan_normalized);
            *gain_r = sqrtf(pan_normalized);
            break;

        default:
            *gain_l = *gain_r = 0.707f;
            break;
    }
}

void voice_spatial_pan_mono(
    const float *mono_input,
    float *stereo_output,
    size_t num_samples,
    float pan,
    voice_pan_law_t law)
{
    if (!mono_input || !stereo_output || num_samples == 0) {
        return;
    }

    float gain_l, gain_r;
    calculate_pan_gains(pan, law, &gain_l, &gain_r);

    /* Apply panning with interleaved stereo output */
    for (size_t i = 0; i < num_samples; i++) {
        float sample = mono_input[i];
        stereo_output[i * 2] = sample * gain_l;      /* Left */
        stereo_output[i * 2 + 1] = sample * gain_r;  /* Right */
    }
}

void voice_spatial_pan_mono_int16(
    const int16_t *mono_input,
    int16_t *stereo_output,
    size_t num_samples,
    float pan,
    voice_pan_law_t law)
{
    if (!mono_input || !stereo_output || num_samples == 0) {
        return;
    }

    float gain_l, gain_r;
    calculate_pan_gains(pan, law, &gain_l, &gain_r);

    /* Apply panning with saturation */
    for (size_t i = 0; i < num_samples; i++) {
        float sample = (float)mono_input[i];

        float left = sample * gain_l;
        float right = sample * gain_r;

        /* Saturate to int16 range */
        if (left > 32767.0f) left = 32767.0f;
        if (left < -32768.0f) left = -32768.0f;
        if (right > 32767.0f) right = 32767.0f;
        if (right < -32768.0f) right = -32768.0f;

        stereo_output[i * 2] = (int16_t)left;
        stereo_output[i * 2 + 1] = (int16_t)right;
    }
}

/* ============================================
 * Initialization Functions
 * ============================================ */

void voice_spatial_source_init(voice_spatial_source_t *source) {
    if (!source) return;

    memset(source, 0, sizeof(voice_spatial_source_t));

    /* Default position: in front of listener */
    source->position.x = 0.0f;
    source->position.y = 0.0f;
    source->position.z = -1.0f;  /* 1 meter in front */

    /* Default velocity: stationary */
    source->velocity.x = 0.0f;
    source->velocity.y = 0.0f;
    source->velocity.z = 0.0f;

    source->gain = 1.0f;
    source->min_distance = 1.0f;      /* Reference distance */
    source->max_distance = 100.0f;    /* Max audible distance */
    source->rolloff_factor = 1.0f;    /* Realistic falloff */

    source->enable_doppler = false;
    source->doppler_factor = 1.0f;

    /* Omnidirectional by default */
    source->cone_inner_angle = 360.0f;
    source->cone_outer_angle = 360.0f;
    source->cone_outer_gain = 0.0f;
    source->direction.z = -1.0f;
}

void voice_spatial_listener_init(voice_spatial_listener_t *listener) {
    if (!listener) return;

    memset(listener, 0, sizeof(voice_spatial_listener_t));

    /* Default position: origin */
    listener->position.x = 0.0f;
    listener->position.y = 0.0f;
    listener->position.z = 0.0f;

    /* Default velocity: stationary */
    listener->velocity.x = 0.0f;
    listener->velocity.y = 0.0f;
    listener->velocity.z = 0.0f;

    /* Default orientation: looking down -Z axis */
    listener->forward.x = 0.0f;
    listener->forward.y = 0.0f;
    listener->forward.z = -1.0f;

    listener->up.x = 0.0f;
    listener->up.y = 1.0f;
    listener->up.z = 0.0f;

    listener->master_gain = 1.0f;
}

void voice_spatial_config_init(voice_spatial_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(voice_spatial_config_t));

    config->sample_rate = 48000;
    config->frame_size = 960;  /* 20ms at 48kHz */

    config->distance_model = VOICE_DISTANCE_INVERSE;
    config->pan_law = VOICE_PAN_CONSTANT_POWER;

    config->enable_hrtf = false;
    config->enable_air_absorption = false;
    config->enable_doppler = false;

    config->speed_of_sound = VOICE_SPEED_OF_SOUND;
    config->air_absorption_factor = 0.001f;
}

/* ============================================
 * Renderer Lifecycle
 * ============================================ */

voice_spatial_renderer_t *voice_spatial_renderer_create(
    const voice_spatial_config_t *config)
{
    if (!config) return NULL;

    voice_spatial_renderer_t *renderer =
        (voice_spatial_renderer_t *)calloc(1, sizeof(voice_spatial_renderer_t));
    if (!renderer) return NULL;

    renderer->config = *config;

    /* Initialize listener to default */
    voice_spatial_listener_init(&renderer->listener);

    /* Allocate work buffer for processing */
    renderer->work_buffer_size = config->frame_size * 2;  /* Stereo */
    renderer->work_buffer = (float *)calloc(renderer->work_buffer_size, sizeof(float));
    if (!renderer->work_buffer) {
        free(renderer);
        return NULL;
    }

    /* Initialize filter states */
    renderer->air_absorption_state_l = 0.0f;
    renderer->air_absorption_state_r = 0.0f;
    renderer->doppler_ratio = 1.0f;

    return renderer;
}

void voice_spatial_renderer_destroy(voice_spatial_renderer_t *renderer) {
    if (!renderer) return;

    if (renderer->work_buffer) {
        free(renderer->work_buffer);
    }

    free(renderer);
}

void voice_spatial_renderer_reset(voice_spatial_renderer_t *renderer) {
    if (!renderer) return;

    renderer->air_absorption_state_l = 0.0f;
    renderer->air_absorption_state_r = 0.0f;
    renderer->doppler_ratio = 1.0f;

    if (renderer->work_buffer) {
        memset(renderer->work_buffer, 0,
               renderer->work_buffer_size * sizeof(float));
    }
}

/* ============================================
 * Listener Control
 * ============================================ */

voice_error_t voice_spatial_set_listener(
    voice_spatial_renderer_t *renderer,
    const voice_spatial_listener_t *listener)
{
    if (!renderer || !listener) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    renderer->listener = *listener;

    /* Normalize orientation vectors */
    vec3_normalize(&renderer->listener.forward);
    vec3_normalize(&renderer->listener.up);

    return VOICE_SUCCESS;
}

voice_error_t voice_spatial_get_listener(
    const voice_spatial_renderer_t *renderer,
    voice_spatial_listener_t *listener)
{
    if (!renderer || !listener) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    *listener = renderer->listener;
    return VOICE_SUCCESS;
}

/* ============================================
 * Source Rendering
 * ============================================ */

/**
 * @brief Apply simple air absorption (low-pass filter effect)
 *
 * High frequencies are absorbed more over distance.
 * Uses a simple one-pole low-pass filter.
 */
static void apply_air_absorption(float *stereo_samples, size_t num_samples,
                                 float distance, float absorption_factor,
                                 float *state_l, float *state_r) {
    if (distance <= 0.0f || absorption_factor <= 0.0f) {
        return;
    }

    /* Calculate filter coefficient based on distance */
    /* Higher distance = more absorption = lower cutoff */
    float absorption = expf(-absorption_factor * distance);
    float coeff = 1.0f - absorption;
    if (coeff > 0.9f) coeff = 0.9f;  /* Limit to prevent instability */

    /* Apply one-pole low-pass filter */
    for (size_t i = 0; i < num_samples; i++) {
        /* Left channel */
        float in_l = stereo_samples[i * 2];
        *state_l = *state_l + coeff * (in_l - *state_l);
        stereo_samples[i * 2] = *state_l;

        /* Right channel */
        float in_r = stereo_samples[i * 2 + 1];
        *state_r = *state_r + coeff * (in_r - *state_r);
        stereo_samples[i * 2 + 1] = *state_r;
    }
}

voice_error_t voice_spatial_render_source(
    voice_spatial_renderer_t *renderer,
    const voice_spatial_source_t *source,
    const float *mono_input,
    float *stereo_output,
    size_t num_samples)
{
    if (!renderer || !source || !mono_input || !stereo_output) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    if (num_samples == 0) {
        return VOICE_SUCCESS;
    }

    /* Calculate distance from listener to source */
    float distance = voice_vec3_distance(&renderer->listener.position,
                                         &source->position);

    /* Calculate distance attenuation */
    float dist_gain = voice_spatial_distance_attenuation(
        distance,
        source->min_distance,
        source->max_distance,
        source->rolloff_factor,
        renderer->config.distance_model);

    /* Calculate total gain */
    float total_gain = source->gain * dist_gain * renderer->listener.master_gain;

    /* Calculate azimuth and convert to pan */
    float azimuth = voice_spatial_azimuth(&renderer->listener, &source->position);
    float pan = voice_spatial_azimuth_to_pan(azimuth);

    /* Calculate pan gains */
    float gain_l, gain_r;
    calculate_pan_gains(pan, renderer->config.pan_law, &gain_l, &gain_r);

    /* Apply total gain to pan gains */
    gain_l *= total_gain;
    gain_r *= total_gain;

    /* Render to stereo output */
    for (size_t i = 0; i < num_samples; i++) {
        float sample = mono_input[i];
        stereo_output[i * 2] = sample * gain_l;      /* Left */
        stereo_output[i * 2 + 1] = sample * gain_r;  /* Right */
    }

    /* Apply air absorption if enabled */
    if (renderer->config.enable_air_absorption) {
        apply_air_absorption(stereo_output, num_samples, distance,
                            renderer->config.air_absorption_factor,
                            &renderer->air_absorption_state_l,
                            &renderer->air_absorption_state_r);
    }

    return VOICE_SUCCESS;
}

voice_error_t voice_spatial_render_source_int16(
    voice_spatial_renderer_t *renderer,
    const voice_spatial_source_t *source,
    const int16_t *mono_input,
    int16_t *stereo_output,
    size_t num_samples)
{
    if (!renderer || !source || !mono_input || !stereo_output) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    if (num_samples == 0) {
        return VOICE_SUCCESS;
    }

    /* Calculate distance from listener to source */
    float distance = voice_vec3_distance(&renderer->listener.position,
                                         &source->position);

    /* Calculate distance attenuation */
    float dist_gain = voice_spatial_distance_attenuation(
        distance,
        source->min_distance,
        source->max_distance,
        source->rolloff_factor,
        renderer->config.distance_model);

    /* Calculate total gain */
    float total_gain = source->gain * dist_gain * renderer->listener.master_gain;

    /* Calculate azimuth and convert to pan */
    float azimuth = voice_spatial_azimuth(&renderer->listener, &source->position);
    float pan = voice_spatial_azimuth_to_pan(azimuth);

    /* Calculate pan gains */
    float gain_l, gain_r;
    calculate_pan_gains(pan, renderer->config.pan_law, &gain_l, &gain_r);

    /* Apply total gain to pan gains */
    gain_l *= total_gain;
    gain_r *= total_gain;

    /* Render to stereo output with saturation */
    for (size_t i = 0; i < num_samples; i++) {
        float sample = (float)mono_input[i];

        float left = sample * gain_l;
        float right = sample * gain_r;

        /* Saturate to int16 range */
        if (left > 32767.0f) left = 32767.0f;
        if (left < -32768.0f) left = -32768.0f;
        if (right > 32767.0f) right = 32767.0f;
        if (right < -32768.0f) right = -32768.0f;

        stereo_output[i * 2] = (int16_t)left;
        stereo_output[i * 2 + 1] = (int16_t)right;
    }

    return VOICE_SUCCESS;
}
