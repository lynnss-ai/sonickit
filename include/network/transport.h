/**
 * @file transport.h
 * @brief Network transport layer abstraction
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Provides network transport layer abstraction, encapsulating UDP/TCP socket operations
 */

#ifndef VOICE_TRANSPORT_H
#define VOICE_TRANSPORT_H

#include "voice/types.h"
#include "voice/error.h"
#include "voice/export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ssize_t compatibility definition */
#if defined(_WIN32) || defined(__EMSCRIPTEN__)
#include <sys/types.h>
#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef ptrdiff_t ssize_t;
#endif
#else
#include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * Type Definitions
 * ============================================ */

/**
 * @brief Transport type
 */
typedef enum {
    VOICE_TRANSPORT_UDP,        /**< UDP transport */
    VOICE_TRANSPORT_TCP,        /**< TCP transport */
    VOICE_TRANSPORT_TLS,        /**< TLS/TCP transport */
    VOICE_TRANSPORT_DTLS        /**< DTLS/UDP transport */
} voice_transport_type_t;

/**
 * @brief Address family
 */
typedef enum {
    VOICE_AF_INET,              /**< IPv4 */
    VOICE_AF_INET6              /**< IPv6 */
} voice_address_family_t;

/**
 * @brief Network address
 */
typedef struct {
    voice_address_family_t family;
    char address[64];           /**< Address string */
    uint16_t port;
} voice_net_address_t;

/**
 * @brief Transport configuration
 */
typedef struct {
    voice_transport_type_t type;    /**< Transport type */
    voice_address_family_t family;  /**< Address family */

    /* Local binding */
    char local_address[64];         /**< Local address (empty=any) */
    uint16_t local_port;            /**< Local port (0=auto) */

    /* Socket options */
    uint32_t recv_buffer_size;      /**< Receive buffer size */
    uint32_t send_buffer_size;      /**< Send buffer size */
    int tos;                        /**< ToS/DSCP value */
    bool reuse_addr;                /**< Address reuse */
    bool non_blocking;              /**< Non-blocking mode */

    /* Timeout */
    uint32_t recv_timeout_ms;       /**< Receive timeout (ms) */
    uint32_t send_timeout_ms;       /**< Send timeout (ms) */

    /* Callbacks */
    void (*on_receive)(const uint8_t *data, size_t size,
                       const voice_net_address_t *from, void *user_data);
    void (*on_error)(voice_error_t error, const char *message, void *user_data);
    void *callback_user_data;
} voice_transport_config_t;

/**
 * @brief Transport statistics
 */
typedef struct {
    uint64_t bytes_sent;            /**< Bytes sent */
    uint64_t bytes_received;        /**< Bytes received */
    uint64_t packets_sent;          /**< Packets sent */
    uint64_t packets_received;      /**< Packets received */
    uint64_t send_errors;           /**< Send errors */
    uint64_t recv_errors;           /**< Receive errors */
    uint32_t last_rtt_us;           /**< Last RTT (microseconds) */
} voice_transport_stats_t;

/* ============================================
 * Forward Declarations
 * ============================================ */

typedef struct voice_transport_s voice_transport_t;

/* ============================================
 * Configuration Initialization
 * ============================================ */

/**
 * @brief Initialize transport configuration (UDP)
 */
VOICE_API void voice_transport_config_init(voice_transport_config_t *config);

/* ============================================
 * Lifecycle
 * ============================================ */

/**
 * @brief Create transport instance
 */
VOICE_API voice_transport_t *voice_transport_create(const voice_transport_config_t *config);

/**
 * @brief Destroy transport instance
 */
VOICE_API void voice_transport_destroy(voice_transport_t *transport);

/* ============================================
 * Connection Management
 * ============================================ */

/**
 * @brief Bind to local address
 */
VOICE_API voice_error_t voice_transport_bind(
    voice_transport_t *transport,
    const char *address,
    uint16_t port
);

/**
 * @brief Connect to remote (UDP: set default destination)
 */
VOICE_API voice_error_t voice_transport_connect(
    voice_transport_t *transport,
    const char *address,
    uint16_t port
);

/**
 * @brief Close transport
 */
VOICE_API voice_error_t voice_transport_close(voice_transport_t *transport);

/* ============================================
 * Data Transmission
 * ============================================ */

/**
 * @brief Send data to connected destination
 */
VOICE_API ssize_t voice_transport_send(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size
);

/**
 * @brief Send data to specified address (UDP)
 */
VOICE_API ssize_t voice_transport_sendto(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size,
    const voice_net_address_t *to
);

/**
 * @brief Receive data
 */
VOICE_API ssize_t voice_transport_recv(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size
);

/**
 * @brief Receive data with source address (UDP)
 */
VOICE_API ssize_t voice_transport_recvfrom(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size,
    voice_net_address_t *from
);

/* ============================================
 * Event Handling
 * ============================================ */

/**
 * @brief Process pending IO events
 * @param timeout_ms Timeout (ms), 0=return immediately, -1=wait indefinitely
 * @return Number of events processed, 0=timeout, <0=error
 */
VOICE_API int voice_transport_poll(
    voice_transport_t *transport,
    int timeout_ms
);

/**
 * @brief Check if readable
 */
VOICE_API bool voice_transport_readable(voice_transport_t *transport);

/**
 * @brief Check if writable
 */
VOICE_API bool voice_transport_writable(voice_transport_t *transport);

/* ============================================
 * Status Query
 * ============================================ */

/**
 * @brief Get local address
 */
VOICE_API voice_error_t voice_transport_get_local_address(
    voice_transport_t *transport,
    voice_net_address_t *address
);

/**
 * @brief Get remote address
 */
VOICE_API voice_error_t voice_transport_get_remote_address(
    voice_transport_t *transport,
    voice_net_address_t *address
);

/**
 * @brief Get statistics
 */
VOICE_API voice_error_t voice_transport_get_stats(
    voice_transport_t *transport,
    voice_transport_stats_t *stats
);

/**
 * @brief Reset statistics
 */
VOICE_API void voice_transport_reset_stats(voice_transport_t *transport);

/* ============================================
 * Socket Options
 * ============================================ */

/**
 * @brief Set QoS (ToS/DSCP)
 */
VOICE_API voice_error_t voice_transport_set_qos(
    voice_transport_t *transport,
    int tos
);

/**
 * @brief Get underlying socket descriptor
 */
VOICE_API int voice_transport_get_fd(voice_transport_t *transport);

/* ============================================
 * Address Helper Functions
 * ============================================ */

/**
 * @brief Parse address string
 */
VOICE_API voice_error_t voice_net_address_parse(
    voice_net_address_t *addr,
    const char *address_str
);

/**
 * @brief Format address as string
 */
VOICE_API size_t voice_net_address_format(
    const voice_net_address_t *addr,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Compare addresses
 */
VOICE_API bool voice_net_address_equal(
    const voice_net_address_t *a,
    const voice_net_address_t *b
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_TRANSPORT_H */
