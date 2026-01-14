/**
 * @file datachannel.c
 * @brief WebRTC DataChannel-style implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * Simplified SCTP-like protocol over UDP for reliable/unreliable messaging.
 */

#include "network/datachannel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define SOCKET_INVALID INVALID_SOCKET
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
typedef int socket_t;
#define SOCKET_INVALID (-1)
#define CLOSE_SOCKET close
#endif

/* ============================================
 * Constants
 * ============================================ */

#define MAX_RETRANSMIT_QUEUE 1024
#define INITIAL_RTT_MS 100
#define MAX_RTT_MS 10000
#define HEARTBEAT_TIMEOUT_MS 30000
#define ALPHA 0.125f  /* RTT smoothing factor */
#define BETA 0.25f    /* RTT variance factor */

/* ============================================
 * Protocol Messages
 * ============================================ */

/* Message types */
typedef enum {
    MSG_TYPE_DATA = 0x00,         /* Data message */
    MSG_TYPE_ACK = 0x01,          /* Acknowledgment */
    MSG_TYPE_OPEN = 0x02,         /* Channel open request */
    MSG_TYPE_OPEN_ACK = 0x03,     /* Channel open acknowledgment */
    MSG_TYPE_CLOSE = 0x04,        /* Channel close */
    MSG_TYPE_HEARTBEAT = 0x05,    /* Keepalive */
    MSG_TYPE_HEARTBEAT_ACK = 0x06 /* Keepalive response */
} msg_type_t;

/* Message flags */
#define MSG_FLAG_TEXT     0x01    /* Text message (vs binary) */
#define MSG_FLAG_BEGIN    0x02    /* First fragment */
#define MSG_FLAG_END      0x04    /* Last fragment */
#define MSG_FLAG_ORDERED  0x08    /* Ordered delivery */
#define MSG_FLAG_RELIABLE 0x10    /* Reliable delivery */

/* Protocol header (12 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t type;                 /* Message type */
    uint8_t flags;                /* Message flags */
    uint16_t channel_id;          /* Channel identifier */
    uint32_t sequence;            /* Sequence number */
    uint32_t timestamp;           /* Timestamp for RTT calculation */
} protocol_header_t;

/* ============================================
 * Retransmit Queue Entry
 * ============================================ */

typedef struct retransmit_entry_s {
    uint8_t *data;
    size_t size;
    uint32_t sequence;
    uint16_t channel_id;
    uint64_t send_time_ms;
    uint64_t expire_time_ms;
    int retransmit_count;
    int max_retransmits;
    struct retransmit_entry_s *next;
} retransmit_entry_t;

/* ============================================
 * DataChannel Structure
 * ============================================ */

struct datachannel_s {
    datachannel_connection_t *connection;
    
    /* Channel identification */
    uint16_t id;
    char label[DATACHANNEL_MAX_LABEL_SIZE];
    char protocol[DATACHANNEL_MAX_PROTOCOL_SIZE];
    
    /* Configuration */
    datachannel_type_t type;
    datachannel_priority_t priority;
    bool ordered;
    int max_retransmits;
    int max_lifetime_ms;
    
    /* State */
    datachannel_state_t state;
    uint32_t send_sequence;
    uint32_t recv_sequence;
    
    /* Buffering */
    uint32_t buffered_amount;
    uint32_t buffered_amount_low_threshold;
    
    /* Statistics */
    datachannel_stats_t stats;
    
    /* Callbacks */
    datachannel_on_open_t on_open;
    void *on_open_user_data;
    
    datachannel_on_close_t on_close;
    void *on_close_user_data;
    
    datachannel_on_error_t on_error;
    void *on_error_user_data;
    
    datachannel_on_message_t on_message;
    void *on_message_user_data;
    
    datachannel_on_buffered_low_t on_buffered_low;
    void *on_buffered_low_user_data;
    
    /* Linked list */
    struct datachannel_s *next;
};

/* ============================================
 * Connection Structure
 * ============================================ */

struct datachannel_connection_s {
    /* Socket */
    socket_t socket;
    bool is_server;
    bool connected;
    
    /* Addresses */
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    
    /* Channels */
    datachannel_t *channels;
    uint16_t next_channel_id;
    
    /* Retransmit queue */
    retransmit_entry_t *retransmit_head;
    retransmit_entry_t *retransmit_tail;
    int retransmit_count;
    
    /* RTT estimation */
    float srtt_ms;          /* Smoothed RTT */
    float rttvar_ms;        /* RTT variance */
    float rto_ms;           /* Retransmit timeout */
    
    /* Timing */
    uint64_t last_send_time_ms;
    uint64_t last_recv_time_ms;
    int heartbeat_interval_ms;
    
    /* Callbacks */
    datachannel_on_channel_t on_channel;
    void *callback_user_data;
    
    /* Buffer sizes */
    size_t send_buffer_size;
    size_t receive_buffer_size;
};

/* ============================================
 * Utility Functions
 * ============================================ */

