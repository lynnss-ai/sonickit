/**
 * @file ice.h
 * @brief ICE (Interactive Connectivity Establishment) support
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * ICE/STUN/TURN support for NAT traversal
 * Based on RFC 5245 (ICE), RFC 5389 (STUN), RFC 5766 (TURN)
 */

#ifndef VOICE_ICE_H
#define VOICE_ICE_H

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
 * Type Definitions
 * ============================================ */

typedef struct voice_ice_agent_s voice_ice_agent_t;
typedef struct voice_stun_client_s voice_stun_client_t;

/** ICE candidate type */
typedef enum {
    VOICE_ICE_CANDIDATE_HOST,       /**< Host address */
    VOICE_ICE_CANDIDATE_SRFLX,      /**< Server reflexive address (STUN) */
    VOICE_ICE_CANDIDATE_PRFLX,      /**< Peer reflexive address */
    VOICE_ICE_CANDIDATE_RELAY,      /**< Relay address (TURN) */
} voice_ice_candidate_type_t;

/** ICE connection state */
typedef enum {
    VOICE_ICE_STATE_NEW,
    VOICE_ICE_STATE_CHECKING,
    VOICE_ICE_STATE_CONNECTED,
    VOICE_ICE_STATE_COMPLETED,
    VOICE_ICE_STATE_FAILED,
    VOICE_ICE_STATE_DISCONNECTED,
    VOICE_ICE_STATE_CLOSED,
} voice_ice_state_t;

/** ICE role */
typedef enum {
    VOICE_ICE_ROLE_CONTROLLING,
    VOICE_ICE_ROLE_CONTROLLED,
} voice_ice_role_t;

/** ICE mode */
typedef enum {
    VOICE_ICE_FULL,                 /**< Full ICE */
    VOICE_ICE_LITE,                 /**< ICE Lite */
} voice_ice_mode_t;

/* ============================================
 * Address Structure
 * ============================================ */

typedef struct {
    uint8_t family;                 /**< AF_INET or AF_INET6 */
    uint16_t port;
    union {
        uint8_t ipv4[4];
        uint8_t ipv6[16];
    } addr;
} voice_network_addr_t;

/* ============================================
 * ICE Candidate
 * ============================================ */

typedef struct {
    char foundation[33];            /**< Foundation ID */
    uint32_t component_id;          /**< Component ID (1=RTP, 2=RTCP) */
    char transport[8];              /**< "udp" or "tcp" */
    uint32_t priority;              /**< Priority */
    voice_network_addr_t address;   /**< Address */
    voice_ice_candidate_type_t type;/**< Candidate type */
    voice_network_addr_t related;   /**< Related address (SRFLX/RELAY) */
    char ufrag[256];                /**< User fragment */
    char pwd[256];                  /**< Password */
} voice_ice_candidate_t;

/* ============================================
 * STUN Configuration
 * ============================================ */

typedef struct {
    char server[256];               /**< STUN server address */
    uint16_t port;                  /**< STUN server port (default 3478) */
    uint32_t timeout_ms;            /**< Timeout */
    uint32_t retries;               /**< Retry count */
} voice_stun_config_t;

/* ============================================
 * TURN Configuration
 * ============================================ */

typedef struct {
    char server[256];               /**< TURN server address */
    uint16_t port;                  /**< TURN server port (default 3478) */
    char username[256];             /**< Username */
    char password[256];             /**< Password */
    char realm[256];                /**< Realm */
    uint32_t lifetime;              /**< Allocation lifetime (seconds) */
    bool use_tls;                   /**< Use TLS (TURNS) */
} voice_turn_config_t;

/* ============================================
 * ICE Agent 配置
 * ============================================ */

