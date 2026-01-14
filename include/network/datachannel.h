/**
 * @file datachannel.h
 * @brief WebRTC DataChannel-style API
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Provides a reliable/unreliable data channel abstraction similar to WebRTC
 * DataChannels, but with a simplified implementation suitable for voice
 * applications. Uses SCTP-style semantics over UDP.
 * 
 * Features:
 * - Reliable ordered delivery (like TCP)
 * - Unreliable unordered delivery (like UDP)
 * - Partial reliability (max retransmits, max lifetime)
 * - Multiple channels on single connection
 * - Binary and text message support
 */

#ifndef VOICE_DATACHANNEL_H
#define VOICE_DATACHANNEL_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Constants
 * ============================================ */

#define DATACHANNEL_MAX_LABEL_SIZE    256
#define DATACHANNEL_MAX_PROTOCOL_SIZE 256
#define DATACHANNEL_MAX_MESSAGE_SIZE  65536
#define DATACHANNEL_MAX_CHANNELS      256

/* ============================================
 * Types
 * ============================================ */

/**
 * @brief DataChannel state
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    DATACHANNEL_STATE_CONNECTING,   /**< Connection in progress */
    DATACHANNEL_STATE_OPEN,         /**< Ready for data transfer */
    DATACHANNEL_STATE_CLOSING,      /**< Closing in progress */
    DATACHANNEL_STATE_CLOSED        /**< Connection closed */
} datachannel_state_t;

/**
 * @brief DataChannel type (reliability mode)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    DATACHANNEL_RELIABLE,           /**< Reliable ordered delivery */
    DATACHANNEL_RELIABLE_UNORDERED, /**< Reliable unordered delivery */
    DATACHANNEL_UNRELIABLE,         /**< Unreliable ordered delivery */
    DATACHANNEL_UNRELIABLE_UNORDERED /**< Unreliable unordered (UDP-like) */
} datachannel_type_t;

/**
 * @brief Message type
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    DATACHANNEL_MSG_BINARY,  /**< Binary data */
    DATACHANNEL_MSG_TEXT     /**< UTF-8 text */
} datachannel_msg_type_t;

/**
 * @brief Channel priority
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef enum {
    DATACHANNEL_PRIORITY_VERY_LOW = 0,
    DATACHANNEL_PRIORITY_LOW = 128,
    DATACHANNEL_PRIORITY_MEDIUM = 256,
    DATACHANNEL_PRIORITY_HIGH = 512
} datachannel_priority_t;

/* Forward declarations */
typedef struct datachannel_s datachannel_t;
typedef struct datachannel_connection_s datachannel_connection_t;

/* ============================================
 * Channel Information
 * ============================================ */

/**
 * @brief DataChannel configuration
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef struct {
    char label[DATACHANNEL_MAX_LABEL_SIZE];       /**< Channel label/name */
    char protocol[DATACHANNEL_MAX_PROTOCOL_SIZE]; /**< Sub-protocol */
    
    datachannel_type_t type;      /**< Reliability mode */
    datachannel_priority_t priority; /**< Channel priority */
    
    /* Partial reliability options */
    bool ordered;                 /**< Ordered delivery */
    int max_retransmits;          /**< Max retransmit attempts (-1 = infinite) */
    int max_lifetime_ms;          /**< Max message lifetime in ms (-1 = infinite) */
    
    /* Negotiated ID (for pre-negotiated channels) */
    bool negotiated;              /**< Pre-negotiated channel */
    uint16_t id;                  /**< Channel ID if pre-negotiated */
} datachannel_config_t;

/**
 * @brief Channel statistics
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef struct {
    uint64_t messages_sent;       /**< Messages sent */
    uint64_t messages_received;   /**< Messages received */
    uint64_t bytes_sent;          /**< Bytes sent */
    uint64_t bytes_received;      /**< Bytes received */
    uint64_t messages_dropped;    /**< Messages dropped (unreliable) */
    uint64_t retransmits;         /**< Retransmission count */
    uint32_t buffered_amount;     /**< Bytes waiting to send */
    float rtt_ms;                 /**< Current RTT estimate */
} datachannel_stats_t;

/* ============================================
 * Callbacks
 * ============================================ */

/**
 * @brief Channel open callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*datachannel_on_open_t)(datachannel_t *channel, void *user_data);

/**
 * @brief Channel close callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*datachannel_on_close_t)(datachannel_t *channel, void *user_data);

/**
 * @brief Channel error callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*datachannel_on_error_t)(datachannel_t *channel, 
                                        const char *error,
                                        void *user_data);

/**
 * @brief Message received callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*datachannel_on_message_t)(datachannel_t *channel,
                                          const void *data,
                                          size_t size,
                                          datachannel_msg_type_t type,
                                          void *user_data);

/**
 * @brief Buffered amount low callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*datachannel_on_buffered_low_t)(datachannel_t *channel,
                                               void *user_data);

/**
 * @brief New channel callback (for connection)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef void (*datachannel_on_channel_t)(datachannel_connection_t *conn,
                                          datachannel_t *channel,
                                          void *user_data);

/* ============================================
 * Connection Configuration
 * ============================================ */

