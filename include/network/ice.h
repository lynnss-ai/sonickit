/**
 * @file ice.h
 * @brief ICE (Interactive Connectivity Establishment) support
 * 
 * ICE/STUN/TURN 支持，用于 NAT 穿透
 * 基于 RFC 5245 (ICE), RFC 5389 (STUN), RFC 5766 (TURN)
 */

#ifndef VOICE_ICE_H
#define VOICE_ICE_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 类型定义
 * ============================================ */

typedef struct voice_ice_agent_s voice_ice_agent_t;
typedef struct voice_stun_client_s voice_stun_client_t;

/** ICE 候选类型 */
typedef enum {
    VOICE_ICE_CANDIDATE_HOST,       /**< 本地地址 */
    VOICE_ICE_CANDIDATE_SRFLX,      /**< 服务器反射地址 (STUN) */
    VOICE_ICE_CANDIDATE_PRFLX,      /**< 对等反射地址 */
    VOICE_ICE_CANDIDATE_RELAY,      /**< 中继地址 (TURN) */
} voice_ice_candidate_type_t;

/** ICE 连接状态 */
typedef enum {
    VOICE_ICE_STATE_NEW,
    VOICE_ICE_STATE_CHECKING,
    VOICE_ICE_STATE_CONNECTED,
    VOICE_ICE_STATE_COMPLETED,
    VOICE_ICE_STATE_FAILED,
    VOICE_ICE_STATE_DISCONNECTED,
    VOICE_ICE_STATE_CLOSED,
} voice_ice_state_t;

/** ICE 角色 */
typedef enum {
    VOICE_ICE_ROLE_CONTROLLING,
    VOICE_ICE_ROLE_CONTROLLED,
} voice_ice_role_t;

/** ICE 模式 */
typedef enum {
    VOICE_ICE_FULL,                 /**< 完整 ICE */
    VOICE_ICE_LITE,                 /**< ICE Lite */
} voice_ice_mode_t;

/* ============================================
 * 地址结构
 * ============================================ */

typedef struct {
    uint8_t family;                 /**< AF_INET 或 AF_INET6 */
    uint16_t port;
    union {
        uint8_t ipv4[4];
        uint8_t ipv6[16];
    } addr;
} voice_network_addr_t;

/* ============================================
 * ICE 候选
 * ============================================ */

typedef struct {
    char foundation[33];            /**< 基础 ID */
    uint32_t component_id;          /**< 组件 ID (1=RTP, 2=RTCP) */
    char transport[8];              /**< "udp" 或 "tcp" */
    uint32_t priority;              /**< 优先级 */
    voice_network_addr_t address;   /**< 地址 */
    voice_ice_candidate_type_t type;/**< 候选类型 */
    voice_network_addr_t related;   /**< 相关地址 (SRFLX/RELAY) */
    char ufrag[256];                /**< 用户片段 */
    char pwd[256];                  /**< 密码 */
} voice_ice_candidate_t;

/* ============================================
 * STUN 配置
 * ============================================ */

typedef struct {
    char server[256];               /**< STUN 服务器地址 */
    uint16_t port;                  /**< STUN 服务器端口 (默认 3478) */
    uint32_t timeout_ms;            /**< 超时时间 */
    uint32_t retries;               /**< 重试次数 */
} voice_stun_config_t;

/* ============================================
 * TURN 配置
 * ============================================ */

typedef struct {
    char server[256];               /**< TURN 服务器地址 */
    uint16_t port;                  /**< TURN 服务器端口 (默认 3478) */
    char username[256];             /**< 用户名 */
    char password[256];             /**< 密码 */
    char realm[256];                /**< 域 */
    uint32_t lifetime;              /**< 分配生命周期 (秒) */
    bool use_tls;                   /**< 使用 TLS (TURNS) */
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
 * @brief 初始化默认配置
 */
void voice_ice_config_init(voice_ice_config_t *config);

/**
 * @brief 创建 ICE Agent
 */
voice_ice_agent_t *voice_ice_agent_create(const voice_ice_config_t *config);

/**
 * @brief 销毁 ICE Agent
 */
void voice_ice_agent_destroy(voice_ice_agent_t *agent);

/**
 * @brief 开始收集候选
 */
voice_error_t voice_ice_gather_candidates(voice_ice_agent_t *agent);

/**
 * @brief 获取本地候选列表
 */
voice_error_t voice_ice_get_local_candidates(
    voice_ice_agent_t *agent,
    voice_ice_candidate_t *candidates,
    size_t *count
);

/**
 * @brief 添加远端候选
 */
voice_error_t voice_ice_add_remote_candidate(
    voice_ice_agent_t *agent,
    const voice_ice_candidate_t *candidate
);

/**
 * @brief 设置远端凭据
 */
voice_error_t voice_ice_set_remote_credentials(
    voice_ice_agent_t *agent,
    const char *ufrag,
    const char *pwd
);

/**
 * @brief 获取本地凭据
 */
voice_error_t voice_ice_get_local_credentials(
    voice_ice_agent_t *agent,
    char *ufrag,
    size_t ufrag_size,
    char *pwd,
    size_t pwd_size
);

/**
 * @brief 开始连接检查
 */
voice_error_t voice_ice_start_checks(voice_ice_agent_t *agent);

/**
 * @brief 获取当前状态
 */
voice_ice_state_t voice_ice_get_state(voice_ice_agent_t *agent);

/**
 * @brief 发送数据 (通过选定的候选对)
 */
voice_error_t voice_ice_send(
    voice_ice_agent_t *agent,
    uint32_t component_id,
    const uint8_t *data,
    size_t size
);

/**
 * @brief 处理收到的数据
 */
voice_error_t voice_ice_process_incoming(
    voice_ice_agent_t *agent,
    const uint8_t *data,
    size_t size,
    const voice_network_addr_t *from
);

/**
 * @brief 关闭 ICE Agent
 */
void voice_ice_close(voice_ice_agent_t *agent);

/* ============================================
 * STUN Client API
 * ============================================ */

/**
 * @brief 创建 STUN 客户端
 */
voice_stun_client_t *voice_stun_client_create(const voice_stun_config_t *config);

/**
 * @brief 销毁 STUN 客户端
 */
void voice_stun_client_destroy(voice_stun_client_t *client);

/**
 * @brief 绑定请求 (获取公网地址)
 */
voice_error_t voice_stun_binding_request(
    voice_stun_client_t *client,
    voice_network_addr_t *mapped_addr
);

/* ============================================
 * SDP 工具函数
 * ============================================ */

/**
 * @brief 将候选序列化为 SDP 属性
 */
size_t voice_ice_candidate_to_sdp(
    const voice_ice_candidate_t *candidate,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief 从 SDP 属性解析候选
 */
voice_error_t voice_ice_candidate_from_sdp(
    const char *sdp_line,
    voice_ice_candidate_t *candidate
);

#ifdef __cplusplus
}
#endif

#endif /* VOICE_ICE_H */