static uint64_t get_time_ms(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static bool init_socket_system(void) {
#ifdef _WIN32
    static bool initialized = false;
    if (!initialized) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            return false;
        }
        initialized = true;
    }
#endif
    return true;
}

static bool set_socket_nonblocking(socket_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) return false;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK) >= 0;
#endif
}

static uint32_t get_timestamp(void) {
    return (uint32_t)(get_time_ms() & 0xFFFFFFFF);
}

/* ============================================
 * Protocol Functions
 * ============================================ */

static voice_error_t send_message(datachannel_connection_t *conn,
                                   uint16_t channel_id,
                                   msg_type_t type,
                                   uint8_t flags,
                                   uint32_t sequence,
                                   const void *payload,
                                   size_t payload_size) {
    if (!conn || conn->socket == SOCKET_INVALID) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    size_t total_size = sizeof(protocol_header_t) + payload_size;
    uint8_t *buffer = (uint8_t *)malloc(total_size);
    if (!buffer) {
        return VOICE_ERROR_OUT_OF_MEMORY;
    }
    
    /* Build header */
    protocol_header_t *header = (protocol_header_t *)buffer;
    header->type = (uint8_t)type;
    header->flags = flags;
    header->channel_id = htons(channel_id);
    header->sequence = htonl(sequence);
    header->timestamp = htonl(get_timestamp());
    
    /* Copy payload */
    if (payload && payload_size > 0) {
        memcpy(buffer + sizeof(protocol_header_t), payload, payload_size);
    }
    
    /* Send */
    ssize_t sent = sendto(conn->socket, (char *)buffer, (int)total_size, 0,
                          (struct sockaddr *)&conn->remote_addr,
                          sizeof(conn->remote_addr));
    
    free(buffer);
    
    if (sent < 0) {
        return VOICE_ERROR_NETWORK;
    }
    
    conn->last_send_time_ms = get_time_ms();
    return VOICE_OK;
}

static void add_to_retransmit_queue(datachannel_connection_t *conn,
                                     uint16_t channel_id,
                                     uint32_t sequence,
                                     const uint8_t *data,
                                     size_t size,
                                     int max_retransmits,
                                     int max_lifetime_ms) {
    if (conn->retransmit_count >= MAX_RETRANSMIT_QUEUE) {
        return; /* Queue full */
    }
    
    retransmit_entry_t *entry = (retransmit_entry_t *)calloc(1, sizeof(retransmit_entry_t));
    if (!entry) return;
    
    entry->data = (uint8_t *)malloc(size);
    if (!entry->data) {
        free(entry);
        return;
    }
    
    memcpy(entry->data, data, size);
    entry->size = size;
    entry->sequence = sequence;
    entry->channel_id = channel_id;
    entry->send_time_ms = get_time_ms();
    entry->retransmit_count = 0;
    entry->max_retransmits = max_retransmits;
    
    if (max_lifetime_ms > 0) {
        entry->expire_time_ms = entry->send_time_ms + max_lifetime_ms;
    } else {
        entry->expire_time_ms = 0; /* No expiration */
    }
    
    /* Add to tail */
    entry->next = NULL;
    if (conn->retransmit_tail) {
        conn->retransmit_tail->next = entry;
    } else {
        conn->retransmit_head = entry;
    }
    conn->retransmit_tail = entry;
    conn->retransmit_count++;
}

static void remove_from_retransmit_queue(datachannel_connection_t *conn,
                                          uint16_t channel_id,
                                          uint32_t sequence) {
    retransmit_entry_t **pp = &conn->retransmit_head;
    
    while (*pp) {
        retransmit_entry_t *entry = *pp;
        if (entry->channel_id == channel_id && entry->sequence == sequence) {
            *pp = entry->next;
            if (entry == conn->retransmit_tail) {
                /* Find new tail */
                conn->retransmit_tail = NULL;
                for (retransmit_entry_t *e = conn->retransmit_head; e; e = e->next) {
                    conn->retransmit_tail = e;
                }
            }
            free(entry->data);
            free(entry);
            conn->retransmit_count--;
            return;
        }
        pp = &entry->next;
    }
}

static void update_rtt(datachannel_connection_t *conn, uint32_t send_timestamp) {
    uint32_t now = get_timestamp();
    float rtt = (float)(now - send_timestamp);
    
    if (rtt < 0 || rtt > MAX_RTT_MS) {
        return; /* Invalid measurement */
    }
    
    if (conn->srtt_ms == 0) {
        /* First measurement */
        conn->srtt_ms = rtt;
        conn->rttvar_ms = rtt / 2.0f;
    } else {
        /* Smooth update */
        conn->rttvar_ms = (1 - BETA) * conn->rttvar_ms + 
                          BETA * fabsf(conn->srtt_ms - rtt);
        conn->srtt_ms = (1 - ALPHA) * conn->srtt_ms + ALPHA * rtt;
    }
    
    /* Calculate RTO */
    conn->rto_ms = conn->srtt_ms + 4.0f * conn->rttvar_ms;
    if (conn->rto_ms < 100) conn->rto_ms = 100;
    if (conn->rto_ms > 60000) conn->rto_ms = 60000;
}

