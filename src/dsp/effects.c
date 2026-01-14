/**
 * @file effects.c
 * @brief Audio effects implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/effects.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================
 * Reverb Implementation (Freeverb-style)
 * ============================================ */

#define REVERB_COMB_COUNT    8
#define REVERB_ALLPASS_COUNT 4

/* Comb filter delays (samples @ 44.1kHz) */
static const int COMB_DELAYS[REVERB_COMB_COUNT] = {
    1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617
};

/* Allpass filter delays */
static const int ALLPASS_DELAYS[REVERB_ALLPASS_COUNT] = {
    556, 441, 341, 225
};

typedef struct {
    float *buffer;
    size_t size;
    size_t index;
    float feedback;
    float damp1;
    float damp2;
    float filter_store;
} comb_filter_t;

typedef struct {
    float *buffer;
    size_t size;
    size_t index;
    float feedback;
} allpass_filter_t;

struct voice_reverb_s {
    voice_reverb_config_t config;
    comb_filter_t combs[REVERB_COMB_COUNT];
    allpass_filter_t allpasses[REVERB_ALLPASS_COUNT];
    float *pre_delay_buffer;
    size_t pre_delay_size;
    size_t pre_delay_index;
    float gain;
};

static void comb_init(comb_filter_t *comb, size_t size)
{
    comb->buffer = (float *)calloc(size, sizeof(float));
    comb->size = size;
    comb->index = 0;
    comb->feedback = 0.5f;
    comb->damp1 = 0.5f;
    comb->damp2 = 0.5f;
    comb->filter_store = 0.0f;
}

static void comb_free(comb_filter_t *comb)
{
    if (comb->buffer) free(comb->buffer);
}

static float comb_process(comb_filter_t *comb, float input)
{
    float output = comb->buffer[comb->index];
    
    comb->filter_store = (output * comb->damp2) + (comb->filter_store * comb->damp1);
    comb->buffer[comb->index] = input + (comb->filter_store * comb->feedback);
    
    comb->index++;
    if (comb->index >= comb->size) comb->index = 0;
    
    return output;
}

static void allpass_init(allpass_filter_t *ap, size_t size)
{
    ap->buffer = (float *)calloc(size, sizeof(float));
    ap->size = size;
    ap->index = 0;
    ap->feedback = 0.5f;
}

static void allpass_free(allpass_filter_t *ap)
{
    if (ap->buffer) free(ap->buffer);
}

static float allpass_process(allpass_filter_t *ap, float input)
{
    float buf_out = ap->buffer[ap->index];
    float output = -input + buf_out;
    
    ap->buffer[ap->index] = input + (buf_out * ap->feedback);
    
    ap->index++;
    if (ap->index >= ap->size) ap->index = 0;
    
    return output;
}

void voice_reverb_config_init(voice_reverb_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_reverb_config_t));
    config->sample_rate = 48000;
    config->type = VOICE_REVERB_ROOM;
    config->room_size = 0.5f;
    config->damping = 0.5f;
    config->wet_level = 0.3f;
    config->dry_level = 0.7f;
    config->width = 1.0f;
    config->pre_delay_ms = 10.0f;
}

voice_reverb_t *voice_reverb_create(const voice_reverb_config_t *config)
{
    if (!config) return NULL;
    
    voice_reverb_t *reverb = (voice_reverb_t *)calloc(1, sizeof(voice_reverb_t));
    if (!reverb) return NULL;
    
    reverb->config = *config;
    
    /* Scale delays for sample rate */
    float scale = config->sample_rate / 44100.0f;
    
    /* Initialize comb filters */
    for (int i = 0; i < REVERB_COMB_COUNT; i++) {
        size_t size = (size_t)(COMB_DELAYS[i] * scale);
        comb_init(&reverb->combs[i], size);
    }
    
    /* Initialize allpass filters */
    for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
        size_t size = (size_t)(ALLPASS_DELAYS[i] * scale);
        allpass_init(&reverb->allpasses[i], size);
        reverb->allpasses[i].feedback = 0.5f;
    }
    
    /* Pre-delay buffer */
    reverb->pre_delay_size = (size_t)(config->pre_delay_ms * config->sample_rate / 1000.0f);
    if (reverb->pre_delay_size > 0) {
        reverb->pre_delay_buffer = (float *)calloc(reverb->pre_delay_size, sizeof(float));
    }
    reverb->pre_delay_index = 0;
    
    reverb->gain = 0.015f;
    
    /* Apply room size */
    voice_reverb_set_room_size(reverb, config->room_size);
    voice_reverb_set_damping(reverb, config->damping);
    
    return reverb;
}

