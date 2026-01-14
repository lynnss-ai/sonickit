/**
 * @file voice.h
 * @brief Voice library main API header
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Cross-platform real-time voice processing library
 * 
 * Features:
 * - Audio capture and playback (miniaudio)
 * - Acoustic echo cancellation (SpeexDSP AEC)
 * - Noise suppression (SpeexDSP / RNNoise)
 * - Automatic gain control (AGC)
 * - Multi-codec support (Opus / G.711 / G.722)
 * - RTP/RTCP network transport
 * - SRTP encryption (AES-CM / AES-GCM)
 * - DTLS-SRTP key exchange
 * - Audio file I/O (WAV / MP3 / FLAC)
 * 
 * Supported platforms:
 * - Windows (WASAPI)
 * - Linux (ALSA / PulseAudio)
 * - macOS (Core Audio)
 * - iOS (Core Audio / AVAudioSession)
 * - Android (AAudio / OpenSL ES)
 */

#ifndef VOICE_H
#define VOICE_H

#include "types.h"
#include "error.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Version Information
 * ============================================ */

#define VOICE_VERSION_MAJOR 1
#define VOICE_VERSION_MINOR 0
#define VOICE_VERSION_PATCH 0

#define VOICE_VERSION_STRING "1.0.0"

/**
 * @brief Get version string
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return Version string
 */
const char *voice_version(void);

/**
 * @brief Get version numbers
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param major Major version
 * @param minor Minor version
 * @param patch Patch version
 */
void voice_version_get(int *major, int *minor, int *patch);

/* ============================================
 * Library Initialization
 * ============================================ */

/**
 * @brief Initialize voice library
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param config Global configuration (NULL for defaults)
 * @return Error code
 */
voice_error_t voice_init(const voice_global_config_t *config);

/**
 * @brief Release voice library resources
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void voice_deinit(void);

/**
 * @brief Check if library is initialized
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return true if initialized
 */
bool voice_is_initialized(void);

/* ============================================
 * Audio Device Management
 * ============================================ */

/** Device type */
typedef enum {
    VOICE_DEVICE_TYPE_CAPTURE,      /**< Capture device */
    VOICE_DEVICE_TYPE_PLAYBACK      /**< Playback device */
} voice_device_type_t;

/** Device information */
typedef struct {
    char id[256];                   /**< Device ID */
    char name[256];                 /**< Device name */
    voice_device_type_t type;       /**< Device type */
    bool is_default;                /**< Is default device */
    uint32_t min_sample_rate;       /**< Minimum sample rate */
    uint32_t max_sample_rate;       /**< Maximum sample rate */
    uint8_t min_channels;           /**< Minimum channels */
    uint8_t max_channels;           /**< Maximum channels */
} voice_device_info_t;

/**
 * @brief Get audio device count
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param type Device type
 * @return Device count
 */
int voice_device_get_count(voice_device_type_t type);

/**
 * @brief Get audio device information
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param type Device type
 * @param index Device index
 * @param info Device information output
 * @return Error code
 */
voice_error_t voice_device_get_info(
    voice_device_type_t type,
    int index,
    voice_device_info_t *info
);

/**
 * @brief Get default device information
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param type Device type
 * @param info Device information output
 * @return Error code
 */
voice_error_t voice_device_get_default(
    voice_device_type_t type,
    voice_device_info_t *info
);

/* ============================================
 * Audio Processing Pipeline
 * ============================================ */

/** Pipeline handle */
typedef struct voice_pipeline_s voice_pipeline_t;

/**
 * @brief Create audio processing pipeline
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param config Pipeline configuration
 * @return Pipeline handle (NULL on failure)
 */
voice_pipeline_t *voice_pipeline_create(const voice_pipeline_config_t *config);

/**
 * @brief Destroy audio processing pipeline
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 */
void voice_pipeline_destroy(voice_pipeline_t *pipeline);

/**
 * @brief Start audio processing
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @return Error code
 */
voice_error_t voice_pipeline_start(voice_pipeline_t *pipeline);

/**
 * @brief Stop audio processing
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @return Error code
 */
voice_error_t voice_pipeline_stop(voice_pipeline_t *pipeline);

/**
 * @brief Check if pipeline is running
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @return true if running
 */
