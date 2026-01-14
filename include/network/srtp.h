/**
 * @file srtp.h
 * @brief SRTP encryption interface
 * 
 * Secure Real-time Transport Protocol (RFC 3711)
 * DTLS-SRTP key exchange (RFC 5764)
 */

#ifndef NETWORK_SRTP_H
#define NETWORK_SRTP_H

#include "voice/types.h"
#include "voice/error.h"
#include "network/rtp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * SRTP 常量
 * ============================================ */

#define SRTP_MASTER_KEY_LEN     16
#define SRTP_MASTER_SALT_LEN    14
#define SRTP_MAX_AUTH_TAG_LEN   16
#define SRTP_MAX_TRAILER_LEN    (SRTP_MAX_AUTH_TAG_LEN + 4)

/* ============================================
 * 加密套件
 * ============================================ */

typedef enum {
    SRTP_PROFILE_AES128_CM_SHA1_80 = 1,  /**< AES-CM 128位密钥, HMAC-SHA1 80位标签 */
    SRTP_PROFILE_AES128_CM_SHA1_32 = 2,  /**< AES-CM 128位密钥, HMAC-SHA1 32位标签 */
    SRTP_PROFILE_AEAD_AES_128_GCM  = 7,  /**< AES-GCM 128位 */
    SRTP_PROFILE_AEAD_AES_256_GCM  = 8,  /**< AES-GCM 256位 */
} srtp_profile_t;

/* ============================================
 * SRTP 句柄
 * ============================================ */

typedef struct srtp_session_s srtp_session_t;

/** SRTP 配置 */
typedef struct {
    srtp_profile_t profile;              /**< 加密配置 */
    uint8_t master_key[32];              /**< 主密钥 */
    size_t master_key_len;               /**< 主密钥长度 */
    uint8_t master_salt[14];             /**< 主盐值 */
    size_t master_salt_len;              /**< 主盐值长度 */
    uint32_t ssrc;                       /**< SSRC */
    bool is_sender;                      /**< 发送方/接收方 */
    uint64_t replay_window_size;         /**< 重放保护窗口大小 */
} srtp_config_t;

/** SRTP 密钥材料 */
typedef struct {
    uint8_t client_write_key[32];
    size_t client_write_key_len;
    uint8_t client_write_salt[14];
    size_t client_write_salt_len;
    uint8_t server_write_key[32];
    size_t server_write_key_len;
    uint8_t server_write_salt[14];
    size_t server_write_salt_len;
    srtp_profile_t profile;
} srtp_keying_material_t;

/* ============================================
 * SRTP API
 * ============================================ */

/**
 * @brief 初始化 SRTP 库
 * @return 错误码
 */
voice_error_t srtp_init(void);

/**
 * @brief 清理 SRTP 库
 */
void srtp_shutdown(void);

/**
 * @brief 初始化默认配置
 */
void srtp_config_init(srtp_config_t *config);

/**
 * @brief 创建 SRTP 会话
 * @param config 配置
 * @return SRTP会话句柄
 */
srtp_session_t *srtp_session_create(const srtp_config_t *config);

/**
 * @brief 销毁 SRTP 会话
 */
void srtp_session_destroy(srtp_session_t *session);

/**
 * @brief 保护 RTP 包 (加密+认证)
 * 
 * @param session SRTP会话
 * @param rtp_packet RTP包数据 (原地加密)
 * @param rtp_len RTP包长度(输入)/SRTP包长度(输出)
 * @param max_len 缓冲区最大长度
 * @return 错误码
 */
voice_error_t srtp_protect(
    srtp_session_t *session,
    uint8_t *rtp_packet,
    size_t *rtp_len,
    size_t max_len
);

/**
 * @brief 解保护 SRTP 包 (验证+解密)
 * 
 * @param session SRTP会话
 * @param srtp_packet SRTP包数据 (原地解密)
 * @param srtp_len SRTP包长度(输入)/RTP包长度(输出)
 * @return 错误码
 */
voice_error_t srtp_unprotect(
    srtp_session_t *session,
    uint8_t *srtp_packet,
    size_t *srtp_len
);

/**
 * @brief 保护 RTCP 包
 */
voice_error_t srtcp_protect(
    srtp_session_t *session,
    uint8_t *rtcp_packet,
    size_t *rtcp_len,
    size_t max_len
);

/**
 * @brief 解保护 SRTCP 包
 */
voice_error_t srtcp_unprotect(
    srtp_session_t *session,
    uint8_t *srtcp_packet,
    size_t *srtcp_len
);

