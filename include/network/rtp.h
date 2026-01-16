/**
 * @file rtp.h
 * @brief RTP/RTCP protocol interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * RFC 3550: RTP - Real-time Transport Protocol
 * RFC 3551: RTP Profile for Audio and Video Conferences
 */

#ifndef NETWORK_RTP_H
#define NETWORK_RTP_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * RTP Constants
 * ============================================ */

#define RTP_VERSION             2
#define RTP_HEADER_SIZE         12
#define RTP_MAX_CSRC            15
#define RTP_MAX_PACKET_SIZE     1500
#define RTP_MAX_PAYLOAD_SIZE    (RTP_MAX_PACKET_SIZE - RTP_HEADER_SIZE)

/* Dynamic payload type range */
#define RTP_PT_DYNAMIC_MIN      96
#define RTP_PT_DYNAMIC_MAX      127

/* ============================================
 * RTP Header Structure
 * ============================================ */

/** RTP fixed header (12 bytes) */
typedef struct {
#if defined(__BIG_ENDIAN__)
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t extension:1;
    uint8_t csrc_count:4;
    uint8_t marker:1;
    uint8_t payload_type:7;
#else
    uint8_t csrc_count:4;
    uint8_t extension:1;
    uint8_t padding:1;
    uint8_t version:2;
    uint8_t payload_type:7;
    uint8_t marker:1;
#endif
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

/** RTP extension header */
typedef struct {
    uint16_t profile_specific;
    uint16_t length;  /* Extension data length (32-bit words) */
    /* Extension data follows */
} rtp_extension_t;

/** RTP packet structure */
typedef struct {
    rtp_header_t header;
    uint32_t csrc[RTP_MAX_CSRC];
    const uint8_t *payload;
    size_t payload_size;

    /* 扩展头信息 */
    bool has_extension;
    uint16_t extension_profile;
    const uint8_t *extension_data;
    size_t extension_size;
} rtp_packet_t;

/* ============================================
 * RTCP Type Definitions
 * ============================================ */

/** RTCP packet type */
typedef enum {
    RTCP_TYPE_SR   = 200,   /**< Sender Report */
    RTCP_TYPE_RR   = 201,   /**< Receiver Report */
    RTCP_TYPE_SDES = 202,   /**< Source Description */
    RTCP_TYPE_BYE  = 203,   /**< Goodbye */
    RTCP_TYPE_APP  = 204,   /**< Application-defined */
    RTCP_TYPE_RTPFB = 205,  /**< Transport Layer Feedback */
    RTCP_TYPE_PSFB = 206,   /**< Payload-specific Feedback */
} rtcp_type_t;

/** RTCP common header */
typedef struct {
#if defined(__BIG_ENDIAN__)
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t count:5;
#else
    uint8_t count:5;
    uint8_t padding:1;
    uint8_t version:2;
#endif
    uint8_t packet_type;
    uint16_t length;  /* Length (32-bit words) - 1 */
} rtcp_header_t;

/** RTCP SR (Sender Report) */
typedef struct {
    uint32_t ssrc;
    uint32_t ntp_sec;       /**< NTP timestamp (seconds) */
    uint32_t ntp_frac;      /**< NTP timestamp (fraction) */
    uint32_t rtp_timestamp; /**< RTP timestamp */
    uint32_t packet_count;  /**< Packets sent */
    uint32_t octet_count;   /**< Bytes sent */
} rtcp_sr_t;

/** RTCP RR (Receiver Report Block) */
typedef struct {
    uint32_t ssrc;          /**< Report source SSRC */
    uint32_t fraction_lost:8;
    uint32_t cumulative_lost:24;
    uint32_t highest_seq;   /**< Highest received sequence number */
    uint32_t jitter;        /**< Jitter */
    uint32_t last_sr;       /**< Last SR timestamp */
    uint32_t delay_since_sr;/**< Delay since last SR */
} rtcp_rr_block_t;

/* ============================================
 * RTP Session Handle
 * ============================================ */

typedef struct rtp_session_s rtp_session_t;

/** RTP session configuration */
typedef struct {
    uint32_t ssrc;              /**< SSRC (0=auto-generate) */
    uint8_t payload_type;       /**< Payload type */
    uint32_t clock_rate;        /**< Clock rate */
    uint32_t max_packet_size;   /**< Maximum packet size */
    bool enable_rtcp;           /**< Enable RTCP */
    uint32_t rtcp_bandwidth;    /**< RTCP bandwidth (bps) */
} rtp_session_config_t;

/** RTP statistics */
typedef struct {
    /* Send statistics */
    uint64_t packets_sent;
    uint64_t bytes_sent;
    uint32_t packets_lost;

    /* Receive statistics */
    uint64_t packets_received;
    uint64_t bytes_received;
    uint32_t packets_lost_recv;
    uint32_t packets_reordered;
    uint32_t packets_duplicate;

    /* Quality metrics */
    uint32_t jitter;            /**< Jitter (clock units) */
    float fraction_lost;        /**< Packet loss rate (0-1) */
    uint32_t rtt_ms;            /**< Round-trip time (ms) */
} rtp_statistics_t;

/* ============================================
 * RTP Session API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void rtp_session_config_init(rtp_session_config_t *config);

/**
 * @brief Create RTP session
 */
VOICE_API rtp_session_t *rtp_session_create(const rtp_session_config_t *config);

/**
 * @brief Destroy RTP session
 */
VOICE_API void rtp_session_destroy(rtp_session_t *session);

/**
 * @brief Create RTP packet
 *
 * @param session RTP session
 * @param payload Payload data
 * @param payload_size Payload size
 * @param timestamp RTP timestamp
 * @param marker Marker bit
 * @param output Output buffer
 * @param output_size Buffer size (input) / actual size (output)
 * @return Error code
 */
VOICE_API voice_error_t rtp_session_create_packet(
    rtp_session_t *session,
    const uint8_t *payload,
    size_t payload_size,
    uint32_t timestamp,
    bool marker,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief Parse RTP packet
 *
 * @param session RTP session (can be NULL)
 * @param data Packet data
 * @param size Packet size
 * @param packet Output parsed result
 * @return Error code
 */
VOICE_API voice_error_t rtp_session_parse_packet(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size,
    rtp_packet_t *packet
);

/**
 * @brief Process received RTP packet
 *
 * Updates receive statistics, handles sequence numbers, etc.
 */
VOICE_API voice_error_t rtp_session_process_received(
    rtp_session_t *session,
    const rtp_packet_t *packet
);

/**
 * @brief Get statistics
 */
VOICE_API voice_error_t rtp_session_get_statistics(
    rtp_session_t *session,
    rtp_statistics_t *stats
);

/**
 * @brief Reset statistics
 */
VOICE_API void rtp_session_reset_statistics(rtp_session_t *session);

/**
 * @brief Get SSRC
 */
VOICE_API uint32_t rtp_session_get_ssrc(rtp_session_t *session);

/**
 * @brief Set SSRC
 */
VOICE_API void rtp_session_set_ssrc(rtp_session_t *session, uint32_t ssrc);

/* ============================================
 * RTCP API
 * ============================================ */

/**
 * @brief Create RTCP SR packet
 */
VOICE_API voice_error_t rtcp_create_sr(
    rtp_session_t *session,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief Create RTCP RR packet
 */
VOICE_API voice_error_t rtcp_create_rr(
    rtp_session_t *session,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief Create RTCP BYE packet
 */
VOICE_API voice_error_t rtcp_create_bye(
    rtp_session_t *session,
    const char *reason,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief Parse RTCP packet
 */
VOICE_API voice_error_t rtcp_parse(
    const uint8_t *data,
    size_t size,
    rtcp_header_t *header
);

/**
 * @brief Process received RTCP SR
 */
VOICE_API voice_error_t rtcp_process_sr(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Process received RTCP RR
 */
VOICE_API voice_error_t rtcp_process_rr(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size
);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Generate random SSRC
 */
VOICE_API uint32_t rtp_generate_ssrc(void);

/**
 * @brief Generate random initial sequence number
 */
VOICE_API uint16_t rtp_generate_sequence(void);

/**
 * @brief Get current NTP timestamp
 */
VOICE_API void rtp_get_ntp_timestamp(uint32_t *ntp_sec, uint32_t *ntp_frac);

/**
 * @brief Calculate sequence number difference (handles wrap-around)
 */
VOICE_API int32_t rtp_sequence_diff(uint16_t seq1, uint16_t seq2);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_RTP_H */