static void process_retransmits(datachannel_connection_t *conn) {
    uint64_t now = get_time_ms();
    retransmit_entry_t **pp = &conn->retransmit_head;
    
    while (*pp) {
        retransmit_entry_t *entry = *pp;
        
        /* Check expiration */
        if (entry->expire_time_ms > 0 && now >= entry->expire_time_ms) {
            /* Expired - remove without retransmit */
            *pp = entry->next;
            if (entry == conn->retransmit_tail) {
                conn->retransmit_tail = NULL;
                for (retransmit_entry_t *e = conn->retransmit_head; e; e = e->next) {
                    conn->retransmit_tail = e;
                }
            }
            
            /* Update channel stats */
            for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
                if (ch->id == entry->channel_id) {
                    ch->stats.messages_dropped++;
                    break;
                }
            }
            
            free(entry->data);
            free(entry);
            conn->retransmit_count--;
            continue;
        }
        
        /* Check for retransmit */
        if (now - entry->send_time_ms >= (uint64_t)conn->rto_ms) {
            if (entry->max_retransmits >= 0 && 
                entry->retransmit_count >= entry->max_retransmits) {
                /* Max retransmits reached - remove */
                *pp = entry->next;
                if (entry == conn->retransmit_tail) {
                    conn->retransmit_tail = NULL;
                    for (retransmit_entry_t *e = conn->retransmit_head; e; e = e->next) {
                        conn->retransmit_tail = e;
                    }
                }
                
                for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
                    if (ch->id == entry->channel_id) {
                        ch->stats.messages_dropped++;
                        break;
                    }
                }
                
                free(entry->data);
                free(entry);
                conn->retransmit_count--;
                continue;
            }
            
            /* Retransmit */
            sendto(conn->socket, (char *)entry->data, (int)entry->size, 0,
                   (struct sockaddr *)&conn->remote_addr,
                   sizeof(conn->remote_addr));
            
            entry->retransmit_count++;
            entry->send_time_ms = now;
            
            /* Update stats */
            for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
                if (ch->id == entry->channel_id) {
                    ch->stats.retransmits++;
                    break;
                }
            }
            
            /* Exponential backoff */
            conn->rto_ms *= 2;
            if (conn->rto_ms > 60000) conn->rto_ms = 60000;
        }
        
        pp = &entry->next;
    }
}

/* ============================================
 * Connection Implementation
 * ============================================ */

void datachannel_connection_config_init(datachannel_connection_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(datachannel_connection_config_t));
    config->local_host = NULL;
    config->local_port = 0;
    config->remote_host = "127.0.0.1";
    config->remote_port = 5000;
    config->is_server = false;
    config->send_buffer_size = 1024 * 1024;
    config->receive_buffer_size = 1024 * 1024;
    config->connect_timeout_ms = 5000;
    config->heartbeat_interval_ms = 5000;
}

