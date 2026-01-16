/**
 * @file error.h
 * @brief Voice library error codes and error handling
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef VOICE_ERROR_H
#define VOICE_ERROR_H

#include "voice/export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Error Code Definitions
 * ============================================ */

typedef enum {
    /* Success */
    VOICE_OK = 0,
    VOICE_SUCCESS = 0,  /* Alias for VOICE_OK */

    /* General errors (1-99) */
    VOICE_ERROR = -1,
    VOICE_ERROR_INVALID_PARAM = -2,
    VOICE_ERROR_NULL_POINTER = -3,
    VOICE_ERROR_OUT_OF_MEMORY = -4,
    VOICE_ERROR_NOT_INITIALIZED = -5,
    VOICE_ERROR_ALREADY_INITIALIZED = -6,
    VOICE_ERROR_NOT_SUPPORTED = -7,
    VOICE_ERROR_TIMEOUT = -8,
    VOICE_ERROR_BUSY = -9,
    VOICE_ERROR_OVERFLOW = -10,
    VOICE_ERROR_UNDERFLOW = -11,
    VOICE_ERROR_ALREADY_RUNNING = -12,
    VOICE_ERROR_NOT_FOUND = -15,
    VOICE_ERROR_BUFFER_FULL = -16,
    VOICE_ERROR_BUFFER_TOO_SMALL = -17,

    /* Aliases for compatibility */
    VOICE_ERROR_NO_MEMORY = VOICE_ERROR_OUT_OF_MEMORY,
    VOICE_ERROR_ENCODE_FAILED = -13,
    VOICE_ERROR_DECODE_FAILED = -14,

    /* Audio device errors (100-199) */
    VOICE_ERROR_DEVICE_NOT_FOUND = -100,
    VOICE_ERROR_DEVICE_OPEN_FAILED = -101,
    VOICE_ERROR_DEVICE_START_FAILED = -102,
    VOICE_ERROR_DEVICE_STOP_FAILED = -103,
    VOICE_ERROR_DEVICE_FORMAT_NOT_SUPPORTED = -104,
    VOICE_ERROR_DEVICE_DISCONNECTED = -105,

    /* Alias for compatibility */
    VOICE_ERROR_DEVICE_OPEN = VOICE_ERROR_DEVICE_OPEN_FAILED,

    /* Codec errors (200-299) */
    VOICE_ERROR_CODEC_NOT_FOUND = -200,
    VOICE_ERROR_CODEC_INIT_FAILED = -201,
    VOICE_ERROR_CODEC_ENCODE_FAILED = -202,
    VOICE_ERROR_CODEC_DECODE_FAILED = -203,
    VOICE_ERROR_CODEC_INVALID_DATA = -204,

    /* Network errors (300-399) */
    VOICE_ERROR_NETWORK = -300,
    VOICE_ERROR_NETWORK_SOCKET = -301,
    VOICE_ERROR_NETWORK_CONNECT = -302,
    VOICE_ERROR_NETWORK_SEND = -303,
    VOICE_ERROR_NETWORK_RECV = -304,
    VOICE_ERROR_NETWORK_TIMEOUT = -305,
    VOICE_ERROR_NETWORK_CLOSED = -306,

    /* RTP/RTCP errors (400-499) */
    VOICE_ERROR_RTP_INVALID_PACKET = -400,
    VOICE_ERROR_RTP_SEQUENCE_GAP = -401,
    VOICE_ERROR_RTCP_INVALID_PACKET = -402,

    /* Alias for compatibility */
    VOICE_ERROR_INVALID_PACKET = VOICE_ERROR_RTP_INVALID_PACKET,

    /* Crypto errors (500-599) */
    VOICE_ERROR_CRYPTO = -500,
    VOICE_ERROR_CRYPTO_INIT_FAILED = -501,
    VOICE_ERROR_CRYPTO_ENCRYPT_FAILED = -502,
    VOICE_ERROR_CRYPTO_DECRYPT_FAILED = -503,
    VOICE_ERROR_CRYPTO_AUTH_FAILED = -504,
    VOICE_ERROR_CRYPTO_REPLAY = -505,
    VOICE_ERROR_CRYPTO_KEY_EXPIRED = -506,
    VOICE_ERROR_DTLS_HANDSHAKE = -507,

    /* DSP errors (600-699) */
    VOICE_ERROR_DSP = -600,
    VOICE_ERROR_DSP_RESAMPLE_FAILED = -601,
    VOICE_ERROR_DSP_DENOISE_FAILED = -602,
    VOICE_ERROR_DSP_AEC_FAILED = -603,

    /* File I/O errors (700-799) */
    VOICE_ERROR_FILE = -700,
    VOICE_ERROR_FILE_OPEN_FAILED = -701,
    VOICE_ERROR_FILE_READ_FAILED = -702,
    VOICE_ERROR_FILE_WRITE_FAILED = -703,
    VOICE_ERROR_FILE_FORMAT_NOT_SUPPORTED = -704,
    VOICE_ERROR_FILE_CORRUPT = -705,
    VOICE_ERROR_FILE_SEEK_FAILED = -706,

    /* System errors (800-899) */
    VOICE_ERROR_SYSTEM = -800,
    VOICE_ERROR_INVALID_STATE = -801,
    VOICE_ERROR_NOT_READY = -802,
    VOICE_ERROR_PROTOCOL = -803,

    /* Aliases for compatibility */
    VOICE_ERROR_FILE_READ = VOICE_ERROR_FILE_READ_FAILED,
    VOICE_ERROR_FILE_WRITE = VOICE_ERROR_FILE_WRITE_FAILED,
    VOICE_ERROR_FILE_SEEK = VOICE_ERROR_FILE_SEEK_FAILED

} voice_error_t;

