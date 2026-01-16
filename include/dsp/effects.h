/**
 * @file effects.h
 * @brief Audio effects processing (reverb, delay, pitch shift, chorus, flanger)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef DSP_EFFECTS_H
#define DSP_EFFECTS_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Reverb Effect
 * ============================================ */

/** Reverb type */
typedef enum {
    VOICE_REVERB_ROOM,          /**< Small room */
    VOICE_REVERB_HALL,          /**< Concert hall */
    VOICE_REVERB_PLATE,         /**< Plate reverb */
    VOICE_REVERB_CHAMBER,       /**< Reverb chamber */
    VOICE_REVERB_CATHEDRAL      /**< Cathedral */
} voice_reverb_type_t;

/** Reverb configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate */
    voice_reverb_type_t type;   /**< Reverb type */
    float room_size;            /**< Room size (0.0-1.0) */
    float damping;              /**< Damping (0.0-1.0) */
    float wet_level;            /**< Wet level ratio (0.0-1.0) */
    float dry_level;            /**< Dry level ratio (0.0-1.0) */
    float width;                /**< Stereo width (0.0-1.0) */
    float pre_delay_ms;         /**< Pre-delay (milliseconds) */
} voice_reverb_config_t;

/** Reverb handle */
typedef struct voice_reverb_s voice_reverb_t;

/**
 * @brief Initialize default reverb configuration
 */
VOICE_API void voice_reverb_config_init(voice_reverb_config_t *config);

/**
 * @brief Create reverb effect
 */
VOICE_API voice_reverb_t *voice_reverb_create(const voice_reverb_config_t *config);

/**
 * @brief Destroy reverb effect
 */
VOICE_API void voice_reverb_destroy(voice_reverb_t *reverb);

/**
 * @brief Process audio (in-place)
 */
VOICE_API voice_error_t voice_reverb_process(
    voice_reverb_t *reverb,
    float *samples,
    size_t count
);

/**
 * @brief Process audio (int16)
 */
VOICE_API voice_error_t voice_reverb_process_int16(
    voice_reverb_t *reverb,
    int16_t *samples,
    size_t count
);

/**
 * @brief Set reverb parameters
 */
VOICE_API void voice_reverb_set_room_size(voice_reverb_t *reverb, float size);
VOICE_API void voice_reverb_set_damping(voice_reverb_t *reverb, float damping);
VOICE_API void voice_reverb_set_wet_level(voice_reverb_t *reverb, float level);
VOICE_API void voice_reverb_set_dry_level(voice_reverb_t *reverb, float level);

/**
 * @brief Reset reverb state
 */
VOICE_API void voice_reverb_reset(voice_reverb_t *reverb);

/* ============================================
 * Delay Effect
 * ============================================ */

/** Delay type */
typedef enum {
    VOICE_DELAY_SIMPLE,         /**< Simple delay */
    VOICE_DELAY_PINGPONG,       /**< Ping-pong delay (stereo) */
    VOICE_DELAY_TAPE,           /**< Tape delay (with modulation) */
    VOICE_DELAY_MULTI_TAP       /**< Multi-tap delay */
} voice_delay_type_t;

/** Delay configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate */
    voice_delay_type_t type;    /**< Delay type */
    float delay_ms;             /**< Delay time (milliseconds) */
    float feedback;             /**< Feedback amount (0.0-1.0) */
    float mix;                  /**< Mix ratio (0.0-1.0) */
    float modulation_rate;      /**< Modulation rate (Hz, tape delay only) */
    float modulation_depth;     /**< Modulation depth (0.0-1.0) */
    uint32_t max_delay_ms;      /**< Maximum delay (milliseconds) */
} voice_delay_config_t;

/** Delay handle */
typedef struct voice_delay_s voice_delay_t;

/**
 * @brief Initialize default delay configuration
 */
VOICE_API void voice_delay_config_init(voice_delay_config_t *config);

/**
 * @brief Create delay effect
 */
