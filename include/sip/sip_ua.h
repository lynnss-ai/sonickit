/**
 * @file sip_ua.h
 * @brief SIP User Agent
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Complete SIP User Agent implementation for making and receiving calls.
 * Handles:
 * - Registration with SIP registrar
 * - Making outgoing calls (INVITE)
 * - Receiving incoming calls
 * - Call management (hold, transfer, etc.)
 * - Dialog state management
 */

#ifndef VOICE_SIP_UA_H
#define VOICE_SIP_UA_H

#include "sip/sip_core.h"
#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * SIP UA Types
 * ============================================ */

/**
 * @brief Registration state
 */
typedef enum {
    SIP_REG_UNREGISTERED,
    SIP_REG_REGISTERING,
    SIP_REG_REGISTERED,
    SIP_REG_UNREGISTERING,
    SIP_REG_FAILED
} sip_registration_state_t;

/**
 * @brief Call state
 */
typedef enum {
    SIP_CALL_IDLE,
    SIP_CALL_CALLING,
    SIP_CALL_INCOMING,
    SIP_CALL_EARLY,
    SIP_CALL_CONNECTING,
    SIP_CALL_CONFIRMED,
    SIP_CALL_DISCONNECTING,
    SIP_CALL_DISCONNECTED,
    SIP_CALL_FAILED
} sip_call_state_t;

/**
 * @brief Call direction
 */
typedef enum {
    SIP_CALL_OUTGOING,
    SIP_CALL_INCOMING_DIR
} sip_call_direction_t;

/**
 * @brief Transport type
 */
typedef enum {
    SIP_TRANSPORT_UDP,
    SIP_TRANSPORT_TCP,
    SIP_TRANSPORT_TLS
} sip_transport_type_t;

/* Forward declarations */
typedef struct sip_ua_s sip_ua_t;
typedef struct sip_call_s sip_call_t;

/* ============================================
 * Call Information
 * ============================================ */

typedef struct {
    char call_id[128];
    sip_call_state_t state;
    sip_call_direction_t direction;

    /* Parties */
    sip_address_t local;
    sip_address_t remote;

    /* Media info */
    char remote_sdp[4096];
    char local_sdp[4096];
    char remote_rtp_host[64];
    uint16_t remote_rtp_port;

    /* Timing */
    uint64_t start_time;
    uint64_t connect_time;
    uint64_t end_time;

    /* Stats */
    uint32_t rtp_packets_sent;
    uint32_t rtp_packets_received;
} sip_call_info_t;

/* ============================================
 * SIP UA Callbacks
 * ============================================ */

/**
 * @brief Registration state callback
 */
typedef void (*sip_on_registration_state_t)(sip_ua_t *ua,
                                             sip_registration_state_t state,
                                             int expires,
                                             void *user_data);

/**
 * @brief Incoming call callback
 */
typedef void (*sip_on_incoming_call_t)(sip_ua_t *ua,
                                        sip_call_t *call,
                                        const sip_call_info_t *info,
                                        void *user_data);

/**
 * @brief Call state callback
 */
typedef void (*sip_on_call_state_t)(sip_ua_t *ua,
                                     sip_call_t *call,
                                     sip_call_state_t state,
                                     void *user_data);

/**
 * @brief Media state callback (for RTP setup)
 */
typedef void (*sip_on_call_media_t)(sip_ua_t *ua,
                                     sip_call_t *call,
                                     const char *remote_host,
                                     uint16_t remote_port,
                                     void *user_data);

/**
 * @brief DTMF callback
 */
typedef void (*sip_on_dtmf_t)(sip_ua_t *ua,
                               sip_call_t *call,
                               char digit,
                               int duration_ms,
                               void *user_data);

/* ============================================
 * SIP UA Configuration
 * ============================================ */

typedef struct {
    /* Account settings */
    char username[128];
    char password[128];
    char domain[256];
    char display_name[128];

    /* Server settings */
    char registrar_host[256];
    uint16_t registrar_port;
    char proxy_host[256];
    uint16_t proxy_port;

    /* Transport */
    sip_transport_type_t transport;
    uint16_t local_port;
    char local_host[64];

    /* Registration */
    bool auto_register;
    int register_expires;
    int register_retry_interval;

    /* RTP settings */
    uint16_t rtp_port_min;
    uint16_t rtp_port_max;

    /* Callbacks */
    sip_on_registration_state_t on_registration;
    sip_on_incoming_call_t on_incoming_call;
    sip_on_call_state_t on_call_state;
    sip_on_call_media_t on_call_media;
    sip_on_dtmf_t on_dtmf;
    void *callback_user_data;

    /* Audio codecs (in preference order) */
    uint8_t audio_codecs[16];
    int audio_codec_count;
} sip_ua_config_t;

