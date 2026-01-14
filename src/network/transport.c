/**
 * @file transport.c
 * @brief Network transport layer implementation
 */

#include "network/transport.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

typedef SOCKET socket_t;
#define INVALID_SOCK INVALID_SOCKET
#define SOCK_ERROR SOCKET_ERROR
#define close_socket closesocket
#define sock_errno WSAGetLastError()

#else /* POSIX */
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef int socket_t;
#define INVALID_SOCK (-1)
#define SOCK_ERROR (-1)
#define close_socket close
#define sock_errno errno
#endif

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_transport_s {
    voice_transport_config_t config;
    socket_t socket;
    bool connected;
    
    voice_net_address_t local_addr;
    voice_net_address_t remote_addr;
    
    voice_transport_stats_t stats;
};

/* ============================================
 * 初始化
 * ============================================ */

#ifdef _WIN32
static bool wsa_initialized = false;

static void init_wsa(void) {
    if (!wsa_initialized) {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        wsa_initialized = true;
    }
}
#endif

/* ============================================
 * 配置
 * ============================================ */

void voice_transport_config_init(voice_transport_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_transport_config_t));
    
    config->type = VOICE_TRANSPORT_UDP;
    config->family = VOICE_AF_INET;
    config->local_port = 0;
    
    config->recv_buffer_size = 65536;
    config->send_buffer_size = 65536;
    config->tos = 0;
    config->reuse_addr = true;
    config->non_blocking = true;
    
    config->recv_timeout_ms = 0;
    config->send_timeout_ms = 0;
}

/* ============================================
 * 生命周期
 * ============================================ */