datachannel_connection_t *datachannel_connection_create(
    const datachannel_connection_config_t *config) {
    
    if (!config) return NULL;
    
    if (!init_socket_system()) {
        return NULL;
    }
    
    datachannel_connection_t *conn = (datachannel_connection_t *)calloc(
        1, sizeof(datachannel_connection_t));
    if (!conn) return NULL;
    
    /* Create UDP socket */
    conn->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (conn->socket == SOCKET_INVALID) {
        free(conn);
        return NULL;
    }
    
    /* Set non-blocking */
    if (!set_socket_nonblocking(conn->socket)) {
        CLOSE_SOCKET(conn->socket);
        free(conn);
        return NULL;
    }
    
    /* Configure local address */
    memset(&conn->local_addr, 0, sizeof(conn->local_addr));
    conn->local_addr.sin_family = AF_INET;
    conn->local_addr.sin_port = htons(config->local_port);
    
    if (config->local_host) {
        inet_pton(AF_INET, config->local_host, &conn->local_addr.sin_addr);
    } else {
        conn->local_addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    /* Bind socket */
    if (bind(conn->socket, (struct sockaddr *)&conn->local_addr,
             sizeof(conn->local_addr)) < 0) {
        CLOSE_SOCKET(conn->socket);
        free(conn);
        return NULL;
    }
    
    /* Get actual bound port */
    socklen_t addr_len = sizeof(conn->local_addr);
    getsockname(conn->socket, (struct sockaddr *)&conn->local_addr, &addr_len);
    
    /* Configure remote address */
    memset(&conn->remote_addr, 0, sizeof(conn->remote_addr));
    conn->remote_addr.sin_family = AF_INET;
    conn->remote_addr.sin_port = htons(config->remote_port);
    
    if (config->remote_host) {
        struct hostent *host = gethostbyname(config->remote_host);
        if (host) {
            memcpy(&conn->remote_addr.sin_addr, host->h_addr_list[0], host->h_length);
        } else {
            inet_pton(AF_INET, config->remote_host, &conn->remote_addr.sin_addr);
        }
    }
    
    /* Set buffer sizes */
    int send_size = (int)config->send_buffer_size;
    int recv_size = (int)config->receive_buffer_size;
    setsockopt(conn->socket, SOL_SOCKET, SO_SNDBUF, (char *)&send_size, sizeof(send_size));
    setsockopt(conn->socket, SOL_SOCKET, SO_RCVBUF, (char *)&recv_size, sizeof(recv_size));
    
    /* Initialize state */
    conn->is_server = config->is_server;
    conn->connected = false;
    conn->channels = NULL;
    conn->next_channel_id = config->is_server ? 1 : 0; /* Odd/even for server/client */
    conn->retransmit_head = NULL;
    conn->retransmit_tail = NULL;
    conn->retransmit_count = 0;
    conn->srtt_ms = INITIAL_RTT_MS;
    conn->rttvar_ms = INITIAL_RTT_MS / 2.0f;
    conn->rto_ms = INITIAL_RTT_MS * 3;
    conn->last_send_time_ms = 0;
    conn->last_recv_time_ms = 0;
    conn->heartbeat_interval_ms = config->heartbeat_interval_ms;
    conn->on_channel = config->on_channel;
    conn->callback_user_data = config->callback_user_data;
    conn->send_buffer_size = config->send_buffer_size;
    conn->receive_buffer_size = config->receive_buffer_size;
    
    return conn;
}

void datachannel_connection_destroy(datachannel_connection_t *conn) {
    if (!conn) return;
    
    /* Close all channels */
    while (conn->channels) {
        datachannel_t *ch = conn->channels;
        conn->channels = ch->next;
        free(ch);
    }
    
    /* Clear retransmit queue */
    while (conn->retransmit_head) {
        retransmit_entry_t *entry = conn->retransmit_head;
        conn->retransmit_head = entry->next;
        free(entry->data);
        free(entry);
    }
    
    /* Close socket */
    if (conn->socket != SOCKET_INVALID) {
        CLOSE_SOCKET(conn->socket);
    }
    
    free(conn);
}

voice_error_t datachannel_connection_connect(datachannel_connection_t *conn) {
    if (!conn) return VOICE_ERROR_INVALID_PARAM;
    
    /* For UDP, just mark as connected */
    conn->connected = true;
    conn->last_recv_time_ms = get_time_ms();
    
    /* Send initial heartbeat */
    send_message(conn, 0, MSG_TYPE_HEARTBEAT, 0, 0, NULL, 0);
    
    return VOICE_OK;
}

voice_error_t datachannel_connection_close(datachannel_connection_t *conn) {
    if (!conn) return VOICE_ERROR_INVALID_PARAM;
    
    /* Close all channels */
    for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
        if (ch->state != DATACHANNEL_STATE_CLOSED) {
            ch->state = DATACHANNEL_STATE_CLOSING;
            send_message(conn, ch->id, MSG_TYPE_CLOSE, 0, 0, NULL, 0);
            ch->state = DATACHANNEL_STATE_CLOSED;
            
            if (ch->on_close) {
                ch->on_close(ch, ch->on_close_user_data);
            }
        }
    }
    
    conn->connected = false;
    return VOICE_OK;
}

