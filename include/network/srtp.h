/**
 * @file srtp.h
 * @brief SRTP encryption interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Secure Real-time Transport Protocol (RFC 3711)
 * DTLS-SRTP key exchange (RFC 5764)
 */

#ifndef NETWORK_SRTP_H
#define NETWORK_SRTP_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include "network/rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * SRTP Constants
 * ============================================ */

#define SRTP_MASTER_KEY_LEN     16
#define SRTP_MASTER_SALT_LEN    14
#define SRTP_MAX_AUTH_TAG_LEN   16
#define SRTP_MAX_TRAILER_LEN    (SRTP_MAX_AUTH_TAG_LEN + 4)

/* ============================================
 * Encryption Profiles
 * ============================================ */

typedef enum {
    SRTP_PROFILE_AES128_CM_SHA1_80 = 1,  /**< AES-CM 128-bit key, HMAC-SHA1 80-bit tag */
    SRTP_PROFILE_AES128_CM_SHA1_32 = 2,  /**< AES-CM 128-bit key, HMAC-SHA1 32-bit tag */
    SRTP_PROFILE_AEAD_AES_128_GCM  = 7,  /**< AES-GCM 128-bit */
    SRTP_PROFILE_AEAD_AES_256_GCM  = 8,  /**< AES-GCM 256-bit */
} srtp_profile_t;

/* ============================================
 * SRTP Handle
 * ============================================ */

typedef struct srtp_session_s srtp_session_t;

/** SRTP configuration */
typedef struct {
    srtp_profile_t profile;              /**< Encryption profile */
    uint8_t master_key[32];              /**< Master key */
    size_t master_key_len;               /**< Master key length */
    uint8_t master_salt[14];             /**< Master salt */
    size_t master_salt_len;              /**< Master salt length */
    uint32_t ssrc;                       /**< SSRC */
    bool is_sender;                      /**< Sender/Receiver */
    uint64_t replay_window_size;         /**< Replay protection window size */
} srtp_config_t;

/** SRTP keying material */
typedef struct {
    uint8_t client_write_key[32];
    size_t client_write_key_len;
    uint8_t client_write_salt[14];
    size_t client_write_salt_len;
    uint8_t server_write_key[32];
    size_t server_write_key_len;
    uint8_t server_write_salt[14];
    size_t server_write_salt_len;
    srtp_profile_t profile;
} srtp_keying_material_t;

/* ============================================
 * SRTP API
 * ============================================ */

/**
 * @brief Initialize SRTP library
 * @return Error code
 */
VOICE_API voice_error_t srtp_init(void);

/**
 * @brief Cleanup SRTP library
 */
VOICE_API void srtp_shutdown(void);

/**
 * @brief Initialize default configuration
 */
VOICE_API void srtp_config_init(srtp_config_t *config);

/**
 * @brief Create SRTP session
 * @param config Configuration
 * @return SRTP session handle
 */
VOICE_API srtp_session_t *srtp_session_create(const srtp_config_t *config);

/**
 * @brief Destroy SRTP session
 */
VOICE_API void srtp_session_destroy(srtp_session_t *session);

/**
 * @brief Protect RTP packet (encrypt + authenticate)
 *
 * @param session SRTP session
 * @param rtp_packet RTP packet data (in-place encryption)
 * @param rtp_len RTP packet length (input) / SRTP packet length (output)
 * @param max_len Maximum buffer length
 * @return Error code
 */
VOICE_API voice_error_t srtp_protect(
    srtp_session_t *session,
    uint8_t *rtp_packet,
    size_t *rtp_len,
    size_t max_len
);

/**
 * @brief Unprotect SRTP packet (verify + decrypt)
 *
 * @param session SRTP session
 * @param srtp_packet SRTP packet data (in-place decryption)
 * @param srtp_len SRTP packet length (input) / RTP packet length (output)
 * @return Error code
 */
VOICE_API voice_error_t srtp_unprotect(
    srtp_session_t *session,
    uint8_t *srtp_packet,
    size_t *srtp_len
);

/**
 * @brief Protect RTCP packet
 */
VOICE_API voice_error_t srtcp_protect(
    srtp_session_t *session,
    uint8_t *rtcp_packet,
    size_t *rtcp_len,
    size_t max_len
);

/**
 * @brief Unprotect SRTCP packet
 */
VOICE_API voice_error_t srtcp_unprotect(
    srtp_session_t *session,
    uint8_t *srtcp_packet,
    size_t *srtcp_len
);

/**
 * @brief Update SRTP key
 */
