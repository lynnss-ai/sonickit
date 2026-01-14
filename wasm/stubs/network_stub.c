/**
 * @file network_stub.c
 * @brief WebAssembly 网络功能桩实现
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * WebAssembly 不支持直接的 Socket 编程
 * 网络功能应在 JavaScript 层通过 WebSocket、WebRTC Data Channel 或 Fetch API 实现
 * 此文件提供基本的桩实现
 */

#include "network/transport.h"
#include "voice/error.h"
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__

/* ============================================
 * Transport 配置初始化
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
 * Transport 对象（桩实现）
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

    transport->config = *config;
    transport->connected = false;

    return transport;
}

void voice_transport_destroy(voice_transport_t *transport) {
    if (transport) {
        free(transport);
    }
}

voice_error_t voice_transport_bind(
    voice_transport_t *transport,
    const char *address,
    uint16_t port)
{
    (void)address;
    (void)port;

    if (!transport) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* WebAssembly: 不支持直接绑定 */
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_transport_connect(
    voice_transport_t *transport,
    const char *address,
    uint16_t port)
{
    (void)address;
    (void)port;

    if (!transport) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* WebAssembly: 不支持直接连接 */
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_transport_close(voice_transport_t *transport) {
    if (!transport) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    transport->connected = false;
    return VOICE_SUCCESS;
}

ssize_t voice_transport_send(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size)
{
    (void)data;
    (void)size;

    if (!transport) {
        return -1;
    }

    /* WebAssembly: 不支持直接发送 */
    return -1;
}

ssize_t voice_transport_sendto(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size,
    const voice_net_address_t *to)
{
    (void)data;
    (void)size;
    (void)to;

    if (!transport) {
        return -1;
    }

    /* WebAssembly: 不支持直接发送 */
    return -1;
}

ssize_t voice_transport_recv(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size)
{
    (void)buffer;
    (void)buffer_size;

    if (!transport) {
        return -1;
    }

    /* WebAssembly: 不支持直接接收 */
    return -1;
}

ssize_t voice_transport_recvfrom(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size,
    voice_net_address_t *from)
{
    (void)buffer;
    (void)buffer_size;
    (void)from;

    if (!transport) {
        return -1;
    }

    /* WebAssembly: 不支持直接接收 */
    return -1;
}

int voice_transport_poll(voice_transport_t *transport, int timeout_ms) {
    (void)transport;
    (void)timeout_ms;
    return 0;
}

bool voice_transport_readable(voice_transport_t *transport) {
    (void)transport;
    return false;
}

bool voice_transport_writable(voice_transport_t *transport) {
    (void)transport;
    return false;
}

bool voice_transport_is_connected(const voice_transport_t *transport) {
    return transport ? transport->connected : false;
}

voice_error_t voice_transport_get_local_address(
    voice_transport_t *transport,
    voice_net_address_t *address)
{
    (void)address;

    if (!transport) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_transport_get_remote_address(
    voice_transport_t *transport,
    voice_net_address_t *address)
{
    (void)address;

    if (!transport) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    return VOICE_ERROR_NOT_SUPPORTED;
}

/* ============================================
 * 地址工具函数
 * ============================================ */

void voice_net_address_init(voice_net_address_t *addr) {
    if (addr) {
        memset(addr, 0, sizeof(voice_net_address_t));
        addr->family = VOICE_AF_INET;
    }
}

voice_error_t voice_net_address_set(
    voice_net_address_t *addr,
    const char *host,
    uint16_t port)
{
    if (!addr || !host) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    strncpy(addr->address, host, sizeof(addr->address) - 1);
    addr->port = port;

    return VOICE_SUCCESS;
}

#endif /* __EMSCRIPTEN__ */
