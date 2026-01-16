/**
 * @file device.h
 * @brief Audio device abstraction layer (miniaudio backend)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/config.h"
#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Audio Device Context
 * ============================================ */

/** Device context (global singleton) */
typedef struct voice_device_context_s voice_device_context_t;

/**
 * @brief Initialize device context
 * @return Error code
 */
VOICE_API voice_error_t voice_device_context_init(void);

/**
 * @brief Destroy device context
 */
VOICE_API void voice_device_context_deinit(void);

/**
 * @brief Get device context
 * @return Device context pointer
 */
VOICE_API voice_device_context_t *voice_device_context_get(void);

/* ============================================
 * Audio Device
 * ============================================ */

/** Audio device handle */
typedef struct voice_device_s voice_device_t;

/** Device data callback */
typedef void (*voice_device_data_callback_t)(
    voice_device_t *device,
    void *output,
    const void *input,
    uint32_t frame_count,
    void *user_data
);

/** Device stop callback */
typedef void (*voice_device_stop_callback_t)(
    voice_device_t *device,
    void *user_data
);

/** Device type */
typedef enum {
    VOICE_DEVICE_CAPTURE,           /**< Capture only */
    VOICE_DEVICE_PLAYBACK,          /**< Playback only */
    VOICE_DEVICE_DUPLEX             /**< Full duplex */
} voice_device_mode_t;

/* Compatibility aliases - for example code */
#define VOICE_DEVICE_MODE_CAPTURE   VOICE_DEVICE_CAPTURE
#define VOICE_DEVICE_MODE_PLAYBACK  VOICE_DEVICE_PLAYBACK
#define VOICE_DEVICE_MODE_DUPLEX    VOICE_DEVICE_DUPLEX

/** Capture data callback (simplified) */
typedef void (*voice_capture_callback_t)(
    voice_device_t *device,
    const int16_t *input,
    size_t frame_count,
    void *user_data
);

/** Playback data callback (simplified) */
typedef void (*voice_playback_callback_t)(
    voice_device_t *device,
    int16_t *output,
    size_t frame_count,
    void *user_data
);

/** Device configuration */
typedef struct {
    voice_device_mode_t mode;       /**< Device mode */

    /* Capture configuration */
    struct {
        const char *device_id;      /**< Device ID (NULL=default) */
        voice_format_t format;      /**< Format */
        uint8_t channels;           /**< Channel count */
        uint32_t sample_rate;       /**< Sample rate */
    } capture;

    /* Playback configuration */
    struct {
        const char *device_id;      /**< Device ID (NULL=default) */
        voice_format_t format;      /**< Format */
        uint8_t channels;           /**< Channel count */
        uint32_t sample_rate;       /**< Sample rate */
    } playback;

    uint32_t period_size_frames;    /**< Period size (frames) */
    uint32_t periods;               /**< Number of periods */

    /* Callbacks */
    voice_device_data_callback_t data_callback;
    voice_device_stop_callback_t stop_callback;
    void *user_data;
} voice_device_desc_t;

/**
 * @brief Initialize default device descriptor
 * @param desc Device descriptor pointer
 * @param mode Device mode
 */
VOICE_API void voice_device_desc_init(voice_device_desc_t *desc, voice_device_mode_t mode);

/**
 * @brief Create audio device
 * @param desc Device descriptor
 * @return Device handle
 */
VOICE_API voice_device_t *voice_device_create(const voice_device_desc_t *desc);

/**
 * @brief Destroy audio device
 * @param device Device handle
 */
VOICE_API void voice_device_destroy(voice_device_t *device);

/**
 * @brief Start device
 * @param device Device handle
 * @return Error code
 */
VOICE_API voice_error_t voice_device_start(voice_device_t *device);

/**
 * @brief Stop device
 * @param device Device handle
 * @return Error code
 */
VOICE_API voice_error_t voice_device_stop(voice_device_t *device);

/**
 * @brief Check if device is running
 * @param device Device handle
 * @return true if running
 */
VOICE_API bool voice_device_is_started(voice_device_t *device);

/**
 * @brief Get device sample rate
 * @param device Device handle
 * @return Sample rate
 */
VOICE_API uint32_t voice_device_get_sample_rate(voice_device_t *device);

/**
 * @brief Get device channel count
 * @param device Device handle
 * @param mode Device mode (capture/playback)
 * @return Channel count
 */
VOICE_API uint8_t voice_device_get_channels(voice_device_t *device, voice_device_mode_t mode);

/**
 * @brief Get device format
 * @param device Device handle
 * @param mode Device mode
 * @return Format
 */
VOICE_API voice_format_t voice_device_get_format(voice_device_t *device, voice_device_mode_t mode);

/* ============================================
 * Device Enumeration
 * ============================================ */

/** Device info */
typedef struct {
    char id[256];
    char name[256];
    bool is_default;
    uint32_t min_channels;
    uint32_t max_channels;
    uint32_t min_sample_rate;
    uint32_t max_sample_rate;
} voice_device_enum_info_t;

/**
 * @brief Get capture device count
 * @return Device count
 */
VOICE_API uint32_t voice_device_get_capture_count(void);

/**
 * @brief Get playback device count
 * @return Device count
 */
VOICE_API uint32_t voice_device_get_playback_count(void);

/**
 * @brief Get capture device info
 * @param index Device index
 * @param info Device info output
 * @return Error code
 */
VOICE_API voice_error_t voice_device_get_capture_info(uint32_t index, voice_device_enum_info_t *info);

/**
 * @brief Get playback device info
 * @param index Device index
 * @param info Device info output
 * @return Error code
 */
VOICE_API voice_error_t voice_device_get_playback_info(uint32_t index, voice_device_enum_info_t *info);

/* ============================================
 * Compatibility API (for example code)
 * ============================================ */

#ifndef VOICE_DEVICE_INFO_T_DEFINED
#define VOICE_DEVICE_INFO_T_DEFINED
/** Device info (simplified, for enumeration) */
typedef struct {
    char id[256];                   /**< Device ID */
    char name[256];                 /**< Device name */
    bool is_default;                /**< Is default device */
} voice_device_info_t;
#endif

/** Device config (extended, with callbacks) */
typedef struct {
    voice_device_mode_t mode;       /**< Device mode */
    uint32_t sample_rate;           /**< Sample rate */
    uint8_t channels;               /**< Channel count */
    uint32_t frame_size;            /**< Frame size (samples) */

    /* Capture callback */
    voice_capture_callback_t capture_callback;
    void *capture_user_data;

    /* Playback callback */
    voice_playback_callback_t playback_callback;
    void *playback_user_data;
} voice_device_ext_config_t;

/**
 * @brief Initialize default device configuration
 * @param config Configuration struct pointer
 */
VOICE_API void voice_device_config_init(voice_device_ext_config_t *config);

/**
 * @brief Create device with simplified configuration
 * @param config Simplified configuration
 * @return Device handle
 */
VOICE_API voice_device_t *voice_device_create_simple(const voice_device_ext_config_t *config);

/**
 * @brief Enumerate devices (compatibility API)
 * @param mode Device mode
 * @param devices Device info array
 * @param count Input: array size, Output: actual device count
 * @return Error code
 */
VOICE_API voice_error_t voice_device_enumerate(
    voice_device_mode_t mode,
    voice_device_info_t *devices,
    size_t *count
);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_DEVICE_H */