void voice_reverb_destroy(voice_reverb_t *reverb)
{
    if (!reverb) return;
    
    for (int i = 0; i < REVERB_COMB_COUNT; i++) {
        comb_free(&reverb->combs[i]);
    }
    for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
        allpass_free(&reverb->allpasses[i]);
    }
    if (reverb->pre_delay_buffer) free(reverb->pre_delay_buffer);
    
    free(reverb);
}

voice_error_t voice_reverb_process(voice_reverb_t *reverb, float *samples, size_t count)
{
    if (!reverb || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float wet = reverb->config.wet_level;
    float dry = reverb->config.dry_level;
    
    for (size_t i = 0; i < count; i++) {
        float input = samples[i] * reverb->gain;
        float output = 0.0f;
        
        /* Pre-delay */
        if (reverb->pre_delay_buffer && reverb->pre_delay_size > 0) {
            float delayed = reverb->pre_delay_buffer[reverb->pre_delay_index];
            reverb->pre_delay_buffer[reverb->pre_delay_index] = input;
            reverb->pre_delay_index++;
            if (reverb->pre_delay_index >= reverb->pre_delay_size) {
                reverb->pre_delay_index = 0;
            }
            input = delayed;
        }
        
        /* Accumulate comb filters */
        for (int c = 0; c < REVERB_COMB_COUNT; c++) {
            output += comb_process(&reverb->combs[c], input);
        }
        
        /* Apply allpass filters */
        for (int a = 0; a < REVERB_ALLPASS_COUNT; a++) {
            output = allpass_process(&reverb->allpasses[a], output);
        }
        
        /* Mix */
        samples[i] = samples[i] * dry + output * wet;
    }
    
    return VOICE_OK;
}

voice_error_t voice_reverb_process_int16(voice_reverb_t *reverb, int16_t *samples, size_t count)
{
    if (!reverb || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    /* Convert to float, process, convert back */
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_reverb_process(reverb, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

void voice_reverb_set_room_size(voice_reverb_t *reverb, float size)
{
    if (!reverb) return;
    
    reverb->config.room_size = size;
    float feedback = 0.28f + size * 0.7f;
    
    for (int i = 0; i < REVERB_COMB_COUNT; i++) {
        reverb->combs[i].feedback = feedback;
    }
}

void voice_reverb_set_damping(voice_reverb_t *reverb, float damping)
{
    if (!reverb) return;
    
    reverb->config.damping = damping;
    
    for (int i = 0; i < REVERB_COMB_COUNT; i++) {
        reverb->combs[i].damp1 = damping;
        reverb->combs[i].damp2 = 1.0f - damping;
    }
}

void voice_reverb_set_wet_level(voice_reverb_t *reverb, float level)
{
    if (!reverb) return;
    reverb->config.wet_level = level;
}

void voice_reverb_set_dry_level(voice_reverb_t *reverb, float level)
{
    if (!reverb) return;
    reverb->config.dry_level = level;
}

void voice_reverb_reset(voice_reverb_t *reverb)
{
    if (!reverb) return;
    
    for (int i = 0; i < REVERB_COMB_COUNT; i++) {
        memset(reverb->combs[i].buffer, 0, reverb->combs[i].size * sizeof(float));
        reverb->combs[i].index = 0;
        reverb->combs[i].filter_store = 0.0f;
    }
    for (int i = 0; i < REVERB_ALLPASS_COUNT; i++) {
        memset(reverb->allpasses[i].buffer, 0, reverb->allpasses[i].size * sizeof(float));
        reverb->allpasses[i].index = 0;
    }
    if (reverb->pre_delay_buffer) {
        memset(reverb->pre_delay_buffer, 0, reverb->pre_delay_size * sizeof(float));
    }
    reverb->pre_delay_index = 0;
}

/* ============================================
 * Delay Implementation
 * ============================================ */

struct voice_delay_s {
    voice_delay_config_t config;
    float *buffer;
    size_t buffer_size;
    size_t write_index;
    float lfo_phase;
};

void voice_delay_config_init(voice_delay_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_delay_config_t));
    config->sample_rate = 48000;
    config->type = VOICE_DELAY_SIMPLE;
    config->delay_ms = 250.0f;
    config->feedback = 0.3f;
    config->mix = 0.5f;
    config->modulation_rate = 0.5f;
    config->modulation_depth = 0.0f;
    config->max_delay_ms = 1000.0f;
}

voice_delay_t *voice_delay_create(const voice_delay_config_t *config)
{
    if (!config) return NULL;
    
    voice_delay_t *delay = (voice_delay_t *)calloc(1, sizeof(voice_delay_t));
    if (!delay) return NULL;
    
    delay->config = *config;
    
    /* Allocate buffer for max delay */
    delay->buffer_size = (size_t)(config->max_delay_ms * config->sample_rate / 1000.0f);
    delay->buffer = (float *)calloc(delay->buffer_size, sizeof(float));
    if (!delay->buffer) {
        free(delay);
        return NULL;
    }
    
    delay->write_index = 0;
    delay->lfo_phase = 0.0f;
    
    return delay;
}

void voice_delay_destroy(voice_delay_t *delay)
{
    if (!delay) return;
    if (delay->buffer) free(delay->buffer);
    free(delay);
}

voice_error_t voice_delay_process(voice_delay_t *delay, float *samples, size_t count)
{
    if (!delay || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float delay_samples = delay->config.delay_ms * delay->config.sample_rate / 1000.0f;
    float feedback = delay->config.feedback;
    float mix = delay->config.mix;
    float lfo_inc = 2.0f * (float)M_PI * delay->config.modulation_rate / delay->config.sample_rate;
    
    for (size_t i = 0; i < count; i++) {
        /* Calculate read position with optional modulation */
        float mod = 0.0f;
        if (delay->config.type == VOICE_DELAY_TAPE) {
            mod = sinf(delay->lfo_phase) * delay->config.modulation_depth * delay->config.sample_rate / 1000.0f;
            delay->lfo_phase += lfo_inc;
            if (delay->lfo_phase > 2.0f * (float)M_PI) {
                delay->lfo_phase -= 2.0f * (float)M_PI;
            }
        }
        
        float read_pos = (float)delay->write_index - delay_samples - mod;
        if (read_pos < 0) read_pos += delay->buffer_size;
        
        /* Linear interpolation */
        size_t read_idx = (size_t)read_pos;
        float frac = read_pos - read_idx;
        
        size_t next_idx = read_idx + 1;
        if (next_idx >= delay->buffer_size) next_idx = 0;
        
        float delayed = delay->buffer[read_idx] * (1.0f - frac) + 
                       delay->buffer[next_idx] * frac;
        
        /* Write with feedback */
        float input = samples[i];
        delay->buffer[delay->write_index] = input + delayed * feedback;
        
        /* Advance write position */
        delay->write_index++;
        if (delay->write_index >= delay->buffer_size) {
            delay->write_index = 0;
        }
        
        /* Output mix */
        samples[i] = input * (1.0f - mix) + delayed * mix;
    }
    
    return VOICE_OK;
}

voice_error_t voice_delay_process_int16(voice_delay_t *delay, int16_t *samples, size_t count)
{
    if (!delay || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_delay_process(delay, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

void voice_delay_set_time(voice_delay_t *delay, float delay_ms)
{
    if (!delay) return;
    if (delay_ms > delay->config.max_delay_ms) {
        delay_ms = delay->config.max_delay_ms;
    }
    delay->config.delay_ms = delay_ms;
}

void voice_delay_set_feedback(voice_delay_t *delay, float feedback)
{
    if (!delay) return;
    if (feedback < 0.0f) feedback = 0.0f;
    if (feedback > 0.99f) feedback = 0.99f;
    delay->config.feedback = feedback;
}

void voice_delay_reset(voice_delay_t *delay)
{
    if (!delay) return;
    memset(delay->buffer, 0, delay->buffer_size * sizeof(float));
    delay->write_index = 0;
    delay->lfo_phase = 0.0f;
}

/* ============================================
 * Pitch Shift Implementation (Simple SOLA)
 * ============================================ */

struct voice_pitch_shift_s {
    voice_pitch_shift_config_t config;
    float pitch_ratio;
    float *buffer;
    size_t buffer_size;
    float read_position;
    size_t write_position;
};

void voice_pitch_shift_config_init(voice_pitch_shift_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_pitch_shift_config_t));
    config->sample_rate = 48000;
    config->semitones = 0.0f;
    config->cents = 0.0f;
    config->frame_size = 1024;
    config->overlap = 4;
    config->preserve_formants = false;
}

voice_pitch_shift_t *voice_pitch_shift_create(const voice_pitch_shift_config_t *config)
{
    if (!config) return NULL;
    
    voice_pitch_shift_t *pitch = (voice_pitch_shift_t *)calloc(1, sizeof(voice_pitch_shift_t));
    if (!pitch) return NULL;
    
    pitch->config = *config;
    
    /* Calculate pitch ratio */
    float total_semitones = config->semitones + config->cents / 100.0f;
    pitch->pitch_ratio = powf(2.0f, total_semitones / 12.0f);
    
    /* Allocate circular buffer */
    pitch->buffer_size = config->frame_size * 4;
    pitch->buffer = (float *)calloc(pitch->buffer_size, sizeof(float));
    if (!pitch->buffer) {
        free(pitch);
        return NULL;
    }
    
    pitch->read_position = 0.0f;
    pitch->write_position = 0;
    
    return pitch;
}

void voice_pitch_shift_destroy(voice_pitch_shift_t *pitch)
{
    if (!pitch) return;
    if (pitch->buffer) free(pitch->buffer);
    free(pitch);
}

voice_error_t voice_pitch_shift_process(voice_pitch_shift_t *pitch, float *samples, size_t count)
{
    if (!pitch || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float ratio = pitch->pitch_ratio;
    
    for (size_t i = 0; i < count; i++) {
        /* Write input to buffer */
        pitch->buffer[pitch->write_position] = samples[i];
        pitch->write_position++;
        if (pitch->write_position >= pitch->buffer_size) {
            pitch->write_position = 0;
        }
        
        /* Read with interpolation */
        size_t read_idx = (size_t)pitch->read_position;
        float frac = pitch->read_position - read_idx;
        
        size_t idx0 = read_idx % pitch->buffer_size;
        size_t idx1 = (read_idx + 1) % pitch->buffer_size;
        
        float output = pitch->buffer[idx0] * (1.0f - frac) + pitch->buffer[idx1] * frac;
        
        /* Advance read position by pitch ratio */
        pitch->read_position += ratio;
        
        /* Wrap and crossfade to avoid clicks */
        float buffer_lag = (float)pitch->write_position - pitch->read_position;
        if (buffer_lag < 0) buffer_lag += pitch->buffer_size;
        
        if (buffer_lag < pitch->config.frame_size / 2 || 
            buffer_lag > pitch->buffer_size - pitch->config.frame_size / 2) {
            /* Reset read position to maintain lag */
            pitch->read_position = (float)pitch->write_position - pitch->config.frame_size;
            if (pitch->read_position < 0) pitch->read_position += pitch->buffer_size;
        }
        
        samples[i] = output;
    }
    
    return VOICE_OK;
}

voice_error_t voice_pitch_shift_process_int16(voice_pitch_shift_t *pitch, int16_t *samples, size_t count)
{
    if (!pitch || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_pitch_shift_process(pitch, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

void voice_pitch_shift_set_shift(voice_pitch_shift_t *pitch, float semitones, float cents)
{
    if (!pitch) return;
    pitch->config.semitones = semitones;
    pitch->config.cents = cents;
    float total = semitones + cents / 100.0f;
    pitch->pitch_ratio = powf(2.0f, total / 12.0f);
}

void voice_pitch_shift_reset(voice_pitch_shift_t *pitch)
{
    if (!pitch) return;
    memset(pitch->buffer, 0, pitch->buffer_size * sizeof(float));
    pitch->read_position = 0.0f;
    pitch->write_position = 0;
}

/* ============================================
 * Chorus Implementation
 * ============================================ */

struct voice_chorus_s {
    voice_chorus_config_t config;
    float *buffer;
    size_t buffer_size;
    size_t write_index;
    float *lfo_phases;
};

void voice_chorus_config_init(voice_chorus_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_chorus_config_t));
    config->sample_rate = 48000;
    config->voices = 3;
    config->rate = 0.5f;
    config->depth = 3.0f;
    config->mix = 0.5f;
    config->feedback = 0.0f;
    config->delay_ms = 20.0f;
}

voice_chorus_t *voice_chorus_create(const voice_chorus_config_t *config)
{
    if (!config) return NULL;
    
    voice_chorus_t *chorus = (voice_chorus_t *)calloc(1, sizeof(voice_chorus_t));
    if (!chorus) return NULL;
    
    chorus->config = *config;
    
    /* Buffer for max delay + depth */
    size_t max_delay_samples = (size_t)((config->delay_ms + config->depth * 2) * 
                                         config->sample_rate / 1000.0f);
    chorus->buffer_size = max_delay_samples + 1024;
    chorus->buffer = (float *)calloc(chorus->buffer_size, sizeof(float));
    
    chorus->lfo_phases = (float *)calloc(config->voices, sizeof(float));
    
    if (!chorus->buffer || !chorus->lfo_phases) {
        if (chorus->buffer) free(chorus->buffer);
        if (chorus->lfo_phases) free(chorus->lfo_phases);
        free(chorus);
        return NULL;
    }
    
    /* Initialize LFO phases with spread */
    for (int v = 0; v < config->voices; v++) {
        chorus->lfo_phases[v] = 2.0f * (float)M_PI * v / config->voices;
    }
    
    chorus->write_index = 0;
    
    return chorus;
}

void voice_chorus_destroy(voice_chorus_t *chorus)
{
    if (!chorus) return;
    if (chorus->buffer) free(chorus->buffer);
    if (chorus->lfo_phases) free(chorus->lfo_phases);
    free(chorus);
}

voice_error_t voice_chorus_process(voice_chorus_t *chorus, float *samples, size_t count)
{
    if (!chorus || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float base_delay = chorus->config.delay_ms * chorus->config.sample_rate / 1000.0f;
    float depth_samples = chorus->config.depth * chorus->config.sample_rate / 1000.0f;
    float lfo_inc = 2.0f * (float)M_PI * chorus->config.rate / chorus->config.sample_rate;
    float mix = chorus->config.mix;
    int voices = chorus->config.voices;
    
    for (size_t i = 0; i < count; i++) {
        float input = samples[i];
        float output = 0.0f;
        
        /* Write to buffer */
        chorus->buffer[chorus->write_index] = input;
        
        /* Sum all chorus voices */
        for (int v = 0; v < voices; v++) {
            float mod = sinf(chorus->lfo_phases[v]) * depth_samples;
            chorus->lfo_phases[v] += lfo_inc;
            if (chorus->lfo_phases[v] > 2.0f * (float)M_PI) {
                chorus->lfo_phases[v] -= 2.0f * (float)M_PI;
            }
            
            float read_pos = (float)chorus->write_index - base_delay - mod;
            if (read_pos < 0) read_pos += chorus->buffer_size;
            
            size_t read_idx = (size_t)read_pos;
            float frac = read_pos - read_idx;
            
            size_t next_idx = (read_idx + 1) % chorus->buffer_size;
            read_idx = read_idx % chorus->buffer_size;
            
            output += chorus->buffer[read_idx] * (1.0f - frac) + 
                     chorus->buffer[next_idx] * frac;
        }
        
        output /= voices;  /* Normalize */
        
        chorus->write_index++;
        if (chorus->write_index >= chorus->buffer_size) {
            chorus->write_index = 0;
        }
        
        samples[i] = input * (1.0f - mix) + output * mix;
    }
    
    return VOICE_OK;
}

voice_error_t voice_chorus_process_int16(voice_chorus_t *chorus, int16_t *samples, size_t count)
{
    if (!chorus || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_chorus_process(chorus, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

void voice_chorus_reset(voice_chorus_t *chorus)
{
    if (!chorus) return;
    memset(chorus->buffer, 0, chorus->buffer_size * sizeof(float));
    chorus->write_index = 0;
    for (int v = 0; v < chorus->config.voices; v++) {
        chorus->lfo_phases[v] = 2.0f * (float)M_PI * v / chorus->config.voices;
    }
}

/* ============================================
 * Flanger Implementation
 * ============================================ */

struct voice_flanger_s {
    voice_flanger_config_t config;
    float *buffer;
    size_t buffer_size;
    size_t write_index;
    float lfo_phase;
};

void voice_flanger_config_init(voice_flanger_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(voice_flanger_config_t));
    config->sample_rate = 48000;
    config->rate = 0.25f;
    config->depth = 0.5f;
    config->mix = 0.5f;
    config->feedback = 0.5f;
    config->delay_ms = 5.0f;
}

voice_flanger_t *voice_flanger_create(const voice_flanger_config_t *config)
{
    if (!config) return NULL;
    
    voice_flanger_t *flanger = (voice_flanger_t *)calloc(1, sizeof(voice_flanger_t));
    if (!flanger) return NULL;
    
    flanger->config = *config;
    
    /* Buffer for max delay */
    flanger->buffer_size = (size_t)(config->delay_ms * 2 * config->sample_rate / 1000.0f) + 256;
    flanger->buffer = (float *)calloc(flanger->buffer_size, sizeof(float));
    if (!flanger->buffer) {
        free(flanger);
        return NULL;
    }
    
    flanger->write_index = 0;
    flanger->lfo_phase = 0.0f;
    
    return flanger;
}

void voice_flanger_destroy(voice_flanger_t *flanger)
{
    if (!flanger) return;
    if (flanger->buffer) free(flanger->buffer);
    free(flanger);
}

voice_error_t voice_flanger_process(voice_flanger_t *flanger, float *samples, size_t count)
{
    if (!flanger || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float max_delay = flanger->config.delay_ms * flanger->config.sample_rate / 1000.0f;
    float lfo_inc = 2.0f * (float)M_PI * flanger->config.rate / flanger->config.sample_rate;
    float mix = flanger->config.mix;
    float feedback = flanger->config.feedback;
    float depth = flanger->config.depth;
    
    for (size_t i = 0; i < count; i++) {
        /* LFO modulation (0 to max_delay) */
        float mod = (sinf(flanger->lfo_phase) + 1.0f) * 0.5f * max_delay * depth;
        flanger->lfo_phase += lfo_inc;
        if (flanger->lfo_phase > 2.0f * (float)M_PI) {
            flanger->lfo_phase -= 2.0f * (float)M_PI;
        }
        
        /* Read position */
        float read_pos = (float)flanger->write_index - mod - 1.0f;
        if (read_pos < 0) read_pos += flanger->buffer_size;
        
        /* Interpolated read */
        size_t read_idx = (size_t)read_pos;
        float frac = read_pos - read_idx;
        
        size_t next_idx = (read_idx + 1) % flanger->buffer_size;
        read_idx = read_idx % flanger->buffer_size;
        
        float delayed = flanger->buffer[read_idx] * (1.0f - frac) + 
                       flanger->buffer[next_idx] * frac;
        
        /* Write with feedback */
        float input = samples[i];
        flanger->buffer[flanger->write_index] = input + delayed * feedback;
        
        flanger->write_index++;
        if (flanger->write_index >= flanger->buffer_size) {
            flanger->write_index = 0;
        }
        
        /* Output mix */
        samples[i] = input * (1.0f - mix) + delayed * mix;
    }
    
    return VOICE_OK;
}

voice_error_t voice_flanger_process_int16(voice_flanger_t *flanger, int16_t *samples, size_t count)
{
    if (!flanger || !samples) return VOICE_ERROR_INVALID_PARAM;
    
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_flanger_process(flanger, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

void voice_flanger_reset(voice_flanger_t *flanger)
{
    if (!flanger) return;
    memset(flanger->buffer, 0, flanger->buffer_size * sizeof(float));
    flanger->write_index = 0;
    flanger->lfo_phase = 0.0f;
}

/* ============================================
 * Effects Chain Implementation
 * ============================================ */

#define MAX_CHAIN_EFFECTS 16

typedef struct {
    voice_effect_type_t type;
    void *effect;
} chain_effect_t;

struct voice_effect_chain_s {
    uint32_t sample_rate;
    chain_effect_t effects[MAX_CHAIN_EFFECTS];
    size_t count;
    bool bypass;
};

voice_effect_chain_t *voice_effect_chain_create(uint32_t sample_rate)
{
    voice_effect_chain_t *chain = (voice_effect_chain_t *)calloc(1, sizeof(voice_effect_chain_t));
    if (!chain) return NULL;
    
    chain->sample_rate = sample_rate;
    chain->count = 0;
    chain->bypass = false;
    
    return chain;
}

void voice_effect_chain_destroy(voice_effect_chain_t *chain)
{
    if (!chain) return;
    voice_effect_chain_clear(chain);
    free(chain);
}

voice_error_t voice_effect_chain_add(voice_effect_chain_t *chain, 
                                      voice_effect_type_t type, void *config)
{
    if (!chain || chain->count >= MAX_CHAIN_EFFECTS) {
        return VOICE_ERROR_OVERFLOW;
    }
    
    void *effect = NULL;
    
    switch (type) {
        case VOICE_EFFECT_REVERB:
            effect = voice_reverb_create((voice_reverb_config_t *)config);
            break;
        case VOICE_EFFECT_DELAY:
            effect = voice_delay_create((voice_delay_config_t *)config);
            break;
        case VOICE_EFFECT_PITCH_SHIFT:
            effect = voice_pitch_shift_create((voice_pitch_shift_config_t *)config);
            break;
        case VOICE_EFFECT_CHORUS:
            effect = voice_chorus_create((voice_chorus_config_t *)config);
            break;
        case VOICE_EFFECT_FLANGER:
            effect = voice_flanger_create((voice_flanger_config_t *)config);
            break;
        default:
            return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (!effect) return VOICE_ERROR_OUT_OF_MEMORY;
    
    chain->effects[chain->count].type = type;
    chain->effects[chain->count].effect = effect;
    chain->count++;
    
    return VOICE_OK;
}

voice_error_t voice_effect_chain_remove(voice_effect_chain_t *chain, size_t index)
{
    if (!chain || index >= chain->count) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    /* Destroy the effect */
    switch (chain->effects[index].type) {
        case VOICE_EFFECT_REVERB:
            voice_reverb_destroy((voice_reverb_t *)chain->effects[index].effect);
            break;
        case VOICE_EFFECT_DELAY:
            voice_delay_destroy((voice_delay_t *)chain->effects[index].effect);
            break;
        case VOICE_EFFECT_PITCH_SHIFT:
            voice_pitch_shift_destroy((voice_pitch_shift_t *)chain->effects[index].effect);
            break;
        case VOICE_EFFECT_CHORUS:
            voice_chorus_destroy((voice_chorus_t *)chain->effects[index].effect);
            break;
        case VOICE_EFFECT_FLANGER:
            voice_flanger_destroy((voice_flanger_t *)chain->effects[index].effect);
            break;
    }
    
    /* Shift remaining effects */
    for (size_t i = index; i < chain->count - 1; i++) {
        chain->effects[i] = chain->effects[i + 1];
    }
    chain->count--;
    
    return VOICE_OK;
}

void voice_effect_chain_clear(voice_effect_chain_t *chain)
{
    if (!chain) return;
    
    while (chain->count > 0) {
        voice_effect_chain_remove(chain, chain->count - 1);
    }
}

voice_error_t voice_effect_chain_process(voice_effect_chain_t *chain, 
                                          float *samples, size_t count)
{
    if (!chain || !samples) return VOICE_ERROR_INVALID_PARAM;
    if (chain->bypass) return VOICE_OK;
    
    for (size_t i = 0; i < chain->count; i++) {
        voice_error_t err = VOICE_OK;
        
        switch (chain->effects[i].type) {
            case VOICE_EFFECT_REVERB:
                err = voice_reverb_process((voice_reverb_t *)chain->effects[i].effect, 
                                           samples, count);
                break;
            case VOICE_EFFECT_DELAY:
                err = voice_delay_process((voice_delay_t *)chain->effects[i].effect, 
                                          samples, count);
                break;
            case VOICE_EFFECT_PITCH_SHIFT:
                err = voice_pitch_shift_process((voice_pitch_shift_t *)chain->effects[i].effect, 
                                                samples, count);
                break;
            case VOICE_EFFECT_CHORUS:
                err = voice_chorus_process((voice_chorus_t *)chain->effects[i].effect, 
                                           samples, count);
                break;
            case VOICE_EFFECT_FLANGER:
                err = voice_flanger_process((voice_flanger_t *)chain->effects[i].effect, 
                                            samples, count);
                break;
        }
        
        if (err != VOICE_OK) return err;
    }
    
    return VOICE_OK;
}

voice_error_t voice_effect_chain_process_int16(voice_effect_chain_t *chain, 
                                                int16_t *samples, size_t count)
{
    if (!chain || !samples) return VOICE_ERROR_INVALID_PARAM;
    if (chain->bypass) return VOICE_OK;
    
    float *temp = (float *)malloc(count * sizeof(float));
    if (!temp) return VOICE_ERROR_OUT_OF_MEMORY;
    
    for (size_t i = 0; i < count; i++) {
        temp[i] = samples[i] / 32768.0f;
    }
    
    voice_error_t err = voice_effect_chain_process(chain, temp, count);
    
    for (size_t i = 0; i < count; i++) {
        float v = temp[i] * 32768.0f;
        if (v > 32767.0f) v = 32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        samples[i] = (int16_t)v;
    }
    
    free(temp);
    return err;
}

void voice_effect_chain_set_bypass(voice_effect_chain_t *chain, bool bypass)
{
    if (!chain) return;
    chain->bypass = bypass;
}

void voice_effect_chain_reset(voice_effect_chain_t *chain)
{
    if (!chain) return;
    
    for (size_t i = 0; i < chain->count; i++) {
        switch (chain->effects[i].type) {
            case VOICE_EFFECT_REVERB:
                voice_reverb_reset((voice_reverb_t *)chain->effects[i].effect);
                break;
            case VOICE_EFFECT_DELAY:
                voice_delay_reset((voice_delay_t *)chain->effects[i].effect);
                break;
            case VOICE_EFFECT_PITCH_SHIFT:
                voice_pitch_shift_reset((voice_pitch_shift_t *)chain->effects[i].effect);
                break;
            case VOICE_EFFECT_CHORUS:
                voice_chorus_reset((voice_chorus_t *)chain->effects[i].effect);
                break;
            case VOICE_EFFECT_FLANGER:
                voice_flanger_reset((voice_flanger_t *)chain->effects[i].effect);
                break;
        }
    }
}
