/**
 * @file sip_ua.c
 * @brief SIP User Agent Implementation
 */

#include "sip/sip_ua.h"
#include "sip/sip_core.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define closesocket close
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

/* Maximum concurrent calls */
#define MAX_CALLS 16

/* ============================================
 * SIP Call Structure
 * ============================================ */

struct sip_call_s {
    sip_ua_t *ua;
    sip_call_info_t info;
    sip_call_state_t state;
    
    /* Dialog state */
    char local_tag[64];
    char remote_tag[64];
    uint32_t local_cseq;
    uint32_t remote_cseq;
    
    /* Transaction state */
    sip_message_t *last_request;
    sip_message_t *last_response;
    
    /* RTP */
    uint16_t local_rtp_port;
    
    /* Timing */
    uint64_t created_time;
    uint64_t last_activity;
    
    bool active;
};

/* ============================================
 * SIP UA Structure
 * ============================================ */

struct sip_ua_s {
    sip_ua_config_t config;
    
    /* Network */
    SOCKET socket;
    struct sockaddr_in local_addr;
    bool running;
    
    /* Registration */
    sip_registration_state_t reg_state;
    char reg_call_id[128];
    uint32_t reg_cseq;
    int reg_expires;
    uint64_t reg_time;
    
    /* Calls */
    sip_call_t calls[MAX_CALLS];
    int call_count;
    
    /* RTP port allocation */
    uint16_t next_rtp_port;
    
    /* Local URI */
    sip_uri_t local_uri;
    char local_host[64];
};

/* ============================================
 * Helper Functions
 * ============================================ */

static uint64_t get_time_ms(void)
{
#ifdef _WIN32
    return GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

static uint16_t allocate_rtp_port(sip_ua_t *ua)
{
    uint16_t port = ua->next_rtp_port;
    ua->next_rtp_port += 2;  /* RTP uses even ports, RTCP uses odd */
    if (ua->next_rtp_port > ua->config.rtp_port_max) {
        ua->next_rtp_port = ua->config.rtp_port_min;
    }
    return port;
}

static sip_call_t *find_call_by_id(sip_ua_t *ua, const char *call_id)
{
    for (int i = 0; i < MAX_CALLS; i++) {
        if (ua->calls[i].active && 
            strcmp(ua->calls[i].info.call_id, call_id) == 0) {
            return &ua->calls[i];
        }
    }
    return NULL;
}

static sip_call_t *allocate_call(sip_ua_t *ua)
{
    for (int i = 0; i < MAX_CALLS; i++) {
        if (!ua->calls[i].active) {
            memset(&ua->calls[i], 0, sizeof(sip_call_t));
            ua->calls[i].ua = ua;
            ua->calls[i].active = true;
            ua->calls[i].created_time = get_time_ms();
            ua->calls[i].local_rtp_port = allocate_rtp_port(ua);
            ua->call_count++;
            return &ua->calls[i];
        }
    }
    return NULL;
}

static void release_call(sip_call_t *call)
{
    if (!call || !call->active) return;
    
    if (call->last_request) {
        sip_message_destroy(call->last_request);
        call->last_request = NULL;
    }
    if (call->last_response) {
        sip_message_destroy(call->last_response);
        call->last_response = NULL;
    }
    
    call->active = false;
    call->ua->call_count--;
}

static void set_call_state(sip_call_t *call, sip_call_state_t state)
{
    if (!call) return;
    
    call->state = state;
    call->info.state = state;
    call->last_activity = get_time_ms();
    
    if (call->ua->config.on_call_state) {
        call->ua->config.on_call_state(call->ua, call, state,
                                        call->ua->config.callback_user_data);
    }
    
    if (state == SIP_CALL_CONFIRMED) {
        call->info.connect_time = get_time_ms();
    } else if (state == SIP_CALL_DISCONNECTED) {
        call->info.end_time = get_time_ms();
    }
}

/* ============================================
 * Network Functions
 * ============================================ */

static voice_error_t send_message(sip_ua_t *ua, const sip_message_t *msg,
                                   const char *host, uint16_t port)
{
    char buffer[8192];
    size_t size;
    
    voice_error_t err = sip_message_serialize(msg, buffer, sizeof(buffer), &size);
    if (err != VOICE_OK) return err;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    struct hostent *he = gethostbyname(host);
    if (he) {
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);
    } else {
        return VOICE_ERROR_NETWORK;
    }
    
    ssize_t sent = sendto(ua->socket, buffer, (int)size, 0,
                          (struct sockaddr*)&addr, sizeof(addr));
    
    return (sent > 0) ? VOICE_OK : VOICE_ERROR_NETWORK;
}

