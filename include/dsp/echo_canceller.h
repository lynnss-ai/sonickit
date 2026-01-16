/**
 * @file echo_canceller.h
 * @brief Acoustic Echo Cancellation (AEC) interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Phase 1 Enhancement:
 * - Frequency-domain NLMS adaptive filter
 * - Double-talk detection (DTD)
 * - Integrated delay estimator (GCC-PHAT)
 */

#ifndef DSP_ECHO_CANCELLER_H
#define DSP_ECHO_CANCELLER_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/config.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Echo Canceller
 * ============================================ */

/** Echo canceller handle */
typedef struct voice_aec_s voice_aec_t;

/** AEC algorithm type */
typedef enum {
    VOICE_AEC_ALG_NLMS,         /**< Time-domain NLMS */
    VOICE_AEC_ALG_FDAF,         /**< Frequency-domain adaptive filter (recommended) */
    VOICE_AEC_ALG_SPEEX,        /**< SpeexDSP backend (if available) */
} voice_aec_algorithm_t;

/** Double-talk detection state */
typedef enum {
    VOICE_DTD_IDLE,             /**< No activity */
    VOICE_DTD_FAR_END,          /**< Far-end only speaking */
    VOICE_DTD_NEAR_END,         /**< Near-end only speaking */
    VOICE_DTD_DOUBLE_TALK,      /**< Double-talk */
} voice_dtd_state_t;

/** Echo canceller extended configuration (includes additional options) */
typedef struct {
    uint32_t sample_rate;               /**< Sample rate */
    uint32_t frame_size;                /**< Frame size (samples) */
    uint32_t filter_length;             /**< Filter length (samples) */
    int echo_suppress_db;               /**< Echo suppression amount (negative dB) */
    int echo_suppress_active_db;        /**< Suppression amount when near-end active */
    bool enable_residual_echo_suppress; /**< Enable residual echo suppression */
    bool enable_comfort_noise;          /**< Enable comfort noise */

    /* Phase 1 new configuration */
    voice_aec_algorithm_t algorithm;    /**< Algorithm type */
    bool enable_delay_estimation;       /**< Enable automatic delay estimation */
    bool enable_dtd;                    /**< Enable double-talk detection */
    uint32_t tail_length_ms;            /**< Echo tail length (ms), default 200 */
    float nlms_step_size;               /**< NLMS step size (0.1-1.0) */
    float dtd_threshold;                /**< DTD threshold (0.5-0.9) */
} voice_aec_ext_config_t;

/** AEC state information */
typedef struct {
    voice_dtd_state_t dtd_state;        /**< Current double-talk state */
    int32_t estimated_delay_samples;    /**< Estimated delay (samples) */
    float   estimated_delay_ms;         /**< Estimated delay (ms) */
    float   erle_db;                    /**< Echo Return Loss Enhancement (dB) */
    float   convergence;                /**< Filter convergence (0-1) */
    bool    delay_stable;               /**< Whether delay estimate is stable */
    uint64_t frames_processed;          /**< Frames processed */
} voice_aec_state_t;

/**
 * @brief Initialize default configuration
 * @param config Configuration struct pointer
 */
VOICE_API void voice_aec_ext_config_init(voice_aec_ext_config_t *config);

/**
 * @brief Create echo canceller
 * @param config Configuration
 * @return Echo canceller handle
 */
VOICE_API voice_aec_t *voice_aec_create(const voice_aec_ext_config_t *config);

/**
 * @brief Destroy echo canceller
 * @param aec Echo canceller handle
 */
VOICE_API void voice_aec_destroy(voice_aec_t *aec);

/**
 * @brief Process echo cancellation (synchronous mode)
 *
 * Requires playback signal and microphone signal to be time-aligned
 *
 * @param aec Echo canceller handle
 * @param mic_input Microphone input (contains echo)
 * @param speaker_ref Speaker reference signal
 * @param output Output (after echo cancellation)
 * @param frame_count Frame count (samples)
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_process(
    voice_aec_t *aec,
    const int16_t *mic_input,
    const int16_t *speaker_ref,
    int16_t *output,
    size_t frame_count
);

/**
 * @brief Playback callback (asynchronous mode)
 *
 * Called in playback thread, caches playback data for AEC
 *
 * @param aec Echo canceller handle
 * @param speaker_data Speaker data
 * @param frame_count Frame count
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_playback(
    voice_aec_t *aec,
    const int16_t *speaker_data,
    size_t frame_count
);

/**
 * @brief Capture callback (asynchronous mode)
 *
 * Called in capture thread, processes echo cancellation
 *
 * @param aec Echo canceller handle
 * @param mic_input Microphone input
 * @param output Output
 * @param frame_count Frame count
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_capture(
    voice_aec_t *aec,
    const int16_t *mic_input,
    int16_t *output,
    size_t frame_count
);

/**
 * @brief Set echo suppression amount
 * @param aec Echo canceller handle
 * @param suppress_db Suppression amount (negative dB)
 * @param suppress_active_db Suppression amount when near-end active
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_set_suppress(
    voice_aec_t *aec,
    int suppress_db,
    int suppress_active_db
);

/**
 * @brief Enable/disable echo cancellation
 * @param aec Echo canceller handle
 * @param enabled Whether to enable
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_set_enabled(voice_aec_t *aec, bool enabled);

/**
 * @brief Check if enabled
 * @param aec Echo canceller handle
 * @return true if enabled
 */
VOICE_API bool voice_aec_is_enabled(voice_aec_t *aec);

/**
 * @brief Reset echo canceller state
 * @param aec Echo canceller handle
 */
VOICE_API void voice_aec_reset(voice_aec_t *aec);

/**
 * @brief Get filter delay estimate
 * @param aec Echo canceller handle
 * @return Delay (samples)
 */
VOICE_API int voice_aec_get_delay(voice_aec_t *aec);

/**
 * @brief Set known system delay
 *
 * Use when precise delay is obtained from hardware API
 *
 * @param aec Echo canceller handle
 * @param delay_samples Delay (samples)
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_set_delay(voice_aec_t *aec, int delay_samples);

/**
 * @brief Get current AEC state
 *
 * @param aec Echo canceller handle
 * @param state State output
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_get_state(voice_aec_t *aec, voice_aec_state_t *state);

/**
 * @brief Get current double-talk detection state
 *
 * @param aec Echo canceller handle
 * @return DTD state
 */
VOICE_API voice_dtd_state_t voice_aec_get_dtd_state(voice_aec_t *aec);

/**
 * @brief Enable/disable delay estimation
 *
 * @param aec Echo canceller handle
 * @param enabled Whether to enable
 * @return Error code
 */
VOICE_API voice_error_t voice_aec_enable_delay_estimation(voice_aec_t *aec, bool enabled);

#ifdef __cplusplus
}
#endif

#endif /* DSP_ECHO_CANCELLER_H */