/**
 * @brief 更新 SRTP 密钥
 */
voice_error_t srtp_session_update_key(
    srtp_session_t *session,
    const uint8_t *master_key,
    size_t key_len,
    const uint8_t *master_salt,
    size_t salt_len
);

/**
 * @brief 获取认证标签长度
 */
size_t srtp_get_auth_tag_len(srtp_profile_t profile);

/**
 * @brief 获取密钥长度
 */
size_t srtp_get_key_len(srtp_profile_t profile);

/**
 * @brief 获取盐值长度
 */
size_t srtp_get_salt_len(srtp_profile_t profile);

/* ============================================
 * DTLS-SRTP API
 * ============================================ */

#ifdef VOICE_HAVE_DTLS

typedef struct dtls_srtp_session_s dtls_srtp_session_t;

/** DTLS 角色 */
typedef enum {
    DTLS_ROLE_CLIENT,
    DTLS_ROLE_SERVER,
    DTLS_ROLE_AUTO,
} dtls_role_t;

/** DTLS-SRTP 配置 */
typedef struct {
    dtls_role_t role;                    /**< 角色 */
    const char *certificate_file;        /**< 证书文件路径 */
    const char *private_key_file;        /**< 私钥文件路径 */
    srtp_profile_t profiles[4];          /**< 支持的SRTP配置 */
    size_t profile_count;                /**< 配置数量 */
    int mtu;                             /**< MTU */
} dtls_srtp_config_t;

/** DTLS 事件类型 */
typedef enum {
    DTLS_EVENT_CONNECTED,       /**< 握手完成 */
    DTLS_EVENT_ERROR,           /**< 发生错误 */
    DTLS_EVENT_CLOSED,          /**< 连接关闭 */
    DTLS_EVENT_KEYS_READY,      /**< 密钥材料就绪 */
} dtls_event_t;

/** DTLS 事件回调 */
typedef void (*dtls_event_callback_t)(
    dtls_srtp_session_t *session,
    dtls_event_t event,
    void *user_data
);

/** DTLS 数据发送回调 */
typedef int (*dtls_send_callback_t)(
    const uint8_t *data,
    size_t len,
    void *user_data
);

/**
 * @brief 初始化默认配置
 */
void dtls_srtp_config_init(dtls_srtp_config_t *config);

/**
 * @brief 创建 DTLS-SRTP 会话
 */
dtls_srtp_session_t *dtls_srtp_create(const dtls_srtp_config_t *config);

/**
 * @brief 销毁 DTLS-SRTP 会话
 */
void dtls_srtp_destroy(dtls_srtp_session_t *session);

/**
 * @brief 设置发送回调
 */
void dtls_srtp_set_send_callback(
    dtls_srtp_session_t *session,
    dtls_send_callback_t callback,
    void *user_data
);

/**
 * @brief 设置事件回调
 */
void dtls_srtp_set_event_callback(
    dtls_srtp_session_t *session,
    dtls_event_callback_t callback,
    void *user_data
);

/**
 * @brief 开始握手
 */
voice_error_t dtls_srtp_start_handshake(dtls_srtp_session_t *session);

/**
 * @brief 处理接收的 DTLS 数据
 */
voice_error_t dtls_srtp_handle_incoming(
    dtls_srtp_session_t *session,
    const uint8_t *data,
    size_t len
);

/**
 * @brief 检查是否已连接
 */
bool dtls_srtp_is_connected(dtls_srtp_session_t *session);

/**
 * @brief 获取协商的 SRTP 配置
 */
srtp_profile_t dtls_srtp_get_profile(dtls_srtp_session_t *session);

/**
 * @brief 获取 SRTP 密钥材料
 */
voice_error_t dtls_srtp_get_keys(
    dtls_srtp_session_t *session,
    srtp_keying_material_t *keys
);

/**
 * @brief 从 DTLS 会话创建 SRTP 会话
 */
srtp_session_t *dtls_srtp_create_srtp_session(
    dtls_srtp_session_t *dtls,
    bool is_sender,
    uint32_t ssrc
);

/**
 * @brief 获取本地指纹
 */
voice_error_t dtls_srtp_get_fingerprint(
    dtls_srtp_session_t *session,
    char *fingerprint,
    size_t max_len
);

/**
 * @brief 设置远端指纹 (用于验证)
 */
voice_error_t dtls_srtp_set_remote_fingerprint(
    dtls_srtp_session_t *session,
    const char *fingerprint
);

#endif /* VOICE_HAVE_DTLS */

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_SRTP_H */