voice_error_t datachannel_connection_process(datachannel_connection_t *conn,
                                              int timeout_ms) {
    if (!conn) return VOICE_ERROR_INVALID_PARAM;
    
    uint64_t start_time = get_time_ms();
    uint8_t buffer[DATACHANNEL_MAX_MESSAGE_SIZE + sizeof(protocol_header_t)];
    
    while (1) {
        /* Check timeout */
        uint64_t elapsed = get_time_ms() - start_time;
        if (timeout_ms >= 0 && elapsed >= (uint64_t)timeout_ms) {
            break;
        }
        
        /* Use select for waiting */
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(conn->socket, &read_fds);
        
        struct timeval tv;
        int remaining = timeout_ms >= 0 ? (int)(timeout_ms - elapsed) : 100;
        tv.tv_sec = remaining / 1000;
        tv.tv_usec = (remaining % 1000) * 1000;
        
        int ready = select((int)conn->socket + 1, &read_fds, NULL, NULL, &tv);
        
        if (ready <= 0) {
            break;
        }
        
        /* Receive message */
        struct sockaddr_in from_addr;
        socklen_t from_len = sizeof(from_addr);
        
        ssize_t received = recvfrom(conn->socket, (char *)buffer, sizeof(buffer), 0,
                                     (struct sockaddr *)&from_addr, &from_len);
        
        if (received < (ssize_t)sizeof(protocol_header_t)) {
            continue;
        }
        
        conn->last_recv_time_ms = get_time_ms();
        
        /* Parse header */
        protocol_header_t *header = (protocol_header_t *)buffer;
        uint16_t channel_id = ntohs(header->channel_id);
        uint32_t sequence = ntohl(header->sequence);
        uint32_t timestamp = ntohl(header->timestamp);
        
        /* Handle message type */
        switch (header->type) {
            case MSG_TYPE_HEARTBEAT:
                send_message(conn, 0, MSG_TYPE_HEARTBEAT_ACK, 0, 0, 
                            &timestamp, sizeof(timestamp));
                break;
                
            case MSG_TYPE_HEARTBEAT_ACK:
                update_rtt(conn, timestamp);
                conn->connected = true;
                break;
                
            case MSG_TYPE_ACK:
                /* Remove from retransmit queue */
                remove_from_retransmit_queue(conn, channel_id, sequence);
                update_rtt(conn, timestamp);
                break;
                
            case MSG_TYPE_OPEN: {
                /* Find or create channel */
                datachannel_t *ch = NULL;
                for (datachannel_t *c = conn->channels; c; c = c->next) {
                    if (c->id == channel_id) {
                        ch = c;
                        break;
                    }
                }
                
                if (!ch) {
                    /* Create new channel for incoming request */
                    datachannel_config_t config;
                    datachannel_config_init(&config);
                    config.id = channel_id;
                    config.negotiated = true;
                    
                    /* Parse label/protocol from payload */
                    size_t payload_size = received - sizeof(protocol_header_t);
                    const char *payload = (const char *)(buffer + sizeof(protocol_header_t));
                    
                    if (payload_size > 0) {
                        size_t label_len = strnlen(payload, DATACHANNEL_MAX_LABEL_SIZE - 1);
                        strncpy(config.label, payload, label_len);
                        config.label[label_len] = '\0';
                    }
                    
                    ch = datachannel_create(conn, &config);
                    
                    if (ch && conn->on_channel) {
                        conn->on_channel(conn, ch, conn->callback_user_data);
                    }
                }
                
                if (ch) {
                    /* Send OPEN_ACK */
                    send_message(conn, channel_id, MSG_TYPE_OPEN_ACK, 0, sequence, NULL, 0);
                    ch->state = DATACHANNEL_STATE_OPEN;
                    
                    if (ch->on_open) {
                        ch->on_open(ch, ch->on_open_user_data);
                    }
                }
                break;
            }
                
            case MSG_TYPE_OPEN_ACK: {
                /* Find channel and mark as open */
                for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
                    if (ch->id == channel_id) {
                        remove_from_retransmit_queue(conn, channel_id, sequence);
                        ch->state = DATACHANNEL_STATE_OPEN;
                        
                        if (ch->on_open) {
                            ch->on_open(ch, ch->on_open_user_data);
                        }
                        break;
                    }
                }
                break;
            }
                
            case MSG_TYPE_CLOSE: {
                for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
                    if (ch->id == channel_id) {
                        ch->state = DATACHANNEL_STATE_CLOSED;
                        
                        if (ch->on_close) {
                            ch->on_close(ch, ch->on_close_user_data);
                        }
                        break;
                    }
                }
                break;
            }
                
            case MSG_TYPE_DATA: {
                /* Find channel */
                datachannel_t *ch = NULL;
                for (datachannel_t *c = conn->channels; c; c = c->next) {
                    if (c->id == channel_id) {
                        ch = c;
                        break;
                    }
                }
                
                if (!ch) break;
                
                /* Send ACK if reliable */
                if (header->flags & MSG_FLAG_RELIABLE) {
                    send_message(conn, channel_id, MSG_TYPE_ACK, 0, sequence, 
                                &timestamp, sizeof(timestamp));
                }
                
                /* Check sequence for ordered delivery */
                if (header->flags & MSG_FLAG_ORDERED) {
                    if (sequence != ch->recv_sequence) {
                        /* Out of order - for now, just accept anyway */
                        /* TODO: Implement reordering buffer */
                    }
                    ch->recv_sequence = sequence + 1;
                }
                
                /* Deliver message */
                size_t payload_size = received - sizeof(protocol_header_t);
                const void *payload = buffer + sizeof(protocol_header_t);
                
                datachannel_msg_type_t msg_type = (header->flags & MSG_FLAG_TEXT) ?
                    DATACHANNEL_MSG_TEXT : DATACHANNEL_MSG_BINARY;
                
                ch->stats.messages_received++;
                ch->stats.bytes_received += payload_size;
                
                if (ch->on_message) {
                    ch->on_message(ch, payload, payload_size, msg_type,
                                   ch->on_message_user_data);
                }
                break;
            }
                
            default:
                break;
        }
    }
    
    /* Process retransmits */
    process_retransmits(conn);
    
    /* Send heartbeat if needed */
    uint64_t now = get_time_ms();
    if (conn->heartbeat_interval_ms > 0 &&
        now - conn->last_send_time_ms >= (uint64_t)conn->heartbeat_interval_ms) {
        send_message(conn, 0, MSG_TYPE_HEARTBEAT, 0, 0, NULL, 0);
    }
    
    /* Check connection timeout */
    if (conn->connected && now - conn->last_recv_time_ms > HEARTBEAT_TIMEOUT_MS) {
        conn->connected = false;
        /* Notify all channels */
        for (datachannel_t *ch = conn->channels; ch; ch = ch->next) {
            if (ch->state == DATACHANNEL_STATE_OPEN && ch->on_error) {
                ch->on_error(ch, "Connection timeout", ch->on_error_user_data);
            }
        }
    }
    
    return VOICE_OK;
}

