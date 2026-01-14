/**
 * @file types.h
 * @brief Voice library common type definitions
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef VOICE_TYPES_H
#define VOICE_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Basic Type Definitions
 * ============================================ */

/** Sample format */
typedef enum {
    VOICE_FORMAT_UNKNOWN = 0,
    VOICE_FORMAT_U8,        /**< Unsigned 8-bit */
    VOICE_FORMAT_S16,       /**< Signed 16-bit (recommended) */
    VOICE_FORMAT_S24,       /**< Signed 24-bit */
    VOICE_FORMAT_S32,       /**< Signed 32-bit */
    VOICE_FORMAT_F32        /**< 32-bit float */
} voice_format_t;

/** Channel layout */
typedef enum {
    VOICE_CHANNEL_MONO = 1,
    VOICE_CHANNEL_STEREO = 2
} voice_channel_t;

/** Sample rate */
typedef enum {
    VOICE_SAMPLE_RATE_8000 = 8000,
    VOICE_SAMPLE_RATE_16000 = 16000,
    VOICE_SAMPLE_RATE_24000 = 24000,
    VOICE_SAMPLE_RATE_32000 = 32000,
    VOICE_SAMPLE_RATE_44100 = 44100,
    VOICE_SAMPLE_RATE_48000 = 48000
} voice_sample_rate_t;

/** Audio frame description */
typedef struct {
    voice_format_t format;          /**< Sample format */
    uint8_t channels;               /**< Number of channels */
    uint32_t sample_rate;           /**< Sample rate */
    uint32_t frame_size_ms;         /**< Frame duration (milliseconds) */
} voice_audio_format_t;

/** Audio frame */
typedef struct {
    void *data;                     /**< Audio data */
    size_t size;                    /**< Data size (bytes) */
    uint32_t samples;               /**< Number of samples (per channel) */
    uint64_t timestamp;             /**< Timestamp (microseconds) */
    voice_audio_format_t format;    /**< Format information */
} voice_frame_t;

/* ============================================
 * Codec Types
 * ============================================ */

typedef enum {
    VOICE_CODEC_NONE = 0,
    VOICE_CODEC_PCM,            /**< Raw PCM */
    VOICE_CODEC_OPUS,           /**< Opus (recommended) */
    VOICE_CODEC_SPEEX,          /**< Speex (deprecated) */
    VOICE_CODEC_G711_ULAW,      /**< G.711 μ-law */
    VOICE_CODEC_G711_ALAW,      /**< G.711 A-law */
    VOICE_CODEC_G722,           /**< G.722 */
    VOICE_CODEC_COUNT
} voice_codec_type_t;

/** RTP Payload Type mapping */
#define VOICE_RTP_PT_PCMU       0   /* G.711 μ-law */
#define VOICE_RTP_PT_PCMA       8   /* G.711 A-law */
#define VOICE_RTP_PT_G722       9   /* G.722 */
#define VOICE_RTP_PT_OPUS       111 /* Opus (dynamic) */
#define VOICE_RTP_PT_SPEEX      97  /* Speex (dynamic) */

/* ============================================
 * Denoising Engine Types
 * ============================================ */

typedef enum {
    VOICE_DENOISE_NONE = 0,
    VOICE_DENOISE_SPEEXDSP,     /**< SpeexDSP traditional DSP */
    VOICE_DENOISE_RNNOISE,      /**< RNNoise deep learning */
    VOICE_DENOISE_AUTO          /**< Auto selection */
} voice_denoise_engine_t;

/* Compatibility alias - example code uses VOICE_DENOISE_SPEEX */
#define VOICE_DENOISE_SPEEX     VOICE_DENOISE_SPEEXDSP

/* ============================================
 * 网络相关类型
 * ============================================ */

/** 网络统计 */
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_lost;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    float loss_rate;            /**< 丢包率 (0-100%) */
    float jitter_ms;            /**< 抖动 (毫秒) */
    float rtt_ms;               /**< 往返时间 (毫秒) */
    uint32_t available_bandwidth; /**< 可用带宽 (bps) */
} voice_network_stats_t;

/** 加密类型 */
typedef enum {
    VOICE_CRYPTO_NONE = 0,
    VOICE_CRYPTO_SRTP_AES128_SHA1_80,   /**< AES-128-CM + HMAC-SHA1-80 */
    VOICE_CRYPTO_SRTP_AES128_SHA1_32,   /**< AES-128-CM + HMAC-SHA1-32 */
    VOICE_CRYPTO_SRTP_AES256_SHA1_80,   /**< AES-256-CM + HMAC-SHA1-80 */
    VOICE_CRYPTO_SRTP_AEAD_AES128_GCM,  /**< AES-128-GCM (推荐) */
    VOICE_CRYPTO_SRTP_AEAD_AES256_GCM   /**< AES-256-GCM */
} voice_crypto_suite_t;

/** 密钥交换方式 */
typedef enum {
    VOICE_KEY_EXCHANGE_PSK = 0,     /**< 预共享密钥 */
    VOICE_KEY_EXCHANGE_DTLS_SRTP    /**< DTLS-SRTP */
} voice_key_exchange_t;

/* ============================================
 * 回调函数类型
 * ============================================ */

/** 音频数据回调 */
typedef void (*voice_audio_callback_t)(
    const voice_frame_t *frame,
    void *user_data
);

/** 事件回调 */
typedef void (*voice_event_callback_t)(
    int event_type,
    void *event_data,
    void *user_data
);

/** 日志回调 */
typedef void (*voice_log_callback_t)(
    int level,
    const char *message,
    void *user_data
);

/* ============================================
 * 工具宏
 * ============================================ */

/** 计算每帧样本数 */
#define VOICE_SAMPLES_PER_FRAME(sample_rate, frame_ms) \
    ((sample_rate) * (frame_ms) / 1000)

/** 计算帧大小(字节) */
#define VOICE_FRAME_SIZE_BYTES(sample_rate, frame_ms, channels, bytes_per_sample) \
    (VOICE_SAMPLES_PER_FRAME(sample_rate, frame_ms) * (channels) * (bytes_per_sample))

/** 获取格式的字节大小 */
static inline size_t voice_format_bytes(voice_format_t format) {
    switch (format) {
        case VOICE_FORMAT_U8:  return 1;
        case VOICE_FORMAT_S16: return 2;
        case VOICE_FORMAT_S24: return 3;
        case VOICE_FORMAT_S32: return 4;
        case VOICE_FORMAT_F32: return 4;
        default: return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* VOICE_TYPES_H */