typedef struct {
    voice_ice_mode_t mode;
    voice_ice_role_t initial_role;

    /* STUN/TURN 服务器 */
    voice_stun_config_t stun_servers[4];
    size_t stun_server_count;
    voice_turn_config_t turn_servers[2];
    size_t turn_server_count;

    /* 超时 */
    uint32_t connectivity_check_timeout_ms;
    uint32_t nomination_timeout_ms;

    /* 候选收集 */
    bool gather_host_candidates;
    bool gather_srflx_candidates;
    bool gather_relay_candidates;

    /* 回调 */
    void (*on_candidate)(const voice_ice_candidate_t *candidate, void *user_data);
    void (*on_state_change)(voice_ice_state_t state, void *user_data);
    void (*on_selected_pair)(const voice_ice_candidate_t *local,
                            const voice_ice_candidate_t *remote, void *user_data);
    void *callback_user_data;
} voice_ice_config_t;

/* ============================================
 * ICE Agent API
 * ============================================ */

/**
 * @brief Initialize default configuration
 */
VOICE_API void voice_ice_config_init(voice_ice_config_t *config);

/**
 * @brief Create ICE Agent
 */
VOICE_API voice_ice_agent_t *voice_ice_agent_create(const voice_ice_config_t *config);

/**
 * @brief Destroy ICE Agent
 */
VOICE_API void voice_ice_agent_destroy(voice_ice_agent_t *agent);

/**
 * @brief Start gathering candidates
 */
VOICE_API voice_error_t voice_ice_gather_candidates(voice_ice_agent_t *agent);

/**
 * @brief Get local candidate list
 */
VOICE_API voice_error_t voice_ice_get_local_candidates(
    voice_ice_agent_t *agent,
    voice_ice_candidate_t *candidates,
    size_t *count
);

/**
 * @brief Add remote candidate
 */
VOICE_API voice_error_t voice_ice_add_remote_candidate(
    voice_ice_agent_t *agent,
    const voice_ice_candidate_t *candidate
);

/**
 * @brief Set remote credentials
 */
VOICE_API voice_error_t voice_ice_set_remote_credentials(
    voice_ice_agent_t *agent,
    const char *ufrag,
    const char *pwd
);

/**
 * @brief Get local credentials
 */
VOICE_API voice_error_t voice_ice_get_local_credentials(
    voice_ice_agent_t *agent,
    char *ufrag,
    size_t ufrag_size,
    char *pwd,
    size_t pwd_size
);

/**
 * @brief Start connectivity checks
 */
VOICE_API voice_error_t voice_ice_start_checks(voice_ice_agent_t *agent);

/**
 * @brief Get current state
 */
VOICE_API voice_ice_state_t voice_ice_get_state(voice_ice_agent_t *agent);

/**
 * @brief Send data (through selected candidate pair)
 */
VOICE_API voice_error_t voice_ice_send(
    voice_ice_agent_t *agent,
    uint32_t component_id,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Process received data
 */
VOICE_API voice_error_t voice_ice_process_incoming(
    voice_ice_agent_t *agent,
    const uint8_t *data,
    size_t size,
    const voice_network_addr_t *from
);

/**
 * @brief Close ICE Agent
 */
VOICE_API void voice_ice_close(voice_ice_agent_t *agent);

/* ============================================
 * STUN Client API
 * ============================================ */

/**
 * @brief Create STUN client
 */
VOICE_API voice_stun_client_t *voice_stun_client_create(const voice_stun_config_t *config);

/**
 * @brief Destroy STUN client
 */
VOICE_API void voice_stun_client_destroy(voice_stun_client_t *client);

/**
 * @brief Binding request (get public address)
 */
VOICE_API voice_error_t voice_stun_binding_request(
    voice_stun_client_t *client,
    voice_network_addr_t *mapped_addr
);

/* ============================================
 * SDP Utility Functions
 * ============================================ */

/**
 * @brief Serialize candidate to SDP attribute
 */
VOICE_API size_t voice_ice_candidate_to_sdp(
    const voice_ice_candidate_t *candidate,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Parse candidate from SDP attribute
 */
VOICE_API voice_error_t voice_ice_candidate_from_sdp(
    const char *sdp_line,
    voice_ice_candidate_t *candidate
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_ICE_H */