bool datachannel_connection_is_connected(datachannel_connection_t *conn) {
    return conn ? conn->connected : false;
}

uint16_t datachannel_connection_get_local_port(datachannel_connection_t *conn) {
    return conn ? ntohs(conn->local_addr.sin_port) : 0;
}

/* ============================================
 * Channel Implementation
 * ============================================ */

void datachannel_config_init(datachannel_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(datachannel_config_t));
    config->type = DATACHANNEL_RELIABLE;
    config->priority = DATACHANNEL_PRIORITY_MEDIUM;
    config->ordered = true;
    config->max_retransmits = -1;  /* Infinite */
    config->max_lifetime_ms = -1;  /* Infinite */
    config->negotiated = false;
    config->id = 0;
}

datachannel_t *datachannel_create(datachannel_connection_t *conn,
                                   const datachannel_config_t *config) {
    if (!conn || !config) return NULL;
    
    datachannel_t *ch = (datachannel_t *)calloc(1, sizeof(datachannel_t));
    if (!ch) return NULL;
    
    ch->connection = conn;
    
    /* Assign ID */
    if (config->negotiated) {
        ch->id = config->id;
    } else {
        ch->id = conn->next_channel_id;
        conn->next_channel_id += 2; /* Skip by 2 for odd/even separation */
    }
    
    /* Copy configuration */
    strncpy(ch->label, config->label, DATACHANNEL_MAX_LABEL_SIZE - 1);
    strncpy(ch->protocol, config->protocol, DATACHANNEL_MAX_PROTOCOL_SIZE - 1);
    ch->type = config->type;
    ch->priority = config->priority;
    ch->ordered = config->ordered;
    ch->max_retransmits = config->max_retransmits;
    ch->max_lifetime_ms = config->max_lifetime_ms;
    
    /* Set reliability based on type */
    switch (config->type) {
        case DATACHANNEL_RELIABLE:
        case DATACHANNEL_RELIABLE_UNORDERED:
            ch->max_retransmits = -1;
            break;
        case DATACHANNEL_UNRELIABLE:
        case DATACHANNEL_UNRELIABLE_UNORDERED:
            ch->max_retransmits = 0;
            break;
    }
    
    if (config->type == DATACHANNEL_RELIABLE_UNORDERED ||
        config->type == DATACHANNEL_UNRELIABLE_UNORDERED) {
        ch->ordered = false;
    }
    
    /* Initialize state */
    ch->state = config->negotiated ? DATACHANNEL_STATE_CONNECTING : DATACHANNEL_STATE_CONNECTING;
    ch->send_sequence = 0;
    ch->recv_sequence = 0;
    ch->buffered_amount = 0;
    ch->buffered_amount_low_threshold = 0;
    memset(&ch->stats, 0, sizeof(ch->stats));
    
    /* Add to connection's channel list */
    ch->next = conn->channels;
    conn->channels = ch;
    
    /* Send OPEN if not negotiated and we're initiating */
    if (!config->negotiated) {
        send_message(conn, ch->id, MSG_TYPE_OPEN, 0, ch->send_sequence,
                     ch->label, strlen(ch->label) + 1);
        
        /* Add to retransmit queue */
        size_t msg_size = sizeof(protocol_header_t) + strlen(ch->label) + 1;
        uint8_t *msg = (uint8_t *)malloc(msg_size);
        if (msg) {
            protocol_header_t *hdr = (protocol_header_t *)msg;
            hdr->type = MSG_TYPE_OPEN;
            hdr->flags = 0;
            hdr->channel_id = htons(ch->id);
            hdr->sequence = htonl(ch->send_sequence);
            hdr->timestamp = htonl(get_timestamp());
            memcpy(msg + sizeof(protocol_header_t), ch->label, strlen(ch->label) + 1);
            
            add_to_retransmit_queue(conn, ch->id, ch->send_sequence,
                                     msg, msg_size, -1, 10000);
            free(msg);
        }
        ch->send_sequence++;
    }
    
    return ch;
}