VOICE_API voice_delay_t *voice_delay_create(const voice_delay_config_t *config);

/**
 * @brief Destroy delay effect
 */
VOICE_API void voice_delay_destroy(voice_delay_t *delay);

/**
 * @brief Process audio (in-place)
 */
VOICE_API voice_error_t voice_delay_process(
    voice_delay_t *delay,
    float *samples,
    size_t count
);

/**
 * @brief Process audio (int16)
 */
VOICE_API voice_error_t voice_delay_process_int16(
    voice_delay_t *delay,
    int16_t *samples,
    size_t count
);

/**
 * @brief Set delay time
 */
VOICE_API void voice_delay_set_time(voice_delay_t *delay, float delay_ms);

/**
 * @brief Set feedback amount
 */
VOICE_API void voice_delay_set_feedback(voice_delay_t *delay, float feedback);

/**
 * @brief Reset delay state
 */
VOICE_API void voice_delay_reset(voice_delay_t *delay);

/* ============================================
 * Pitch Shift
 * ============================================ */

/** Pitch shift configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate */
    float semitones;            /**< Semitone shift (-12 to +12) */
    float cents;                /**< Cents fine-tune (-100 to +100) */
    uint32_t frame_size;        /**< Frame size (samples) */
    uint32_t overlap;           /**< Overlap factor (usually 4 or 8) */
    bool preserve_formants;     /**< Preserve formants (for voice) */
} voice_pitch_shift_config_t;

/** Pitch shift handle */
typedef struct voice_pitch_shift_s voice_pitch_shift_t;

/**
 * @brief Initialize default pitch shift configuration
 */
VOICE_API void voice_pitch_shift_config_init(voice_pitch_shift_config_t *config);

/**
 * @brief Create pitch shifter
 */
VOICE_API voice_pitch_shift_t *voice_pitch_shift_create(const voice_pitch_shift_config_t *config);

/**
 * @brief Destroy pitch shifter
 */
VOICE_API void voice_pitch_shift_destroy(voice_pitch_shift_t *pitch);

/**
 * @brief Process audio (in-place)
 */
VOICE_API voice_error_t voice_pitch_shift_process(
    voice_pitch_shift_t *pitch,
    float *samples,
    size_t count
);

/**
 * @brief Process audio (int16)
 */
VOICE_API voice_error_t voice_pitch_shift_process_int16(
    voice_pitch_shift_t *pitch,
    int16_t *samples,
    size_t count
);

/**
 * @brief Set pitch shift
 * @param semitones Semitones (-12 to +12)
 * @param cents Cents (-100 to +100)
 */
VOICE_API void voice_pitch_shift_set_shift(
    voice_pitch_shift_t *pitch,
    float semitones,
    float cents
);

/**
 * @brief Reset pitch shifter state
 */
VOICE_API void voice_pitch_shift_reset(voice_pitch_shift_t *pitch);

/* ============================================
 * Chorus Effect
 * ============================================ */

/** Chorus configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate */
    uint8_t voices;             /**< Number of voices (1-8) */
    float rate;                 /**< LFO frequency (Hz) */
    float depth;                /**< Modulation depth (milliseconds) */
    float mix;                  /**< Mix ratio (0.0-1.0) */
    float feedback;             /**< Feedback amount (0.0-1.0) */
    float delay_ms;             /**< Base delay (milliseconds) */
} voice_chorus_config_t;

/** Chorus handle */
typedef struct voice_chorus_s voice_chorus_t;

/**
 * @brief Initialize default chorus configuration
 */
VOICE_API void voice_chorus_config_init(voice_chorus_config_t *config);

/**
 * @brief Create chorus effect
 */
VOICE_API voice_chorus_t *voice_chorus_create(const voice_chorus_config_t *config);

/**
 * @brief Destroy chorus effect
 */
VOICE_API void voice_chorus_destroy(voice_chorus_t *chorus);

/**
 * @brief Process audio (in-place)
 */
VOICE_API voice_error_t voice_chorus_process(
    voice_chorus_t *chorus,
    float *samples,
    size_t count
);