/**
 * @brief Connection configuration
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
typedef struct {
    /* Local binding */
    const char *local_host;       /**< Local bind address (NULL = any) */
    uint16_t local_port;          /**< Local port (0 = auto) */
    
    /* Remote peer */
    const char *remote_host;      /**< Remote host */
    uint16_t remote_port;         /**< Remote port */
    
    /* Connection role */
    bool is_server;               /**< Act as server (wait for connection) */
    
    /* Buffer sizes */
    size_t send_buffer_size;      /**< Send buffer size */
    size_t receive_buffer_size;   /**< Receive buffer size */
    
    /* Timeouts */
    int connect_timeout_ms;       /**< Connection timeout */
    int heartbeat_interval_ms;    /**< Keepalive interval */
    
    /* Callbacks for connection */
    datachannel_on_channel_t on_channel;  /**< New channel created callback */
    void *callback_user_data;
} datachannel_connection_config_t;

/* ============================================
 * Connection Functions
 * ============================================ */

/**
 * @brief Initialize connection config with defaults
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_connection_config_init(datachannel_connection_config_t *config);

/**
 * @brief Create DataChannel connection
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_connection_t *datachannel_connection_create(
    const datachannel_connection_config_t *config);

/**
 * @brief Destroy connection
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_connection_destroy(datachannel_connection_t *conn);

/**
 * @brief Connect to remote peer
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t datachannel_connection_connect(datachannel_connection_t *conn);

/**
 * @brief Close connection
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t datachannel_connection_close(datachannel_connection_t *conn);

/**
 * @brief Process pending events
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * @param conn Connection instance
 * @param timeout_ms Maximum time to wait for events
 * @return VOICE_OK on success
 */
voice_error_t datachannel_connection_process(datachannel_connection_t *conn,
                                              int timeout_ms);

/**
 * @brief Check if connection is established
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
bool datachannel_connection_is_connected(datachannel_connection_t *conn);

/**
 * @brief Get local port (useful if port was auto-assigned)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint16_t datachannel_connection_get_local_port(datachannel_connection_t *conn);

/* ============================================
 * Channel Functions
 * ============================================ */

/**
 * @brief Initialize channel config with defaults
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_config_init(datachannel_config_t *config);

/**
 * @brief Create a new DataChannel
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_t *datachannel_create(datachannel_connection_t *conn,
                                   const datachannel_config_t *config);

/**
 * @brief Close a DataChannel
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t datachannel_close(datachannel_t *channel);

/**
 * @brief Send binary data
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t datachannel_send(datachannel_t *channel,
                                const void *data,
                                size_t size);

/**
 * @brief Send text message
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t datachannel_send_text(datachannel_t *channel,
                                     const char *text);

/**
 * @brief Get channel state
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_state_t datachannel_get_state(datachannel_t *channel);

/**
 * @brief Get channel ID
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint16_t datachannel_get_id(datachannel_t *channel);

/**
 * @brief Get channel label
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
const char *datachannel_get_label(datachannel_t *channel);

/**
 * @brief Get channel protocol
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
const char *datachannel_get_protocol(datachannel_t *channel);

/**
 * @brief Get buffered amount (bytes waiting to send)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint32_t datachannel_get_buffered_amount(datachannel_t *channel);

/**
 * @brief Set buffered amount low threshold
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_set_buffered_amount_low_threshold(datachannel_t *channel,
                                                    uint32_t threshold);

/**
 * @brief Get channel statistics
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t datachannel_get_stats(datachannel_t *channel,
                                     datachannel_stats_t *stats);

/* ============================================
 * Channel Callbacks
 * ============================================ */

/**
 * @brief Set open callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_set_on_open(datachannel_t *channel,
                              datachannel_on_open_t callback,
                              void *user_data);

/**
 * @brief Set close callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_set_on_close(datachannel_t *channel,
                               datachannel_on_close_t callback,
                               void *user_data);

/**
 * @brief Set error callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_set_on_error(datachannel_t *channel,
                               datachannel_on_error_t callback,
                               void *user_data);

/**
 * @brief Set message callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_set_on_message(datachannel_t *channel,
                                 datachannel_on_message_t callback,
                                 void *user_data);

/**
 * @brief Set buffered amount low callback
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void datachannel_set_on_buffered_low(datachannel_t *channel,
                                      datachannel_on_buffered_low_t callback,
                                      void *user_data);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * @brief Get state as string
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
const char *datachannel_state_to_string(datachannel_state_t state);

/**
 * @brief Get type as string
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
const char *datachannel_type_to_string(datachannel_type_t type);

/* ============================================
 * Quick API for simple use cases
 * ============================================ */

/**
 * @brief Create simple reliable channel
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_t *datachannel_create_reliable(datachannel_connection_t *conn,
                                            const char *label);

/**
 * @brief Create simple unreliable channel
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_t *datachannel_create_unreliable(datachannel_connection_t *conn,
                                              const char *label);

/**
 * @brief Create channel with max retransmits
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_t *datachannel_create_with_retransmits(datachannel_connection_t *conn,
                                                    const char *label,
                                                    int max_retransmits);

/**
 * @brief Create channel with max lifetime
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
datachannel_t *datachannel_create_with_lifetime(datachannel_connection_t *conn,
                                                 const char *label,
                                                 int max_lifetime_ms);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_DATACHANNEL_H */
