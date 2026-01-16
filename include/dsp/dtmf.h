/**
 * @file dtmf.h
 * @brief DTMF (Dual-Tone Multi-Frequency) detection and generation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * DTMF signal detection and generation module.
 * Used for telephone system keypad tone encoding/decoding.
 */

#ifndef VOICE_DTMF_H
#define VOICE_DTMF_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * DTMF Character Definitions
 * ============================================ */

/**
 * DTMF 字符
 * 标准按键: 0-9, *, #
 * 扩展按键: A, B, C, D
 */
typedef enum {
    VOICE_DTMF_0 = '0',
    VOICE_DTMF_1 = '1',
    VOICE_DTMF_2 = '2',
    VOICE_DTMF_3 = '3',
    VOICE_DTMF_4 = '4',
    VOICE_DTMF_5 = '5',
    VOICE_DTMF_6 = '6',
    VOICE_DTMF_7 = '7',
    VOICE_DTMF_8 = '8',
    VOICE_DTMF_9 = '9',
    VOICE_DTMF_STAR = '*',
    VOICE_DTMF_HASH = '#',
    VOICE_DTMF_A = 'A',
    VOICE_DTMF_B = 'B',
    VOICE_DTMF_C = 'C',
    VOICE_DTMF_D = 'D',
    VOICE_DTMF_NONE = 0,
} voice_dtmf_digit_t;

/* ============================================
 * DTMF Detector
 * ============================================ */

typedef struct voice_dtmf_detector_s voice_dtmf_detector_t;

/** Detector configuration */
typedef struct {
    uint32_t sample_rate;
    uint32_t frame_size;

    /* Goertzel parameters */
    float detection_threshold;      /**< Detection threshold */
    float twist_threshold;          /**< High/low frequency energy ratio threshold */
    float reverse_twist_threshold;  /**< Reverse twist threshold */

    /* Time parameters */
    uint32_t min_on_time_ms;        /**< Minimum key press duration */
    uint32_t min_off_time_ms;       /**< Minimum key interval time */

    /* Callback */
    void (*on_digit)(voice_dtmf_digit_t digit, uint32_t duration_ms, void *user_data);
    void *callback_user_data;
} voice_dtmf_detector_config_t;

/** Detection result */
typedef struct {
    voice_dtmf_digit_t digit;       /**< Detected digit */
    bool valid;                     /**< Whether valid */
    float low_freq_energy;          /**< Low frequency energy */
    float high_freq_energy;         /**< High frequency energy */
    float twist;                    /**< Energy ratio */
    uint32_t duration_ms;           /**< Duration */
} voice_dtmf_result_t;

/* ============================================
 * DTMF Generator
 * ============================================ */

typedef struct voice_dtmf_generator_s voice_dtmf_generator_t;

/** Generator configuration */
typedef struct {
    uint32_t sample_rate;
    float amplitude;                /**< Amplitude (0.0-1.0) */
    float low_freq_gain;            /**< Low frequency gain adjustment */
    float high_freq_gain;           /**< High frequency gain adjustment */
    uint32_t tone_duration_ms;      /**< Tone duration */
    uint32_t pause_duration_ms;     /**< Tone interval time */
} voice_dtmf_generator_config_t;

/* ============================================
 * DTMF Detector API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_dtmf_detector_config_init(voice_dtmf_detector_config_t *config);

/**
 * @brief Create detector
 */
VOICE_API voice_dtmf_detector_t *voice_dtmf_detector_create(
    const voice_dtmf_detector_config_t *config
);

/**
 * @brief Destroy detector
 */
VOICE_API void voice_dtmf_detector_destroy(voice_dtmf_detector_t *detector);

/**
 * @brief Process audio frame
 * @param detector Detector
 * @param samples Audio samples
 * @param num_samples Number of samples
 * @param result Detection result (optional)
 * @return Detected digit, VOICE_DTMF_NONE if none detected
 */
VOICE_API voice_dtmf_digit_t voice_dtmf_detector_process(
    voice_dtmf_detector_t *detector,
    const int16_t *samples,
    size_t num_samples,
    voice_dtmf_result_t *result
);

/**
 * @brief Reset detector
 */
VOICE_API void voice_dtmf_detector_reset(voice_dtmf_detector_t *detector);

/**
 * @brief Get detected digit sequence
 * @param detector Detector
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of digits
 */
VOICE_API size_t voice_dtmf_detector_get_digits(
    voice_dtmf_detector_t *detector,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Clear digit buffer
 */
VOICE_API void voice_dtmf_detector_clear_digits(voice_dtmf_detector_t *detector);

/* ============================================
 * DTMF Generator API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_dtmf_generator_config_init(voice_dtmf_generator_config_t *config);

/**
 * @brief Create generator
 */
VOICE_API voice_dtmf_generator_t *voice_dtmf_generator_create(
    const voice_dtmf_generator_config_t *config
);

/**
 * @brief Destroy generator
 */
VOICE_API void voice_dtmf_generator_destroy(voice_dtmf_generator_t *generator);

/**
 * @brief Generate single DTMF tone
 * @param generator Generator
 * @param digit Digit
 * @param output Output buffer
 * @param num_samples Number of samples
 * @return Generated sample count
 */
VOICE_API size_t voice_dtmf_generator_generate(
    voice_dtmf_generator_t *generator,
    voice_dtmf_digit_t digit,
    int16_t *output,
    size_t num_samples
);

/**
 * @brief Generate DTMF sequence
 * @param generator Generator
 * @param digits Digit sequence (null-terminated)
 * @param output Output buffer
 * @param max_samples Buffer size
 * @return Generated sample count
 */
VOICE_API size_t voice_dtmf_generator_generate_sequence(
    voice_dtmf_generator_t *generator,
    const char *digits,
    int16_t *output,
    size_t max_samples
);

/**
 * @brief Reset generator
 */
VOICE_API void voice_dtmf_generator_reset(voice_dtmf_generator_t *generator);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Validate if DTMF character is valid
 */
VOICE_API bool voice_dtmf_is_valid_digit(char c);

/**
 * @brief Get DTMF frequencies
 * @param digit Digit
 * @param low_freq Output low frequency
 * @param high_freq Output high frequency
 */
VOICE_API void voice_dtmf_get_frequencies(
    voice_dtmf_digit_t digit,
    float *low_freq,
    float *high_freq
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_DTMF_H */
