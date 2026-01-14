/**
 * @file network_stub.c
 * @brief WebAssembly 网络功能桩实现（可选）
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * WebAssembly 不支持直接的 Socket 编程
 * 网络功能应通过 WebSocket、WebRTC Data Channel 或 Fetch API 在 JavaScript 层实现
 * 此文件提供基本的桩实现供需要网络功能的模块使用
 */

#include "network/transport.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__

/* ============================================
 * 传输层配置（桩）
 * ============================================ */

void voice_transport_config_init(voice_transport_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_transport_config_t));
    config->type = VOICE_TRANSPORT_UDP;
    config->family = VOICE_AF_INET;
    config->local_port = 0;
    config->recv_buffer_size = 65536;
    config->send_buffer_size = 65536;
    config->reuse_addr = true;
    config->non_blocking = true;
}

/* ============================================
 * 传输层对象（桩）
 * ============================================ */

struct voice_transport_s {
    voice_transport_config_t config;
    bool connected;
};

voice_transport_t *voice_transport_create(const voice_transport_config_t *config) {
    if (!config) {
        return NULL;
    }
    
    voice_transport_t *transport = (voice_transport_t *)calloc(1, sizeof(voice_transport_t));
    if (!transport) {
        return NULL;
    }
    
    memcpy(&transport->config, config, sizeof(voice_transport_config_t));
    transport->connected = false;
    
    return transport;
}

void voice_transport_destroy(voice_transport_t *transport) {
    if (transport) {
        free(transport);
    }
}

/* ============================================
 * 连接管理（桩）
 * ============================================ */

voice_error_t voice_transport_bind(voice_transport_t *transport, const voice_net_address_t *addr) {
    if (!transport || !addr) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* WebAssembly 不支持直接绑定端口 */
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_transport_connect(voice_transport_t *transport, const voice_net_address_t *addr) {
    if (!transport || !addr) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* WebSocket/WebRTC 连接应在 JavaScript 层实现 */
    transport->connected = true;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_transport_disconnect(voice_transport_t *transport) {
    if (!transport) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    transport->connected = false;
    return VOICE_ERROR_NONE;
}

bool voice_transport_is_connected(const voice_transport_t *transport) {
    return transport && transport->connected;
}

/* ============================================
 * 数据传输（桩）
 * ============================================ */

int voice_transport_send(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size,
    const voice_net_address_t *addr
) {
    if (!transport || !data) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* 数据传输应通过 JavaScript 桥接 */
    (void)size;
    (void)addr;
    return VOICE_ERROR_NOT_SUPPORTED;
}

int voice_transport_recv(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size,
    voice_net_address_t *addr
) {
    if (!transport || !buffer) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    /* 数据接收应通过 JavaScript 桥接 */
    (void)buffer_size;
    (void)addr;
    return VOICE_ERROR_NOT_SUPPORTED;
}

/* ============================================
 * 地址工具（可以保留）
 * ============================================ */

voice_error_t voice_net_address_init(
    voice_net_address_t *addr,
    voice_address_family_t family,
    const char *ip,
    uint16_t port
) {
    if (!addr || !ip) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    memset(addr, 0, sizeof(voice_net_address_t));
    addr->family = family;
    addr->port = port;
    strncpy(addr->ip, ip, sizeof(addr->ip) - 1);
    
    return VOICE_ERROR_NONE;
}

voice_error_t voice_net_address_to_string(const voice_net_address_t *addr, char *buffer, size_t size) {
    if (!addr || !buffer || size < 22) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    snprintf(buffer, size, "%s:%u", addr->ip, addr->port);
    return VOICE_ERROR_NONE;
}

bool voice_net_address_equal(const voice_net_address_t *a, const voice_net_address_t *b) {
    if (!a || !b) {
        return false;
    }
    
    return (a->family == b->family) &&
           (a->port == b->port) &&
           (strcmp(a->ip, b->ip) == 0);
}

/* ============================================
 * 统计信息（桩）
 * ============================================ */

voice_error_t voice_transport_get_stats(
    const voice_transport_t *transport,
    voice_transport_stats_t *stats
) {
    if (!transport || !stats) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    memset(stats, 0, sizeof(voice_transport_stats_t));
    return VOICE_ERROR_NONE;
}

void voice_transport_reset_stats(voice_transport_t *transport) {
    (void)transport;
    /* 空操作 */
}

/* ============================================
 * 选项设置（桩）
 * ============================================ */

voice_error_t voice_transport_set_option(
    voice_transport_t *transport,
    voice_transport_option_t option,
    const void *value,
    size_t size
) {
    if (!transport || !value) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    (void)option;
    (void)size;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_transport_get_option(
    const voice_transport_t *transport,
    voice_transport_option_t option,
    void *value,
    size_t *size
) {
    if (!transport || !value || !size) {
        return VOICE_ERROR_INVALID_ARGUMENT;
    }
    
    (void)option;
    return VOICE_ERROR_NOT_SUPPORTED;
}

#endif /* __EMSCRIPTEN__ */
