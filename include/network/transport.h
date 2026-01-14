/**
 * @file transport.h
 * @brief Network transport layer abstraction
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * 提供网络传输层抽象，封装 UDP/TCP socket 操作
 */

#ifndef VOICE_TRANSPORT_H
#define VOICE_TRANSPORT_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ssize_t 兼容性定义 */
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
 * 类型定义
 * ============================================ */

/**
 * @brief 传输类型
 */
typedef enum {
    VOICE_TRANSPORT_UDP,        /**< UDP 传输 */
    VOICE_TRANSPORT_TCP,        /**< TCP 传输 */
    VOICE_TRANSPORT_TLS,        /**< TLS/TCP 传输 */
    VOICE_TRANSPORT_DTLS        /**< DTLS/UDP 传输 */
} voice_transport_type_t;

/**
 * @brief 地址族
 */
typedef enum {
    VOICE_AF_INET,              /**< IPv4 */
    VOICE_AF_INET6              /**< IPv6 */
} voice_address_family_t;

/**
 * @brief 网络地址
 */
typedef struct {
    voice_address_family_t family;
    char address[64];           /**< 地址字符串 */
    uint16_t port;
} voice_net_address_t;

/**
 * @brief 传输配置
 */
typedef struct {
    voice_transport_type_t type;    /**< 传输类型 */
    voice_address_family_t family;  /**< 地址族 */

    /* 本地绑定 */
    char local_address[64];         /**< 本地地址 (空=任意) */
    uint16_t local_port;            /**< 本地端口 (0=自动) */

    /* Socket 选项 */
    uint32_t recv_buffer_size;      /**< 接收缓冲区大小 */
    uint32_t send_buffer_size;      /**< 发送缓冲区大小 */
    int tos;                        /**< ToS/DSCP 值 */
    bool reuse_addr;                /**< 地址重用 */
    bool non_blocking;              /**< 非阻塞模式 */

    /* 超时 */
    uint32_t recv_timeout_ms;       /**< 接收超时 (ms) */
    uint32_t send_timeout_ms;       /**< 发送超时 (ms) */

    /* 回调 */
    void (*on_receive)(const uint8_t *data, size_t size,
                       const voice_net_address_t *from, void *user_data);
    void (*on_error)(voice_error_t error, const char *message, void *user_data);
    void *callback_user_data;
} voice_transport_config_t;

/**
 * @brief 传输统计
 */
typedef struct {
    uint64_t bytes_sent;            /**< 发送字节数 */
    uint64_t bytes_received;        /**< 接收字节数 */
    uint64_t packets_sent;          /**< 发送包数 */
    uint64_t packets_received;      /**< 接收包数 */
    uint64_t send_errors;           /**< 发送错误数 */
    uint64_t recv_errors;           /**< 接收错误数 */
    uint32_t last_rtt_us;           /**< 最近 RTT (微秒) */
} voice_transport_stats_t;

/* ============================================
 * 前向声明
 * ============================================ */

typedef struct voice_transport_s voice_transport_t;

/* ============================================
 * 配置初始化
 * ============================================ */

/**
 * @brief 初始化传输配置 (UDP)
 */
void voice_transport_config_init(voice_transport_config_t *config);

/* ============================================
 * 生命周期
 * ============================================ */

/**
 * @brief 创建传输实例
 */
voice_transport_t *voice_transport_create(const voice_transport_config_t *config);

/**
 * @brief 销毁传输实例
 */
void voice_transport_destroy(voice_transport_t *transport);

/* ============================================
 * 连接管理
 * ============================================ */

/**
 * @brief 绑定到本地地址
 */
voice_error_t voice_transport_bind(
    voice_transport_t *transport,
    const char *address,
    uint16_t port
);

/**
 * @brief 连接到远端 (UDP: 设置默认目标)
 */
voice_error_t voice_transport_connect(
    voice_transport_t *transport,
    const char *address,
    uint16_t port
);

/**
 * @brief 关闭传输
 */
voice_error_t voice_transport_close(voice_transport_t *transport);

/* ============================================
 * 数据传输
 * ============================================ */

/**
 * @brief 发送数据到已连接的目标
 */
ssize_t voice_transport_send(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size
);

/**
 * @brief 发送数据到指定地址 (UDP)
 */
ssize_t voice_transport_sendto(
    voice_transport_t *transport,
    const uint8_t *data,
    size_t size,
    const voice_net_address_t *to
);

/**
 * @brief 接收数据
 */
ssize_t voice_transport_recv(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size
);

/**
 * @brief 接收数据并获取源地址 (UDP)
 */
ssize_t voice_transport_recvfrom(
    voice_transport_t *transport,
    uint8_t *buffer,
    size_t buffer_size,
    voice_net_address_t *from
);

/* ============================================
 * 事件处理
 * ============================================ */

/**
 * @brief 处理挂起的 IO 事件
 * @param timeout_ms 超时时间 (ms)，0=立即返回，-1=无限等待
 * @return 处理的事件数，0=超时，<0=错误
 */
int voice_transport_poll(
    voice_transport_t *transport,
    int timeout_ms
);

/**
 * @brief 检查是否可读
 */
bool voice_transport_readable(voice_transport_t *transport);

/**
 * @brief 检查是否可写
 */
bool voice_transport_writable(voice_transport_t *transport);

/* ============================================
 * 状态查询
 * ============================================ */

/**
 * @brief 获取本地地址
 */
voice_error_t voice_transport_get_local_address(
    voice_transport_t *transport,
    voice_net_address_t *address
);

/**
 * @brief 获取远端地址
 */
voice_error_t voice_transport_get_remote_address(
    voice_transport_t *transport,
    voice_net_address_t *address
);

/**
 * @brief 获取统计信息
 */
voice_error_t voice_transport_get_stats(
    voice_transport_t *transport,
    voice_transport_stats_t *stats
);

/**
 * @brief 重置统计
 */
void voice_transport_reset_stats(voice_transport_t *transport);

/* ============================================
 * Socket 选项
 * ============================================ */

/**
 * @brief 设置 QoS (ToS/DSCP)
 */
voice_error_t voice_transport_set_qos(
    voice_transport_t *transport,
    int tos
);

/**
 * @brief 获取底层 socket 描述符
 */
int voice_transport_get_fd(voice_transport_t *transport);

/* ============================================
 * 地址辅助函数
 * ============================================ */

/**
 * @brief 解析地址字符串
 */
voice_error_t voice_net_address_parse(
    voice_net_address_t *addr,
    const char *address_str
);

/**
 * @brief 格式化地址为字符串
 */
size_t voice_net_address_format(
    const voice_net_address_t *addr,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief 比较地址
 */
bool voice_net_address_equal(
    const voice_net_address_t *a,
    const voice_net_address_t *b
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_TRANSPORT_H */