voice_error_t datachannel_close(datachannel_t *channel) {
    if (!channel) return VOICE_ERROR_INVALID_PARAM;
    
    if (channel->state == DATACHANNEL_STATE_CLOSED) {
        return VOICE_OK;
    }
    
    channel->state = DATACHANNEL_STATE_CLOSING;
    
    /* Send CLOSE */
    send_message(channel->connection, channel->id, MSG_TYPE_CLOSE, 0, 0, NULL, 0);
    
    channel->state = DATACHANNEL_STATE_CLOSED;
    
    if (channel->on_close) {
        channel->on_close(channel, channel->on_close_user_data);
    }
    
    return VOICE_OK;
}

voice_error_t datachannel_send(datachannel_t *channel,
                                const void *data,
                                size_t size) {
    if (!channel || !data || size == 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (channel->state != DATACHANNEL_STATE_OPEN) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    if (size > DATACHANNEL_MAX_MESSAGE_SIZE) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    datachannel_connection_t *conn = channel->connection;
    
    /* Build flags */
    uint8_t flags = MSG_FLAG_BEGIN | MSG_FLAG_END; /* Single message */
    if (channel->ordered) flags |= MSG_FLAG_ORDERED;
    if (channel->max_retransmits != 0) flags |= MSG_FLAG_RELIABLE;
    
    /* Send message */
    voice_error_t err = send_message(conn, channel->id, MSG_TYPE_DATA, flags,
                                      channel->send_sequence, data, size);
    
    if (err != VOICE_OK) {
        return err;
    }
    
    /* Add to retransmit queue if reliable */
    if (flags & MSG_FLAG_RELIABLE) {
        size_t msg_size = sizeof(protocol_header_t) + size;
        uint8_t *msg = (uint8_t *)malloc(msg_size);
        if (msg) {
            protocol_header_t *hdr = (protocol_header_t *)msg;
            hdr->type = MSG_TYPE_DATA;
            hdr->flags = flags;
            hdr->channel_id = htons(channel->id);
            hdr->sequence = htonl(channel->send_sequence);
            hdr->timestamp = htonl(get_timestamp());
            memcpy(msg + sizeof(protocol_header_t), data, size);
            
            add_to_retransmit_queue(conn, channel->id, channel->send_sequence,
                                     msg, msg_size, channel->max_retransmits,
                                     channel->max_lifetime_ms);
            free(msg);
        }
        
        channel->buffered_amount += (uint32_t)size;
    }
    
    channel->send_sequence++;
    channel->stats.messages_sent++;
    channel->stats.bytes_sent += size;
    
    return VOICE_OK;
}

voice_error_t datachannel_send_text(datachannel_t *channel, const char *text) {
    if (!channel || !text) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    if (channel->state != DATACHANNEL_STATE_OPEN) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }
    
    size_t len = strlen(text);
    if (len > DATACHANNEL_MAX_MESSAGE_SIZE) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    datachannel_connection_t *conn = channel->connection;
    
    /* Build flags with TEXT flag */
    uint8_t flags = MSG_FLAG_BEGIN | MSG_FLAG_END | MSG_FLAG_TEXT;
    if (channel->ordered) flags |= MSG_FLAG_ORDERED;
    if (channel->max_retransmits != 0) flags |= MSG_FLAG_RELIABLE;
    
    /* Send message */
    voice_error_t err = send_message(conn, channel->id, MSG_TYPE_DATA, flags,
                                      channel->send_sequence, text, len);
    
    if (err != VOICE_OK) {
        return err;
    }
    
    /* Add to retransmit queue if reliable */
    if (flags & MSG_FLAG_RELIABLE) {
        size_t msg_size = sizeof(protocol_header_t) + len;
        uint8_t *msg = (uint8_t *)malloc(msg_size);
        if (msg) {
            protocol_header_t *hdr = (protocol_header_t *)msg;
            hdr->type = MSG_TYPE_DATA;
            hdr->flags = flags;
            hdr->channel_id = htons(channel->id);
            hdr->sequence = htonl(channel->send_sequence);
            hdr->timestamp = htonl(get_timestamp());
            memcpy(msg + sizeof(protocol_header_t), text, len);
            
            add_to_retransmit_queue(conn, channel->id, channel->send_sequence,
                                     msg, msg_size, channel->max_retransmits,
                                     channel->max_lifetime_ms);
            free(msg);
        }
        
        channel->buffered_amount += (uint32_t)len;
    }
    
    channel->send_sequence++;
    channel->stats.messages_sent++;
    channel->stats.bytes_sent += len;
    
    return VOICE_OK;
}

datachannel_state_t datachannel_get_state(datachannel_t *channel) {
    return channel ? channel->state : DATACHANNEL_STATE_CLOSED;
}

uint16_t datachannel_get_id(datachannel_t *channel) {
    return channel ? channel->id : 0;
}

const char *datachannel_get_label(datachannel_t *channel) {
    return channel ? channel->label : "";
}

const char *datachannel_get_protocol(datachannel_t *channel) {
    return channel ? channel->protocol : "";
}