/* ============================================
 * SIP UA Functions
 * ============================================ */

/**
 * @brief Initialize UA config with defaults
 */
VOICE_API void sip_ua_config_init(sip_ua_config_t *config);

/**
 * @brief Create SIP User Agent
 */
VOICE_API sip_ua_t *sip_ua_create(const sip_ua_config_t *config);

/**
 * @brief Destroy SIP User Agent
 */
VOICE_API void sip_ua_destroy(sip_ua_t *ua);

/**
 * @brief Start the UA (begin listening and processing)
 */
VOICE_API voice_error_t sip_ua_start(sip_ua_t *ua);

/**
 * @brief Stop the UA
 */
VOICE_API voice_error_t sip_ua_stop(sip_ua_t *ua);

/**
 * @brief Process pending events (call periodically)
 */
VOICE_API voice_error_t sip_ua_process(sip_ua_t *ua, int timeout_ms);

/* ============================================
 * Registration
 * ============================================ */

/**
 * @brief Register with SIP registrar
 */
VOICE_API voice_error_t sip_ua_register(sip_ua_t *ua);

/**
 * @brief Unregister from SIP registrar
 */
VOICE_API voice_error_t sip_ua_unregister(sip_ua_t *ua);

/**
 * @brief Get registration state
 */
VOICE_API sip_registration_state_t sip_ua_get_registration_state(sip_ua_t *ua);

/**
 * @brief Check if registered
 */
VOICE_API bool sip_ua_is_registered(sip_ua_t *ua);

/* ============================================
 * Call Management
 * ============================================ */

/**
 * @brief Make outgoing call
 */
VOICE_API sip_call_t *sip_ua_make_call(sip_ua_t *ua, const char *destination);

/**
 * @brief Answer incoming call
 */
VOICE_API voice_error_t sip_call_answer(sip_call_t *call, int status_code);

/**
 * @brief Reject incoming call
 */
VOICE_API voice_error_t sip_call_reject(sip_call_t *call, int status_code);

/**
 * @brief Hang up call
 */
VOICE_API voice_error_t sip_call_hangup(sip_call_t *call);

/**
 * @brief Put call on hold
 */
VOICE_API voice_error_t sip_call_hold(sip_call_t *call);

/**
 * @brief Resume held call
 */
VOICE_API voice_error_t sip_call_resume(sip_call_t *call);

/**
 * @brief Send DTMF digit
 */
VOICE_API voice_error_t sip_call_send_dtmf(sip_call_t *call, char digit, int duration_ms);

/**
 * @brief Transfer call (REFER)
 */
VOICE_API voice_error_t sip_call_transfer(sip_call_t *call, const char *destination);

/**
 * @brief Get call info
 */
VOICE_API voice_error_t sip_call_get_info(sip_call_t *call, sip_call_info_t *info);

/**
 * @brief Get call state
 */
VOICE_API sip_call_state_t sip_call_get_state(sip_call_t *call);

/**
 * @brief Get call ID
 */
VOICE_API const char *sip_call_get_id(sip_call_t *call);

/**
 * @brief Set local SDP
 */
VOICE_API voice_error_t sip_call_set_local_sdp(sip_call_t *call, const char *sdp);

/**
 * @brief Get remote SDP
 */
VOICE_API const char *sip_call_get_remote_sdp(sip_call_t *call);

/* ============================================
 * SDP Helpers
 * ============================================ */

/**
 * @brief Generate basic audio SDP
 */
VOICE_API voice_error_t sip_generate_sdp(char *buffer, size_t size,
                                const char *session_name,
                                const char *local_ip,
                                uint16_t rtp_port,
                                const uint8_t *codecs,
                                int codec_count);

/**
 * @brief Parse SDP for RTP info
 */
VOICE_API voice_error_t sip_parse_sdp_rtp(const char *sdp,
                                 char *host, size_t host_size,
                                 uint16_t *port,
                                 uint8_t *codecs, int *codec_count);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Get call state as string
 */
VOICE_API const char *sip_call_state_to_string(sip_call_state_t state);

/**
 * @brief Get registration state as string
 */
VOICE_API const char *sip_registration_state_to_string(sip_registration_state_t state);

/**
 * @brief Get UA local URI
 */
VOICE_API voice_error_t sip_ua_get_local_uri(sip_ua_t *ua, char *buffer, size_t size);

/**
 * @brief Get active call count
 */
VOICE_API int sip_ua_get_call_count(sip_ua_t *ua);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_SIP_UA_H */