bool voice_pipeline_is_running(voice_pipeline_t *pipeline);

/**
 * @brief Set denoising engine
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @param engine Denoising engine type
 * @return Error code
 */
voice_error_t voice_pipeline_set_denoise_engine(
    voice_pipeline_t *pipeline,
    voice_denoise_engine_t engine
);

/**
 * @brief Set codec
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @param type Codec type
 * @return Error code
 */
voice_error_t voice_pipeline_set_codec(
    voice_pipeline_t *pipeline,
    voice_codec_type_t type
);

/**
 * @brief Set bit rate
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @param bitrate Bit rate (bps)
 * @return Error code
 */
voice_error_t voice_pipeline_set_bitrate(
    voice_pipeline_t *pipeline,
    uint32_t bitrate
);

/**
 * @brief Enable/disable echo cancellation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @param enabled Enable or not
 * @return Error code
 */
voice_error_t voice_pipeline_set_aec_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief Enable/disable denoising
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @param enabled Enable or not
 * @return Error code
 */
voice_error_t voice_pipeline_set_denoise_enabled(
    voice_pipeline_t *pipeline,
    bool enabled
);

/**
 * @brief Get network statistics
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param pipeline Pipeline handle
 * @param stats Statistics output
 * @return Error code
 */
voice_error_t voice_pipeline_get_network_stats(
    voice_pipeline_t *pipeline,
    voice_network_stats_t *stats
);

/* ============================================
 * Simplified API - Local Recording/Playback
 * ============================================ */

/** Recorder handle */
typedef struct voice_recorder_s voice_recorder_t;

/** Player handle */
typedef struct voice_player_s voice_player_t;

/**
 * @brief Create recorder
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param config Device configuration
 * @param output_file Output file path (NULL to not save file)
 * @return Recorder handle
 */
voice_recorder_t *voice_recorder_create(
    const voice_device_config_t *config,
    const char *output_file
);

/**
 * @brief Destroy recorder
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param recorder Recorder handle
 */
void voice_recorder_destroy(voice_recorder_t *recorder);

/**
 * @brief Start recording
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param recorder Recorder handle
 * @return Error code
 */
voice_error_t voice_recorder_start(voice_recorder_t *recorder);

/**
 * @brief Stop recording
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param recorder Recorder handle
 * @return Error code
 */
voice_error_t voice_recorder_stop(voice_recorder_t *recorder);

/**
 * @brief Set recording data callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param recorder Recorder handle
 * @param callback Callback function
 * @param user_data User data
 */
void voice_recorder_set_callback(
    voice_recorder_t *recorder,
    voice_audio_callback_t callback,
    void *user_data
);

/**
 * @brief Create player
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param config Device configuration
 * @return Player handle
 */
voice_player_t *voice_player_create(const voice_device_config_t *config);

/**
 * @brief Destroy player
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param player Player handle
 */
void voice_player_destroy(voice_player_t *player);

/**
 * @brief Play file
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param player Player handle
 * @param path File path
 * @return Error code
 */
voice_error_t voice_player_play_file(voice_player_t *player, const char *path);

/**
 * @brief Play PCM data
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param player Player handle
 * @param data PCM data
 * @param size Data size
 * @return Error code
 */
voice_error_t voice_player_play_pcm(
    voice_player_t *player,
    const void *data,
    size_t size
);

/**
 * @brief Stop playback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param player Player handle
 * @return Error code
 */
voice_error_t voice_player_stop(voice_player_t *player);

/* ============================================
 * Platform-Specific Functions
 * ============================================ */

/**
 * @brief Get platform name
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return Platform name string
 */
const char *voice_platform_name(void);

/**
 * @brief Get CPU usage
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return CPU usage (0-100)
 */
float voice_platform_get_cpu_usage(void);

/**
 * @brief Get battery level
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return Battery level (0-100, -1 means no battery)
 */
int voice_platform_get_battery_level(void);

/**
 * @brief Check if on battery power
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return true if on battery
 */
bool voice_platform_on_battery(void);

/**
 * @brief Request audio focus (mobile)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return Error code
 */
voice_error_t voice_platform_request_audio_focus(void);

/**
 * @brief Release audio focus (mobile)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @return Error code
 */
voice_error_t voice_platform_release_audio_focus(void);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_H */