/* ============================================
 * Log Level
 * ============================================ */

typedef enum {
    VOICE_LOG_TRACE = 0,
    VOICE_LOG_DEBUG = 1,
    VOICE_LOG_INFO = 2,
    VOICE_LOG_WARN = 3,
    VOICE_LOG_ERROR = 4,
    VOICE_LOG_FATAL = 5,
    VOICE_LOG_NONE = 6
} voice_log_level_t;

/* ============================================
 * Error Handling Functions
 * ============================================ */

/**
 * @brief Get error message for error code
 * @param error Error code
 * @return Error message string
 */
VOICE_API const char *voice_error_string(voice_error_t error);

/**
 * @brief Get last error code
 * @return Error code
 */
VOICE_API voice_error_t voice_get_last_error(void);

/**
 * @brief Set last error code
 * @param error Error code
 */
VOICE_API void voice_set_last_error(voice_error_t error);

/**
 * @brief Clear last error
 */
VOICE_API void voice_clear_error(void);

/**
 * @brief Set log level
 * @param level Log level
 */
VOICE_API void voice_set_log_level(voice_log_level_t level);

/**
 * @brief Get current log level
 * @return Log level
 */
VOICE_API voice_log_level_t voice_get_log_level(void);

/**
 * @brief Set log callback
 * @param callback Log callback function
 * @param user_data User data
 */
VOICE_API void voice_set_log_callback(
    void (*callback)(voice_log_level_t level, const char *msg, void *user_data),
    void *user_data
);

/**
 * @brief Output log message
 * @param level Log level
 * @param fmt Format string
 * @param ... Arguments
 */
VOICE_API void voice_log(voice_log_level_t level, const char *fmt, ...);

/* 日志宏 */
#define VOICE_LOG_T(fmt, ...) voice_log(VOICE_LOG_TRACE, fmt, ##__VA_ARGS__)
#define VOICE_LOG_D(fmt, ...) voice_log(VOICE_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define VOICE_LOG_I(fmt, ...) voice_log(VOICE_LOG_INFO, fmt, ##__VA_ARGS__)
#define VOICE_LOG_W(fmt, ...) voice_log(VOICE_LOG_WARN, fmt, ##__VA_ARGS__)
#define VOICE_LOG_E(fmt, ...) voice_log(VOICE_LOG_ERROR, fmt, ##__VA_ARGS__)
#define VOICE_LOG_F(fmt, ...) voice_log(VOICE_LOG_FATAL, fmt, ##__VA_ARGS__)

/* 断言宏 */
#ifdef VOICE_DEBUG
    #define VOICE_ASSERT(cond) \
        do { \
            if (!(cond)) { \
                VOICE_LOG_F("Assertion failed: %s at %s:%d", #cond, __FILE__, __LINE__); \
            } \
        } while(0)
#else
    #define VOICE_ASSERT(cond) ((void)0)
#endif

/* Error check macros */
#define VOICE_CHECK(cond, err) \
    do { \
        if (!(cond)) { \
            voice_set_last_error(err); \
            return err; \
        } \
    } while(0)

#define VOICE_CHECK_NULL(ptr) \
    VOICE_CHECK((ptr) != NULL, VOICE_ERROR_NULL_POINTER)

#define VOICE_CHECK_INIT(obj) \
    VOICE_CHECK((obj) != NULL && (obj)->initialized, VOICE_ERROR_NOT_INITIALIZED)

#ifdef __cplusplus
}
#endif

#endif /* VOICE_ERROR_H */