voice_transport_t *voice_transport_create(const voice_transport_config_t *config) {
#ifdef _WIN32
    init_wsa();
#endif
    
    voice_transport_t *transport = (voice_transport_t *)calloc(1, sizeof(voice_transport_t));
    if (!transport) return NULL;
    
    if (config) {
        transport->config = *config;
    } else {
        voice_transport_config_init(&transport->config);
    }
    
    /* 创建 socket */
    int family = (transport->config.family == VOICE_AF_INET6) ? AF_INET6 : AF_INET;
    int type = (transport->config.type == VOICE_TRANSPORT_UDP || 
                transport->config.type == VOICE_TRANSPORT_DTLS) ? 
               SOCK_DGRAM : SOCK_STREAM;
    int protocol = (type == SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP;
    
    transport->socket = socket(family, type, protocol);
    if (transport->socket == INVALID_SOCK) {
        free(transport);
        return NULL;
    }
    
    /* 设置选项 */
    if (transport->config.reuse_addr) {
        int opt = 1;
        setsockopt(transport->socket, SOL_SOCKET, SO_REUSEADDR, 
                   (const char *)&opt, sizeof(opt));
    }
    
    if (transport->config.recv_buffer_size > 0) {
        int size = (int)transport->config.recv_buffer_size;
        setsockopt(transport->socket, SOL_SOCKET, SO_RCVBUF,
                   (const char *)&size, sizeof(size));
    }
    
    if (transport->config.send_buffer_size > 0) {
        int size = (int)transport->config.send_buffer_size;
        setsockopt(transport->socket, SOL_SOCKET, SO_SNDBUF,
                   (const char *)&size, sizeof(size));
    }
    
    /* 非阻塞模式 */
    if (transport->config.non_blocking) {
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(transport->socket, FIONBIO, &mode);
#else
        int flags = fcntl(transport->socket, F_GETFL, 0);
        fcntl(transport->socket, F_SETFL, flags | O_NONBLOCK);
#endif
    }
    
    /* 设置超时 */
    if (transport->config.recv_timeout_ms > 0) {
#ifdef _WIN32
        DWORD timeout = transport->config.recv_timeout_ms;
        setsockopt(transport->socket, SOL_SOCKET, SO_RCVTIMEO,
                   (const char *)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = transport->config.recv_timeout_ms / 1000;
        tv.tv_usec = (transport->config.recv_timeout_ms % 1000) * 1000;
        setsockopt(transport->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    }
    
    /* QoS */
    if (transport->config.tos > 0) {
        setsockopt(transport->socket, IPPROTO_IP, IP_TOS,
                   (const char *)&transport->config.tos, sizeof(int));
    }
    
    return transport;
}

void voice_transport_destroy(voice_transport_t *transport) {
    if (!transport) return;
    
    if (transport->socket != INVALID_SOCK) {
        close_socket(transport->socket);
    }
    
    free(transport);
}

/* ============================================
 * 连接管理
 * ============================================ */

voice_error_t voice_transport_bind(
    voice_transport_t *transport,
    const char *address,
    uint16_t port
) {
    if (!transport) return VOICE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (address && address[0]) {
        inet_pton(AF_INET, address, &addr.sin_addr);
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    if (bind(transport->socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCK_ERROR) {
        return VOICE_ERROR_NETWORK;
    }
    
    /* 获取实际绑定的地址 */
    socklen_t len = sizeof(addr);
    getsockname(transport->socket, (struct sockaddr *)&addr, &len);
    
    transport->local_addr.family = VOICE_AF_INET;
    inet_ntop(AF_INET, &addr.sin_addr, transport->local_addr.address,
              sizeof(transport->local_addr.address));
    transport->local_addr.port = ntohs(addr.sin_port);
    
    return VOICE_SUCCESS;
}

voice_error_t voice_transport_connect(
    voice_transport_t *transport,
    const char *address,
    uint16_t port
) {
    if (!transport || !address) return VOICE_ERROR_INVALID_PARAM;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1) {
        /* 尝试 DNS 解析 */
        struct addrinfo hints = {0}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%u", port);
        
        if (getaddrinfo(address, port_str, &hints, &res) != 0) {
            return VOICE_ERROR_NETWORK;
        }
        
        memcpy(&addr, res->ai_addr, sizeof(addr));
        freeaddrinfo(res);
    }
    
    if (connect(transport->socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCK_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
#else
        if (errno != EINPROGRESS)
#endif
        {
            return VOICE_ERROR_NETWORK;
        }
    }
    
    transport->connected = true;
    
    transport->remote_addr.family = VOICE_AF_INET;
    strncpy(transport->remote_addr.address, address, 
            sizeof(transport->remote_addr.address) - 1);
    transport->remote_addr.port = port;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_transport_close(voice_transport_t *transport) {
    if (!transport) return VOICE_ERROR_INVALID_PARAM;
    
    if (transport->socket != INVALID_SOCK) {
        close_socket(transport->socket);
        transport->socket = INVALID_SOCK;
    }
    
    transport->connected = false;
    
    return VOICE_SUCCESS;
}

/* ============================================
 * 数据传输
 * ============================================ */

ssize_t voice_transport_send(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size
) {
    if (!transport || !data) return -1;
    
    ssize_t sent = send(transport->socket, (const char *)data, (int)size, 0);
    
    if (sent > 0) {
        transport->stats.bytes_sent += sent;
        transport->stats.packets_sent++;
    } else if (sent < 0) {
        transport->stats.send_errors++;
    }
    
    return sent;
}

ssize_t voice_transport_sendto(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size,
    const voice_net_address_t *to
) {
    if (!transport || !data || !to) return -1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(to->port);
    inet_pton(AF_INET, to->address, &addr.sin_addr);
    
    ssize_t sent = sendto(transport->socket, (const char *)data, (int)size, 0,
                          (struct sockaddr *)&addr, sizeof(addr));
    
    if (sent > 0) {
        transport->stats.bytes_sent += sent;
        transport->stats.packets_sent++;
    } else if (sent < 0) {
        transport->stats.send_errors++;
    }
    
    return sent;
}

ssize_t voice_transport_recv(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size
) {
    if (!transport || !buffer) return -1;
    
    ssize_t received = recv(transport->socket, (char *)buffer, (int)buffer_size, 0);
    
    if (received > 0) {
        transport->stats.bytes_received += received;
        transport->stats.packets_received++;
    } else if (received < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
        {
            transport->stats.recv_errors++;
        }
    }
    
    return received;
}

ssize_t voice_transport_recvfrom(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size,
    voice_net_address_t *from
) {
    if (!transport || !buffer) return -1;
    
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    
    ssize_t received = recvfrom(transport->socket, (char *)buffer, (int)buffer_size, 0,
                                (struct sockaddr *)&addr, &addr_len);
    
    if (received > 0) {
        transport->stats.bytes_received += received;
        transport->stats.packets_received++;
        
        if (from) {
            from->family = VOICE_AF_INET;
            from->port = ntohs(addr.sin_port);
            inet_ntop(AF_INET, &addr.sin_addr, from->address, sizeof(from->address));
        }
    } else if (received < 0) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
#else
        if (errno != EAGAIN && errno != EWOULDBLOCK)
#endif
        {
            transport->stats.recv_errors++;
        }
    }
    
    return received;
}

/* ============================================
 * 事件处理
 * ============================================ */

int voice_transport_poll(
    voice_transport_t *transport,
    int timeout_ms
) {
    if (!transport || transport->socket == INVALID_SOCK) return -1;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(transport->socket, &readfds);
    
    struct timeval tv, *ptv = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        ptv = &tv;
    }
    
    int result = select((int)transport->socket + 1, &readfds, NULL, NULL, ptv);
    
    if (result > 0 && transport->config.on_receive) {
        uint8_t buffer[2048];
        voice_net_address_t from;
        
        ssize_t received = voice_transport_recvfrom(transport, buffer, sizeof(buffer), &from);
        if (received > 0) {
            transport->config.on_receive(buffer, received, &from,
                                        transport->config.callback_user_data);
        }
    }
    
    return result;
}

bool voice_transport_readable(voice_transport_t *transport) {
    if (!transport || transport->socket == INVALID_SOCK) return false;
    
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(transport->socket, &readfds);
    
    struct timeval tv = {0, 0};
    
    return select((int)transport->socket + 1, &readfds, NULL, NULL, &tv) > 0;
}

bool voice_transport_writable(voice_transport_t *transport) {
    if (!transport || transport->socket == INVALID_SOCK) return false;
    
    fd_set writefds;
    FD_ZERO(&writefds);
    FD_SET(transport->socket, &writefds);
    
    struct timeval tv = {0, 0};
    
    return select((int)transport->socket + 1, NULL, &writefds, NULL, &tv) > 0;
}

/* ============================================
 * 状态查询
 * ============================================ */

voice_error_t voice_transport_get_local_address(
    voice_transport_t *transport,
    voice_net_address_t *address
) {
    if (!transport || !address) return VOICE_ERROR_INVALID_PARAM;
    
    *address = transport->local_addr;
    return VOICE_SUCCESS;
}

voice_error_t voice_transport_get_remote_address(
    voice_transport_t *transport,
    voice_net_address_t *address
) {
    if (!transport || !address) return VOICE_ERROR_INVALID_PARAM;
    
    *address = transport->remote_addr;
    return VOICE_SUCCESS;
}

voice_error_t voice_transport_get_stats(
    voice_transport_t *transport,
    voice_transport_stats_t *stats
) {
    if (!transport || !stats) return VOICE_ERROR_INVALID_PARAM;
    
    *stats = transport->stats;
    return VOICE_SUCCESS;
}

void voice_transport_reset_stats(voice_transport_t *transport) {
    if (transport) {
        memset(&transport->stats, 0, sizeof(voice_transport_stats_t));
    }
}

voice_error_t voice_transport_set_qos(
    voice_transport_t *transport,
    int tos
) {
    if (!transport) return VOICE_ERROR_INVALID_PARAM;
    
    if (setsockopt(transport->socket, IPPROTO_IP, IP_TOS,
                   (const char *)&tos, sizeof(tos)) == SOCK_ERROR) {
        return VOICE_ERROR_NETWORK;
    }
    
    transport->config.tos = tos;
    return VOICE_SUCCESS;
}

int voice_transport_get_fd(voice_transport_t *transport) {
    if (!transport) return -1;
    return (int)transport->socket;
}

/* ============================================
 * 地址辅助函数
 * ============================================ */

voice_error_t voice_net_address_parse(
    voice_net_address_t *addr,
    const char *address_str
) {
    if (!addr || !address_str) return VOICE_ERROR_INVALID_PARAM;
    
    memset(addr, 0, sizeof(voice_net_address_t));
    addr->family = VOICE_AF_INET;
    
    /* 尝试解析 "address:port" 格式 */
    char *colon = strrchr(address_str, ':');
    if (colon && strchr(address_str, '.')) {
        /* IPv4 with port */
        size_t addr_len = colon - address_str;
        if (addr_len >= sizeof(addr->address)) {
            addr_len = sizeof(addr->address) - 1;
        }
        strncpy(addr->address, address_str, addr_len);
        addr->port = (uint16_t)atoi(colon + 1);
    } else {
        strncpy(addr->address, address_str, sizeof(addr->address) - 1);
        addr->port = 0;
    }
    
    return VOICE_SUCCESS;
}

size_t voice_net_address_format(
    const voice_net_address_t *addr,
    char *buffer,
    size_t buffer_size
) {
    if (!addr || !buffer || buffer_size == 0) return 0;
    
    int written = snprintf(buffer, buffer_size, "%s:%u", 
                           addr->address, addr->port);
    
    return (size_t)(written > 0 ? written : 0);
}

bool voice_net_address_equal(
    const voice_net_address_t *a,
    const voice_net_address_t *b
) {
    if (!a || !b) return false;
    
    return a->family == b->family &&
           a->port == b->port &&
           strcmp(a->address, b->address) == 0;
}