/**
 * @brief Process audio (int16)
 */
VOICE_API voice_error_t voice_chorus_process_int16(
    voice_chorus_t *chorus,
    int16_t *samples,
    size_t count
);

/**
 * @brief Reset chorus state
 */
VOICE_API void voice_chorus_reset(voice_chorus_t *chorus);

/* ============================================
 * Flanger Effect
 * ============================================ */

/** Flanger configuration */
typedef struct {
    uint32_t sample_rate;       /**< Sample rate */
    float rate;                 /**< LFO frequency (Hz) */
    float depth;                /**< Modulation depth (0.0-1.0) */
    float mix;                  /**< Mix ratio (0.0-1.0) */
    float feedback;             /**< Feedback amount (-1.0 to 1.0) */
    float delay_ms;             /**< Base delay (milliseconds, usually 1-10) */
} voice_flanger_config_t;

/** Flanger handle */
typedef struct voice_flanger_s voice_flanger_t;

/**
 * @brief Initialize default flanger configuration
 */
VOICE_API void voice_flanger_config_init(voice_flanger_config_t *config);

/**
 * @brief Create flanger effect
 */
VOICE_API voice_flanger_t *voice_flanger_create(const voice_flanger_config_t *config);

/**
 * @brief Destroy flanger effect
 */
VOICE_API void voice_flanger_destroy(voice_flanger_t *flanger);

/**
 * @brief Process audio (in-place)
 */
VOICE_API voice_error_t voice_flanger_process(
    voice_flanger_t *flanger,
    float *samples,
    size_t count
);

/**
 * @brief Process audio (int16)
 */
VOICE_API voice_error_t voice_flanger_process_int16(
    voice_flanger_t *flanger,
    int16_t *samples,
    size_t count
);

/**
 * @brief Reset flanger state
 */
VOICE_API void voice_flanger_reset(voice_flanger_t *flanger);

/* ============================================
 * Effects Chain
 * ============================================ */

/** Effect type */
typedef enum {
    VOICE_EFFECT_REVERB,
    VOICE_EFFECT_DELAY,
    VOICE_EFFECT_PITCH_SHIFT,
    VOICE_EFFECT_CHORUS,
    VOICE_EFFECT_FLANGER
} voice_effect_type_t;

/** Effects chain handle */
typedef struct voice_effect_chain_s voice_effect_chain_t;

/**
 * @brief Create effects chain
 */
VOICE_API voice_effect_chain_t *voice_effect_chain_create(uint32_t sample_rate);

/**
 * @brief Destroy effects chain
 */
VOICE_API void voice_effect_chain_destroy(voice_effect_chain_t *chain);

/**
 * @brief Add effect to chain
 */
VOICE_API voice_error_t voice_effect_chain_add(
    voice_effect_chain_t *chain,
    voice_effect_type_t type,
    void *config
);

/**
 * @brief Remove effect
 */
VOICE_API voice_error_t voice_effect_chain_remove(
    voice_effect_chain_t *chain,
    size_t index
);

/**
 * @brief Clear effects chain
 */
VOICE_API void voice_effect_chain_clear(voice_effect_chain_t *chain);

/**
 * @brief Process audio through effects chain
 */
VOICE_API voice_error_t voice_effect_chain_process(
    voice_effect_chain_t *chain,
    float *samples,
    size_t count
);

/**
 * @brief Process audio (int16)
 */
VOICE_API voice_error_t voice_effect_chain_process_int16(
    voice_effect_chain_t *chain,
    int16_t *samples,
    size_t count
);

/**
 * @brief Bypass effects chain
 */
VOICE_API void voice_effect_chain_set_bypass(voice_effect_chain_t *chain, bool bypass);

/**
 * @brief Reset effects chain
 */
VOICE_API void voice_effect_chain_reset(voice_effect_chain_t *chain);

#ifdef __cplusplus
}
#endif

#endif /* DSP_EFFECTS_H */