static voice_error_t receive_message(sip_ua_t *ua, sip_message_t *msg,
                                      char *from_host, uint16_t *from_port,
                                      int timeout_ms)
{
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(ua->socket, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select((int)ua->socket + 1, &readfds, NULL, NULL, &tv);
    if (result <= 0) {
        return VOICE_ERROR_TIMEOUT;
    }
    
    char buffer[8192];
    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    
    ssize_t received = recvfrom(ua->socket, buffer, sizeof(buffer) - 1, 0,
                                (struct sockaddr*)&from_addr, &from_len);
    
    if (received <= 0) {
        return VOICE_ERROR_NETWORK;
    }
    
    buffer[received] = '\0';
    
    if (from_host) {
        inet_ntop(AF_INET, &from_addr.sin_addr, from_host, 64);
    }
    if (from_port) {
        *from_port = ntohs(from_addr.sin_port);
    }
    
    return sip_message_parse(buffer, received, msg);
}

/* ============================================
 * SIP UA Configuration
 * ============================================ */

void sip_ua_config_init(sip_ua_config_t *config)
{
    if (!config) return;
    memset(config, 0, sizeof(sip_ua_config_t));
    
    config->registrar_port = SIP_DEFAULT_PORT;
    config->proxy_port = SIP_DEFAULT_PORT;
    config->transport = SIP_TRANSPORT_UDP;
    config->local_port = 5060;
    
    config->auto_register = true;
    config->register_expires = 3600;
    config->register_retry_interval = 60;
    
    config->rtp_port_min = 10000;
    config->rtp_port_max = 20000;
}

/* ============================================
 * SIP UA Core Functions
 * ============================================ */

sip_ua_t *sip_ua_create(const sip_ua_config_t *config)
{
    if (!config) return NULL;
    
    sip_ua_t *ua = (sip_ua_t *)calloc(1, sizeof(sip_ua_t));
    if (!ua) return NULL;
    
    ua->config = *config;
    ua->socket = INVALID_SOCKET;
    ua->reg_state = SIP_REG_UNREGISTERED;
    ua->next_rtp_port = config->rtp_port_min;
    
    /* Build local URI */
    ua->local_uri.scheme = SIP_URI_SCHEME_SIP;
    strncpy(ua->local_uri.user, config->username, sizeof(ua->local_uri.user) - 1);
    strncpy(ua->local_uri.host, config->domain, sizeof(ua->local_uri.host) - 1);
    ua->local_uri.port = SIP_DEFAULT_PORT;
    
#ifdef _WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
#endif
    
    return ua;
}

void sip_ua_destroy(sip_ua_t *ua)
{
    if (!ua) return;
    
    sip_ua_stop(ua);
    
    /* Release all calls */
    for (int i = 0; i < MAX_CALLS; i++) {
        if (ua->calls[i].active) {
            release_call(&ua->calls[i]);
        }
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    free(ua);
}

voice_error_t sip_ua_start(sip_ua_t *ua)
{
    if (!ua) return VOICE_ERROR_INVALID_PARAM;
    
    /* Create UDP socket */
    ua->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (ua->socket == INVALID_SOCKET) {
        return VOICE_ERROR_NETWORK;
    }
    
    /* Bind to local port */
    memset(&ua->local_addr, 0, sizeof(ua->local_addr));
    ua->local_addr.sin_family = AF_INET;
    ua->local_addr.sin_addr.s_addr = INADDR_ANY;
    ua->local_addr.sin_port = htons(ua->config.local_port);
    
    if (bind(ua->socket, (struct sockaddr*)&ua->local_addr, 
             sizeof(ua->local_addr)) < 0) {
        closesocket(ua->socket);
        ua->socket = INVALID_SOCKET;
        return VOICE_ERROR_NETWORK;
    }
    
    /* Get local IP */
    if (ua->config.local_host[0]) {
        strncpy(ua->local_host, ua->config.local_host, sizeof(ua->local_host) - 1);
    } else {
        /* Try to get local IP - simplified */
        strncpy(ua->local_host, "127.0.0.1", sizeof(ua->local_host) - 1);
        
        /* Attempt to get real IP via connect trick */
        SOCKET temp = socket(AF_INET, SOCK_DGRAM, 0);
        if (temp != INVALID_SOCKET) {
            struct sockaddr_in dest;
            dest.sin_family = AF_INET;
            dest.sin_port = htons(80);
            inet_pton(AF_INET, "8.8.8.8", &dest.sin_addr);
            
            if (connect(temp, (struct sockaddr*)&dest, sizeof(dest)) == 0) {
                struct sockaddr_in local;
                socklen_t len = sizeof(local);
                if (getsockname(temp, (struct sockaddr*)&local, &len) == 0) {
                    inet_ntop(AF_INET, &local.sin_addr, ua->local_host, 
                             sizeof(ua->local_host));
                }
            }
            closesocket(temp);
        }
    }
    
    ua->running = true;
    
    /* Auto-register if configured */
    if (ua->config.auto_register && ua->config.registrar_host[0]) {
        sip_ua_register(ua);
    }
    
    return VOICE_OK;
}

voice_error_t sip_ua_stop(sip_ua_t *ua)
{
    if (!ua) return VOICE_ERROR_INVALID_PARAM;
    
    ua->running = false;
    
    /* Unregister if registered */
    if (ua->reg_state == SIP_REG_REGISTERED) {
        sip_ua_unregister(ua);
    }
    
    /* Hang up all calls */
    for (int i = 0; i < MAX_CALLS; i++) {
        if (ua->calls[i].active && 
            ua->calls[i].state != SIP_CALL_DISCONNECTED) {
            sip_call_hangup(&ua->calls[i]);
        }
    }
    
    if (ua->socket != INVALID_SOCKET) {
        closesocket(ua->socket);
        ua->socket = INVALID_SOCKET;
    }
    
    return VOICE_OK;
}

voice_error_t sip_ua_process(sip_ua_t *ua, int timeout_ms)
{
    if (!ua || !ua->running) return VOICE_ERROR_INVALID_PARAM;
    
    sip_message_t msg;
    char from_host[64];
    uint16_t from_port;
    
    voice_error_t err = receive_message(ua, &msg, from_host, &from_port, timeout_ms);
    if (err == VOICE_ERROR_TIMEOUT) {
        /* Check for registration refresh */
        if (ua->reg_state == SIP_REG_REGISTERED) {
            uint64_t now = get_time_ms();
            uint64_t refresh_time = ua->reg_time + (ua->reg_expires * 1000 / 2);
            if (now >= refresh_time) {
                sip_ua_register(ua);
            }
        }
        return VOICE_OK;
    }
    
    if (err != VOICE_OK) {
        return err;
    }
    
    /* Process received message */
    if (msg.type == SIP_MSG_RESPONSE) {
        /* Handle response */
        sip_call_t *call = find_call_by_id(ua, msg.call_id.call_id);
        
        /* Check if it's a registration response */
        if (strcmp(msg.call_id.call_id, ua->reg_call_id) == 0) {
            if (msg.status_code >= 200 && msg.status_code < 300) {
                ua->reg_state = SIP_REG_REGISTERED;
                ua->reg_time = get_time_ms();
                
                /* Get expires from response */
                const char *expires = sip_message_get_header(&msg, "Expires");
                if (expires) {
                    ua->reg_expires = atoi(expires);
                }
                
                if (ua->config.on_registration) {
                    ua->config.on_registration(ua, SIP_REG_REGISTERED, 
                                               ua->reg_expires,
                                               ua->config.callback_user_data);
                }
            } else if (msg.status_code >= 400) {
                ua->reg_state = SIP_REG_FAILED;
                if (ua->config.on_registration) {
                    ua->config.on_registration(ua, SIP_REG_FAILED, 0,
                                               ua->config.callback_user_data);
                }
            }
        } else if (call) {
            /* Handle call response */
            if (msg.cseq.method == SIP_METHOD_INVITE) {
                if (msg.status_code == 100) {
                    /* Trying - do nothing */
                } else if (msg.status_code >= 180 && msg.status_code < 200) {
                    /* Ringing/Progress */
                    set_call_state(call, SIP_CALL_EARLY);
                } else if (msg.status_code >= 200 && msg.status_code < 300) {
                    /* Success - send ACK */
                    sip_message_t ack;
                    sip_message_create_ack(&ack, call->last_request);
                    
                    /* Add Via */
                    ack.via[0].port = ua->config.local_port;
                    strncpy(ack.via[0].host, ua->local_host, sizeof(ack.via[0].host) - 1);
                    strncpy(ack.via[0].transport, "UDP", sizeof(ack.via[0].transport) - 1);
                    sip_generate_branch(ack.via[0].branch, sizeof(ack.via[0].branch));
                    ack.via_count = 1;
                    
                    send_message(ua, &ack, from_host, from_port);
                    
                    /* Store remote tag */
                    strncpy(call->remote_tag, msg.to.tag, sizeof(call->remote_tag) - 1);
                    
                    /* Parse SDP for media info */
                    if (msg.body) {
                        strncpy(call->info.remote_sdp, msg.body, 
                               sizeof(call->info.remote_sdp) - 1);
                        
                        /* Notify about media */
                        if (ua->config.on_call_media) {
                            /* Simple SDP parsing for RTP info */
                            char *c_line = strstr(msg.body, "c=IN IP4 ");
                            char *m_line = strstr(msg.body, "m=audio ");
                            
                            if (c_line && m_line) {
                                c_line += 9;
                                char *end = strchr(c_line, '\r');
                                if (end) *end = '\0';
                                strncpy(call->info.remote_rtp_host, c_line, 
                                       sizeof(call->info.remote_rtp_host) - 1);
                                if (end) *end = '\r';
                                
                                m_line += 8;
                                call->info.remote_rtp_port = (uint16_t)atoi(m_line);
                                
                                ua->config.on_call_media(ua, call, 
                                                          call->info.remote_rtp_host,
                                                          call->info.remote_rtp_port,
                                                          ua->config.callback_user_data);
                            }
                        }
                    }
                    
                    set_call_state(call, SIP_CALL_CONFIRMED);
                } else if (msg.status_code >= 400) {
                    /* Failed */
                    set_call_state(call, SIP_CALL_FAILED);
                    release_call(call);
                }
            } else if (msg.cseq.method == SIP_METHOD_BYE) {
                if (msg.status_code >= 200) {
                    set_call_state(call, SIP_CALL_DISCONNECTED);
                    release_call(call);
                }
            }
        }
    } else {
        /* Handle request */
        if (msg.method == SIP_METHOD_INVITE) {
            /* Incoming call */
            sip_call_t *call = allocate_call(ua);
            if (call) {
                strncpy(call->info.call_id, msg.call_id.call_id, 
                       sizeof(call->info.call_id) - 1);
                call->info.direction = SIP_CALL_INCOMING_DIR;
                call->info.local = msg.to;
                call->info.remote = msg.from;
                call->info.start_time = get_time_ms();
                
                strncpy(call->remote_tag, msg.from.tag, sizeof(call->remote_tag) - 1);
                sip_generate_tag(call->local_tag, sizeof(call->local_tag));
                call->remote_cseq = msg.cseq.seq;
                
                if (msg.body) {
                    strncpy(call->info.remote_sdp, msg.body,
                           sizeof(call->info.remote_sdp) - 1);
                }
                
                /* Send 180 Ringing */
                sip_message_t ringing;
                sip_message_create_response(&ringing, &msg, 180, "Ringing");
                strncpy(ringing.to.tag, call->local_tag, sizeof(ringing.to.tag) - 1);
                send_message(ua, &ringing, from_host, from_port);
                
                set_call_state(call, SIP_CALL_INCOMING);
                
                if (ua->config.on_incoming_call) {
                    ua->config.on_incoming_call(ua, call, &call->info,
                                                 ua->config.callback_user_data);
                }
            } else {
                /* No available call slots - send 486 Busy */
                sip_message_t busy;
                sip_message_create_response(&busy, &msg, 486, "Busy Here");
                send_message(ua, &busy, from_host, from_port);
            }
        } else if (msg.method == SIP_METHOD_ACK) {
            /* ACK received - call is fully established */
            sip_call_t *call = find_call_by_id(ua, msg.call_id.call_id);
            if (call && call->state == SIP_CALL_CONNECTING) {
                set_call_state(call, SIP_CALL_CONFIRMED);
            }
        } else if (msg.method == SIP_METHOD_BYE) {
            /* Hang up request */
            sip_call_t *call = find_call_by_id(ua, msg.call_id.call_id);
            if (call) {
                /* Send 200 OK */
                sip_message_t ok;
                sip_message_create_response(&ok, &msg, 200, "OK");
                send_message(ua, &ok, from_host, from_port);
                
                set_call_state(call, SIP_CALL_DISCONNECTED);
                release_call(call);
            } else {
                /* Send 481 Call Does Not Exist */
                sip_message_t nope;
                sip_message_create_response(&nope, &msg, 481, 
                                            "Call/Transaction Does Not Exist");
                send_message(ua, &nope, from_host, from_port);
            }
        } else if (msg.method == SIP_METHOD_CANCEL) {
            /* Cancel incoming call */
            sip_call_t *call = find_call_by_id(ua, msg.call_id.call_id);
            if (call) {
                /* Send 200 OK to CANCEL */
                sip_message_t ok;
                sip_message_create_response(&ok, &msg, 200, "OK");
                send_message(ua, &ok, from_host, from_port);
                
                /* Send 487 to original INVITE */
                sip_message_t terminated;
                terminated.type = SIP_MSG_RESPONSE;
                terminated.status_code = 487;
                strcpy(terminated.reason_phrase, "Request Terminated");
                /* ... copy headers from original INVITE ... */
                
                set_call_state(call, SIP_CALL_DISCONNECTED);
                release_call(call);
            }
        } else if (msg.method == SIP_METHOD_OPTIONS) {
            /* Keep-alive / capability query */
            sip_message_t ok;
            sip_message_create_response(&ok, &msg, 200, "OK");
            sip_message_add_header(&ok, "Allow", 
                                   "INVITE, ACK, BYE, CANCEL, OPTIONS, INFO");
            send_message(ua, &ok, from_host, from_port);
        }
    }
    
    /* Clean up message body if allocated */
    if (msg.body) {
        free(msg.body);
    }
    
    return VOICE_OK;
}

/* ============================================
 * Registration
 * ============================================ */

voice_error_t sip_ua_register(sip_ua_t *ua)
{
    if (!ua || !ua->running) return VOICE_ERROR_INVALID_PARAM;
    if (!ua->config.registrar_host[0]) return VOICE_ERROR_INVALID_PARAM;
    
    sip_message_t reg;
    sip_uri_t registrar;
    
    registrar.scheme = SIP_URI_SCHEME_SIP;
    strncpy(registrar.host, ua->config.registrar_host, sizeof(registrar.host) - 1);
    registrar.port = ua->config.registrar_port;
    
    sip_address_t contact;
    memset(&contact, 0, sizeof(contact));
    contact.uri.scheme = SIP_URI_SCHEME_SIP;
    strncpy(contact.uri.user, ua->config.username, sizeof(contact.uri.user) - 1);
    strncpy(contact.uri.host, ua->local_host, sizeof(contact.uri.host) - 1);
    contact.uri.port = ua->config.local_port;
    
    sip_message_create_register(&reg, &registrar, &ua->local_uri, &contact,
                                 ua->config.register_expires);
    
    /* Store call-id for matching response */
    strncpy(ua->reg_call_id, reg.call_id.call_id, sizeof(ua->reg_call_id) - 1);
    ua->reg_cseq = reg.cseq.seq;
    
    /* Add Via */
    reg.via[0].port = ua->config.local_port;
    strncpy(reg.via[0].host, ua->local_host, sizeof(reg.via[0].host) - 1);
    strncpy(reg.via[0].transport, "UDP", sizeof(reg.via[0].transport) - 1);
    sip_generate_branch(reg.via[0].branch, sizeof(reg.via[0].branch));
    reg.via_count = 1;
    
    ua->reg_state = SIP_REG_REGISTERING;
    
    if (ua->config.on_registration) {
        ua->config.on_registration(ua, SIP_REG_REGISTERING, 0,
                                    ua->config.callback_user_data);
    }
    
    return send_message(ua, &reg, ua->config.registrar_host, 
                        ua->config.registrar_port);
}

voice_error_t sip_ua_unregister(sip_ua_t *ua)
{
    if (!ua) return VOICE_ERROR_INVALID_PARAM;
    
    /* Send REGISTER with Expires: 0 */
    sip_message_t reg;
    sip_uri_t registrar;
    
    registrar.scheme = SIP_URI_SCHEME_SIP;
    strncpy(registrar.host, ua->config.registrar_host, sizeof(registrar.host) - 1);
    registrar.port = ua->config.registrar_port;
    
    sip_message_create_register(&reg, &registrar, &ua->local_uri, NULL, 0);
    
    /* Add Via */
    reg.via[0].port = ua->config.local_port;
    strncpy(reg.via[0].host, ua->local_host, sizeof(reg.via[0].host) - 1);
    strncpy(reg.via[0].transport, "UDP", sizeof(reg.via[0].transport) - 1);
    sip_generate_branch(reg.via[0].branch, sizeof(reg.via[0].branch));
    reg.via_count = 1;
    
    ua->reg_state = SIP_REG_UNREGISTERING;
    
    return send_message(ua, &reg, ua->config.registrar_host,
                        ua->config.registrar_port);
}

sip_registration_state_t sip_ua_get_registration_state(sip_ua_t *ua)
{
    return ua ? ua->reg_state : SIP_REG_UNREGISTERED;
}

bool sip_ua_is_registered(sip_ua_t *ua)
{
    return ua && ua->reg_state == SIP_REG_REGISTERED;
}

/* ============================================
 * Call Management
 * ============================================ */

sip_call_t *sip_ua_make_call(sip_ua_t *ua, const char *destination)
{
    if (!ua || !ua->running || !destination) return NULL;
    
    sip_call_t *call = allocate_call(ua);
    if (!call) return NULL;
    
    /* Parse destination */
    sip_uri_t to_uri;
    if (strncasecmp(destination, "sip:", 4) == 0) {
        sip_uri_parse(destination, &to_uri);
    } else {
        /* Assume just a username */
        to_uri.scheme = SIP_URI_SCHEME_SIP;
        strncpy(to_uri.user, destination, sizeof(to_uri.user) - 1);
        strncpy(to_uri.host, ua->config.domain, sizeof(to_uri.host) - 1);
        to_uri.port = SIP_DEFAULT_PORT;
    }
    
    /* Generate Call-ID */
    sip_generate_call_id(call->info.call_id, sizeof(call->info.call_id), 
                         ua->local_host);
    sip_generate_tag(call->local_tag, sizeof(call->local_tag));
    call->local_cseq = 1;
    
    call->info.direction = SIP_CALL_OUTGOING;
    call->info.local.uri = ua->local_uri;
    strncpy(call->info.local.tag, call->local_tag, sizeof(call->info.local.tag) - 1);
    call->info.remote.uri = to_uri;
    call->info.start_time = get_time_ms();
    
    /* Generate SDP */
    char sdp[1024];
    snprintf(sdp, sizeof(sdp),
        "v=0\r\n"
        "o=- %lu %lu IN IP4 %s\r\n"
        "s=Voice Call\r\n"
        "c=IN IP4 %s\r\n"
        "t=0 0\r\n"
        "m=audio %u RTP/AVP 0 8 101\r\n"
        "a=rtpmap:0 PCMU/8000\r\n"
        "a=rtpmap:8 PCMA/8000\r\n"
        "a=rtpmap:101 telephone-event/8000\r\n"
        "a=fmtp:101 0-15\r\n"
        "a=sendrecv\r\n",
        (unsigned long)get_time_ms(),
        (unsigned long)get_time_ms(),
        ua->local_host,
        ua->local_host,
        call->local_rtp_port
    );
    strncpy(call->info.local_sdp, sdp, sizeof(call->info.local_sdp) - 1);
    
    /* Create INVITE */
    call->last_request = sip_message_create();
    if (!call->last_request) {
        release_call(call);
        return NULL;
    }
    
    sip_message_create_invite(call->last_request, &to_uri, &ua->local_uri,
                               call->info.call_id, call->local_cseq, sdp);
    
    /* Set From tag */
    strncpy(call->last_request->from.tag, call->local_tag, 
            sizeof(call->last_request->from.tag) - 1);
    
    /* Add Via */
    call->last_request->via[0].port = ua->config.local_port;
    strncpy(call->last_request->via[0].host, ua->local_host, 
            sizeof(call->last_request->via[0].host) - 1);
    strncpy(call->last_request->via[0].transport, "UDP", 
            sizeof(call->last_request->via[0].transport) - 1);
    sip_generate_branch(call->last_request->via[0].branch, 
                        sizeof(call->last_request->via[0].branch));
    call->last_request->via_count = 1;
    
    /* Determine where to send */
    const char *target_host = ua->config.proxy_host[0] ? 
                               ua->config.proxy_host : to_uri.host;
    uint16_t target_port = ua->config.proxy_host[0] ? 
                            ua->config.proxy_port : 
                            (to_uri.port ? to_uri.port : SIP_DEFAULT_PORT);
    
    voice_error_t err = send_message(ua, call->last_request, 
                                      target_host, target_port);
    if (err != VOICE_OK) {
        release_call(call);
        return NULL;
    }
    
    set_call_state(call, SIP_CALL_CALLING);
    
    return call;
}

voice_error_t sip_call_answer(sip_call_t *call, int status_code)
{
    if (!call || !call->ua) return VOICE_ERROR_INVALID_PARAM;
    if (call->state != SIP_CALL_INCOMING) return VOICE_ERROR_INVALID_PARAM;
    
    if (status_code < 200) status_code = 200;
    
    sip_message_t ok;
    memset(&ok, 0, sizeof(ok));
    
    ok.type = SIP_MSG_RESPONSE;
    ok.status_code = status_code;
    strncpy(ok.reason_phrase, sip_status_reason(status_code), 
            sizeof(ok.reason_phrase) - 1);
    
    /* Copy headers from stored INVITE */
    ok.from = call->info.remote;
    ok.to = call->info.local;
    strncpy(ok.to.tag, call->local_tag, sizeof(ok.to.tag) - 1);
    strncpy(ok.call_id.call_id, call->info.call_id, sizeof(ok.call_id.call_id) - 1);
    ok.cseq.seq = call->remote_cseq;
    ok.cseq.method = SIP_METHOD_INVITE;
    ok.max_forwards = 70;
    
    /* Contact */
    ok.contact.uri = call->ua->local_uri;
    strncpy(ok.contact.uri.host, call->ua->local_host, 
            sizeof(ok.contact.uri.host) - 1);
    ok.contact.uri.port = call->ua->config.local_port;
    
    /* Generate SDP */
    char sdp[1024];
    snprintf(sdp, sizeof(sdp),
        "v=0\r\n"
        "o=- %lu %lu IN IP4 %s\r\n"
        "s=Voice Call\r\n"
        "c=IN IP4 %s\r\n"
        "t=0 0\r\n"
        "m=audio %u RTP/AVP 0 8 101\r\n"
        "a=rtpmap:0 PCMU/8000\r\n"
        "a=rtpmap:8 PCMA/8000\r\n"
        "a=rtpmap:101 telephone-event/8000\r\n"
        "a=fmtp:101 0-15\r\n"
        "a=sendrecv\r\n",
        (unsigned long)get_time_ms(),
        (unsigned long)get_time_ms(),
        call->ua->local_host,
        call->ua->local_host,
        call->local_rtp_port
    );
    strncpy(call->info.local_sdp, sdp, sizeof(call->info.local_sdp) - 1);
    sip_message_set_body(&ok, "application/sdp", sdp, strlen(sdp));
    
    /* Parse remote SDP for RTP info */
    if (call->info.remote_sdp[0] && call->ua->config.on_call_media) {
        char *c_line = strstr(call->info.remote_sdp, "c=IN IP4 ");
        char *m_line = strstr(call->info.remote_sdp, "m=audio ");
        
        if (c_line && m_line) {
            c_line += 9;
            char *end = strchr(c_line, '\r');
            if (end) {
                size_t len = end - c_line;
                if (len < sizeof(call->info.remote_rtp_host)) {
                    strncpy(call->info.remote_rtp_host, c_line, len);
                    call->info.remote_rtp_host[len] = '\0';
                }
            }
            
            m_line += 8;
            call->info.remote_rtp_port = (uint16_t)atoi(m_line);
            
            call->ua->config.on_call_media(call->ua, call,
                                            call->info.remote_rtp_host,
                                            call->info.remote_rtp_port,
                                            call->ua->config.callback_user_data);
        }
    }
    
    /* Determine where to send (from Via of INVITE) */
    const char *target_host = call->info.remote.uri.host;
    uint16_t target_port = call->info.remote.uri.port ? 
                           call->info.remote.uri.port : SIP_DEFAULT_PORT;
    
    voice_error_t err = send_message(call->ua, &ok, target_host, target_port);
    
    if (ok.body) free(ok.body);
    
    if (err == VOICE_OK) {
        set_call_state(call, SIP_CALL_CONNECTING);
    }
    
    return err;
}

voice_error_t sip_call_reject(sip_call_t *call, int status_code)
{
    if (!call || !call->ua) return VOICE_ERROR_INVALID_PARAM;
    if (call->state != SIP_CALL_INCOMING) return VOICE_ERROR_INVALID_PARAM;
    
    if (status_code < 400) status_code = 603;  /* Decline */
    
    sip_message_t reject;
    memset(&reject, 0, sizeof(reject));
    
    reject.type = SIP_MSG_RESPONSE;
    reject.status_code = status_code;
    strncpy(reject.reason_phrase, sip_status_reason(status_code),
            sizeof(reject.reason_phrase) - 1);
    
    reject.from = call->info.remote;
    reject.to = call->info.local;
    strncpy(reject.to.tag, call->local_tag, sizeof(reject.to.tag) - 1);
    strncpy(reject.call_id.call_id, call->info.call_id, 
            sizeof(reject.call_id.call_id) - 1);
    reject.cseq.seq = call->remote_cseq;
    reject.cseq.method = SIP_METHOD_INVITE;
    reject.max_forwards = 70;
    
    const char *target_host = call->info.remote.uri.host;
    uint16_t target_port = call->info.remote.uri.port ? 
                           call->info.remote.uri.port : SIP_DEFAULT_PORT;
    
    voice_error_t err = send_message(call->ua, &reject, target_host, target_port);
    
    if (err == VOICE_OK) {
        set_call_state(call, SIP_CALL_DISCONNECTED);
        release_call(call);
    }
    
    return err;
}

voice_error_t sip_call_hangup(sip_call_t *call)
{
    if (!call || !call->ua) return VOICE_ERROR_INVALID_PARAM;
    
    if (call->state == SIP_CALL_DISCONNECTED || 
        call->state == SIP_CALL_IDLE) {
        return VOICE_OK;
    }
    
    sip_message_t bye;
    memset(&bye, 0, sizeof(bye));
    
    call->local_cseq++;
    
    sip_message_create_bye(&bye, call->info.call_id,
                           &call->info.remote.uri,
                           &call->info.local.uri,
                           call->local_cseq);
    
    strncpy(bye.from.tag, call->local_tag, sizeof(bye.from.tag) - 1);
    if (call->remote_tag[0]) {
        strncpy(bye.to.tag, call->remote_tag, sizeof(bye.to.tag) - 1);
    }
    
    /* Add Via */
    bye.via[0].port = call->ua->config.local_port;
    strncpy(bye.via[0].host, call->ua->local_host, sizeof(bye.via[0].host) - 1);
    strncpy(bye.via[0].transport, "UDP", sizeof(bye.via[0].transport) - 1);
    sip_generate_branch(bye.via[0].branch, sizeof(bye.via[0].branch));
    bye.via_count = 1;
    
    const char *target_host = call->info.remote.uri.host;
    uint16_t target_port = call->info.remote.uri.port ? 
                           call->info.remote.uri.port : SIP_DEFAULT_PORT;
    
    set_call_state(call, SIP_CALL_DISCONNECTING);
    
    voice_error_t err = send_message(call->ua, &bye, target_host, target_port);
    
    if (err != VOICE_OK) {
        set_call_state(call, SIP_CALL_DISCONNECTED);
        release_call(call);
    }
    
    return err;
}

voice_error_t sip_call_hold(sip_call_t *call)
{
    /* Would send re-INVITE with a=sendonly/inactive in SDP */
    (void)call;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t sip_call_resume(sip_call_t *call)
{
    /* Would send re-INVITE with a=sendrecv in SDP */
    (void)call;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t sip_call_send_dtmf(sip_call_t *call, char digit, int duration_ms)
{
    /* Would send INFO with application/dtmf-relay */
    (void)call;
    (void)digit;
    (void)duration_ms;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t sip_call_transfer(sip_call_t *call, const char *destination)
{
    /* Would send REFER request */
    (void)call;
    (void)destination;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t sip_call_get_info(sip_call_t *call, sip_call_info_t *info)
{
    if (!call || !info) return VOICE_ERROR_INVALID_PARAM;
    *info = call->info;
    return VOICE_OK;
}

sip_call_state_t sip_call_get_state(sip_call_t *call)
{
    return call ? call->state : SIP_CALL_IDLE;
}

const char *sip_call_get_id(sip_call_t *call)
{
    return call ? call->info.call_id : NULL;
}

voice_error_t sip_call_set_local_sdp(sip_call_t *call, const char *sdp)
{
    if (!call || !sdp) return VOICE_ERROR_INVALID_PARAM;
    strncpy(call->info.local_sdp, sdp, sizeof(call->info.local_sdp) - 1);
    return VOICE_OK;
}

const char *sip_call_get_remote_sdp(sip_call_t *call)
{
    return call ? call->info.remote_sdp : NULL;
}

/* ============================================
 * Utility Functions
 * ============================================ */

const char *sip_call_state_to_string(sip_call_state_t state)
{
    switch (state) {
        case SIP_CALL_IDLE: return "Idle";
        case SIP_CALL_CALLING: return "Calling";
        case SIP_CALL_INCOMING: return "Incoming";
        case SIP_CALL_EARLY: return "Early";
        case SIP_CALL_CONNECTING: return "Connecting";
        case SIP_CALL_CONFIRMED: return "Confirmed";
        case SIP_CALL_DISCONNECTING: return "Disconnecting";
        case SIP_CALL_DISCONNECTED: return "Disconnected";
        case SIP_CALL_FAILED: return "Failed";
        default: return "Unknown";
    }
}

const char *sip_registration_state_to_string(sip_registration_state_t state)
{
    switch (state) {
        case SIP_REG_UNREGISTERED: return "Unregistered";
        case SIP_REG_REGISTERING: return "Registering";
        case SIP_REG_REGISTERED: return "Registered";
        case SIP_REG_UNREGISTERING: return "Unregistering";
        case SIP_REG_FAILED: return "Failed";
        default: return "Unknown";
    }
}

voice_error_t sip_ua_get_local_uri(sip_ua_t *ua, char *buffer, size_t size)
{
    if (!ua || !buffer) return VOICE_ERROR_INVALID_PARAM;
    return sip_uri_to_string(&ua->local_uri, buffer, size);
}

int sip_ua_get_call_count(sip_ua_t *ua)
{
    return ua ? ua->call_count : 0;
}

/* ============================================
 * SDP Helpers
 * ============================================ */

voice_error_t sip_generate_sdp(char *buffer, size_t size,
                                const char *session_name,
                                const char *local_ip,
                                uint16_t rtp_port,
                                const uint8_t *codecs,
                                int codec_count)
{
    if (!buffer || !local_ip || size == 0) return VOICE_ERROR_INVALID_PARAM;
    
    /* Build format list */
    char formats[64] = "";
    char *fp = formats;
    int remaining = sizeof(formats);
    
    for (int i = 0; i < codec_count && remaining > 3; i++) {
        int len = snprintf(fp, remaining, " %u", codecs[i]);
        fp += len;
        remaining -= len;
    }
    
    if (!formats[0]) {
        strcpy(formats, " 0 8");  /* Default: PCMU, PCMA */
    }
    
    int len = snprintf(buffer, size,
        "v=0\r\n"
        "o=- %lu %lu IN IP4 %s\r\n"
        "s=%s\r\n"
        "c=IN IP4 %s\r\n"
        "t=0 0\r\n"
        "m=audio %u RTP/AVP%s\r\n"
        "a=rtpmap:0 PCMU/8000\r\n"
        "a=rtpmap:8 PCMA/8000\r\n"
        "a=sendrecv\r\n",
        (unsigned long)get_time_ms(),
        (unsigned long)get_time_ms(),
        local_ip,
        session_name ? session_name : "Voice Session",
        local_ip,
        rtp_port,
        formats
    );
    
    if (len >= (int)size) return VOICE_ERROR_OVERFLOW;
    
    return VOICE_OK;
}

voice_error_t sip_parse_sdp_rtp(const char *sdp,
                                 char *host, size_t host_size,
                                 uint16_t *port,
                                 uint8_t *codecs, int *codec_count)
{
    if (!sdp) return VOICE_ERROR_INVALID_PARAM;
    
    /* Parse c= line for host */
    if (host && host_size > 0) {
        const char *c_line = strstr(sdp, "c=IN IP4 ");
        if (c_line) {
            c_line += 9;
            const char *end = strchr(c_line, '\r');
            if (!end) end = strchr(c_line, '\n');
            
            size_t len = end ? (size_t)(end - c_line) : strlen(c_line);
            if (len >= host_size) len = host_size - 1;
            strncpy(host, c_line, len);
            host[len] = '\0';
        } else {
            host[0] = '\0';
        }
    }
    
    /* Parse m=audio line for port and codecs */
    const char *m_line = strstr(sdp, "m=audio ");
    if (m_line) {
        m_line += 8;
        
        if (port) {
            *port = (uint16_t)atoi(m_line);
        }
        
        if (codecs && codec_count) {
            *codec_count = 0;
            
            /* Skip past port and RTP/AVP */
            const char *formats = strstr(m_line, "RTP/AVP");
            if (formats) {
                formats += 7;
                
                while (*formats && *formats != '\r' && *formats != '\n') {
                    while (*formats == ' ') formats++;
                    if (*formats && *formats != '\r' && *formats != '\n') {
                        codecs[*codec_count] = (uint8_t)atoi(formats);
                        (*codec_count)++;
                        if (*codec_count >= 16) break;
                        
                        while (*formats && *formats != ' ' && 
                               *formats != '\r' && *formats != '\n') {
                            formats++;
                        }
                    }
                }
            }
        }
    } else {
        if (port) *port = 0;
        if (codec_count) *codec_count = 0;
    }
    
    return VOICE_OK;
}