uint32_t datachannel_get_buffered_amount(datachannel_t *channel) {
    if (!channel) return 0;
    
    /* Calculate current buffered amount from retransmit queue */
    uint32_t buffered = 0;
    datachannel_connection_t *conn = channel->connection;
    
    for (retransmit_entry_t *e = conn->retransmit_head; e; e = e->next) {
        if (e->channel_id == channel->id) {
            buffered += (uint32_t)e->size;
        }
    }
    
    return buffered;
}

void datachannel_set_buffered_amount_low_threshold(datachannel_t *channel,
                                                    uint32_t threshold) {
    if (channel) {
        channel->buffered_amount_low_threshold = threshold;
    }
}

voice_error_t datachannel_get_stats(datachannel_t *channel,
                                     datachannel_stats_t *stats) {
    if (!channel || !stats) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    *stats = channel->stats;
    stats->buffered_amount = datachannel_get_buffered_amount(channel);
    stats->rtt_ms = channel->connection->srtt_ms;
    
    return VOICE_OK;
}

/* ============================================
 * Callback Setters
 * ============================================ */

void datachannel_set_on_open(datachannel_t *channel,
                              datachannel_on_open_t callback,
                              void *user_data) {
    if (channel) {
        channel->on_open = callback;
        channel->on_open_user_data = user_data;
    }
}

void datachannel_set_on_close(datachannel_t *channel,
                               datachannel_on_close_t callback,
                               void *user_data) {
    if (channel) {
        channel->on_close = callback;
        channel->on_close_user_data = user_data;
    }
}

void datachannel_set_on_error(datachannel_t *channel,
                               datachannel_on_error_t callback,
                               void *user_data) {
    if (channel) {
        channel->on_error = callback;
        channel->on_error_user_data = user_data;
    }
}

void datachannel_set_on_message(datachannel_t *channel,
                                 datachannel_on_message_t callback,
                                 void *user_data) {
    if (channel) {
        channel->on_message = callback;
        channel->on_message_user_data = user_data;
    }
}

void datachannel_set_on_buffered_low(datachannel_t *channel,
                                      datachannel_on_buffered_low_t callback,
                                      void *user_data) {
    if (channel) {
        channel->on_buffered_low = callback;
        channel->on_buffered_low_user_data = user_data;
    }
}

/* ============================================
 * Utility Functions
 * ============================================ */

const char *datachannel_state_to_string(datachannel_state_t state) {
    switch (state) {
        case DATACHANNEL_STATE_CONNECTING: return "connecting";
        case DATACHANNEL_STATE_OPEN: return "open";
        case DATACHANNEL_STATE_CLOSING: return "closing";
        case DATACHANNEL_STATE_CLOSED: return "closed";
        default: return "unknown";
    }
}

const char *datachannel_type_to_string(datachannel_type_t type) {
    switch (type) {
        case DATACHANNEL_RELIABLE: return "reliable";
        case DATACHANNEL_RELIABLE_UNORDERED: return "reliable-unordered";
        case DATACHANNEL_UNRELIABLE: return "unreliable";
        case DATACHANNEL_UNRELIABLE_UNORDERED: return "unreliable-unordered";
        default: return "unknown";
    }
}

/* ============================================
 * Quick API
 * ============================================ */

datachannel_t *datachannel_create_reliable(datachannel_connection_t *conn,
                                            const char *label) {
    datachannel_config_t config;
    datachannel_config_init(&config);
    config.type = DATACHANNEL_RELIABLE;
    
    if (label) {
        strncpy(config.label, label, DATACHANNEL_MAX_LABEL_SIZE - 1);
    }
    
    return datachannel_create(conn, &config);
}

datachannel_t *datachannel_create_unreliable(datachannel_connection_t *conn,
                                              const char *label) {
    datachannel_config_t config;
    datachannel_config_init(&config);
    config.type = DATACHANNEL_UNRELIABLE_UNORDERED;
    
    if (label) {
        strncpy(config.label, label, DATACHANNEL_MAX_LABEL_SIZE - 1);
    }
    
    return datachannel_create(conn, &config);
}

datachannel_t *datachannel_create_with_retransmits(datachannel_connection_t *conn,
                                                    const char *label,
                                                    int max_retransmits) {
    datachannel_config_t config;
    datachannel_config_init(&config);
    config.type = DATACHANNEL_RELIABLE;
    config.max_retransmits = max_retransmits;
    
    if (label) {
        strncpy(config.label, label, DATACHANNEL_MAX_LABEL_SIZE - 1);
    }
    
    return datachannel_create(conn, &config);
}

datachannel_t *datachannel_create_with_lifetime(datachannel_connection_t *conn,
                                                 const char *label,
                                                 int max_lifetime_ms) {
    datachannel_config_t config;
    datachannel_config_init(&config);
    config.type = DATACHANNEL_RELIABLE;
    config.max_lifetime_ms = max_lifetime_ms;
    
    if (label) {
        strncpy(config.label, label, DATACHANNEL_MAX_LABEL_SIZE - 1);
    }
    
    return datachannel_create(conn, &config);
}