VOICE_API voice_error_t srtp_session_update_key(
    srtp_session_t *session,
    const uint8_t *master_key,
    size_t key_len,
    const uint8_t *master_salt,
    size_t salt_len
);

/**
 * @brief Get authentication tag length
 */
VOICE_API size_t srtp_get_auth_tag_len(srtp_profile_t profile);

/**
 * @brief Get key length
 */
VOICE_API size_t srtp_get_key_len(srtp_profile_t profile);

/**
 * @brief Get salt length
 */
VOICE_API size_t srtp_get_salt_len(srtp_profile_t profile);

/* ============================================
 * DTLS-SRTP API
 * ============================================ */

#ifdef VOICE_HAVE_DTLS

typedef struct dtls_srtp_session_s dtls_srtp_session_t;

/** DTLS role */
typedef enum {
    DTLS_ROLE_CLIENT,
    DTLS_ROLE_SERVER,
    DTLS_ROLE_AUTO,
} dtls_role_t;

/** DTLS-SRTP configuration */
typedef struct {
    dtls_role_t role;                    /**< Role */
    const char *certificate_file;        /**< Certificate file path */
    const char *private_key_file;        /**< Private key file path */
    srtp_profile_t profiles[4];          /**< Supported SRTP profiles */
    size_t profile_count;                /**< Profile count */
    int mtu;                             /**< MTU */
} dtls_srtp_config_t;

/** DTLS event type */
typedef enum {
    DTLS_EVENT_CONNECTED,       /**< Handshake complete */
    DTLS_EVENT_ERROR,           /**< Error occurred */
    DTLS_EVENT_CLOSED,          /**< Connection closed */
    DTLS_EVENT_KEYS_READY,      /**< Key material ready */
} dtls_event_t;

/** DTLS event callback */
typedef void (*dtls_event_callback_t)(
    dtls_srtp_session_t *session,
    dtls_event_t event,
    void *user_data
);

/** DTLS data send callback */
typedef int (*dtls_send_callback_t)(
    const uint8_t *data,
    size_t len,
    void *user_data
);

/**
 * @brief Initialize default configuration
 */
VOICE_API void dtls_srtp_config_init(dtls_srtp_config_t *config);

/**
 * @brief Create DTLS-SRTP session
 */
VOICE_API dtls_srtp_session_t *dtls_srtp_create(const dtls_srtp_config_t *config);

/**
 * @brief Destroy DTLS-SRTP session
 */
VOICE_API void dtls_srtp_destroy(dtls_srtp_session_t *session);

/**
 * @brief Set send callback
 */
VOICE_API void dtls_srtp_set_send_callback(
    dtls_srtp_session_t *session,
    dtls_send_callback_t callback,
    void *user_data
);

/**
 * @brief Set event callback
 */
VOICE_API void dtls_srtp_set_event_callback(
    dtls_srtp_session_t *session,
    dtls_event_callback_t callback,
    void *user_data
);

/**
 * @brief Start handshake
 */
VOICE_API voice_error_t dtls_srtp_start_handshake(dtls_srtp_session_t *session);

/**
 * @brief Handle incoming DTLS data
 */
VOICE_API voice_error_t dtls_srtp_handle_incoming(
    dtls_srtp_session_t *session,
    const uint8_t *data,
    size_t len
);

/**
 * @brief Check if connected
 */
VOICE_API bool dtls_srtp_is_connected(dtls_srtp_session_t *session);

/**
 * @brief Get negotiated SRTP profile
 */
VOICE_API srtp_profile_t dtls_srtp_get_profile(dtls_srtp_session_t *session);

/**
 * @brief Get SRTP key material
 */
VOICE_API voice_error_t dtls_srtp_get_keys(
    dtls_srtp_session_t *session,
    srtp_keying_material_t *keys
);

/**
 * @brief Create SRTP session from DTLS session
 */
VOICE_API srtp_session_t *dtls_srtp_create_srtp_session(
    dtls_srtp_session_t *dtls,
    bool is_sender,
    uint32_t ssrc
);

/**
 * @brief Get local fingerprint
 */
VOICE_API voice_error_t dtls_srtp_get_fingerprint(
    dtls_srtp_session_t *session,
    char *fingerprint,
    size_t max_len
);

/**
 * @brief Set remote fingerprint (for verification)
 */
VOICE_API voice_error_t dtls_srtp_set_remote_fingerprint(
    dtls_srtp_session_t *session,
    const char *fingerprint
);

#endif /* VOICE_HAVE_DTLS */

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_SRTP_H */
