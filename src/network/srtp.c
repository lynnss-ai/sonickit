/**
 * @file srtp.c
 * @brief SRTP implementation using libsrtp2
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "network/srtp.h"
#include <string.h>

#ifdef VOICE_HAVE_SRTP

#include <srtp2/srtp.h>
#include <stdlib.h>
#include <string.h>

/* ============================================
 * SRTP 会话结构
 * ============================================ */

struct srtp_session_s {
    srtp_t srtp_ctx;
    srtp_policy_t policy;
    srtp_config_t config;
    bool initialized;
};

/* Global initialization flag */
static bool g_srtp_initialized = false;

/* ============================================
 * 工具函数
 * ============================================ */

static srtp_profile_t libsrtp_profile_to_ours(srtp_profile_t profile)
{
    return profile;
}

static int ours_profile_to_libsrtp(srtp_profile_t profile)
{
    switch (profile) {
    case SRTP_PROFILE_AES128_CM_SHA1_80:
        return srtp_profile_aes128_cm_sha1_80;
    case SRTP_PROFILE_AES128_CM_SHA1_32:
        return srtp_profile_aes128_cm_sha1_32;
    case SRTP_PROFILE_AEAD_AES_128_GCM:
        return srtp_profile_aead_aes_128_gcm;
    case SRTP_PROFILE_AEAD_AES_256_GCM:
        return srtp_profile_aead_aes_256_gcm;
    default:
        return srtp_profile_aes128_cm_sha1_80;
    }
}

/* ============================================
 * SRTP 库初始化
 * ============================================ */

voice_error_t srtp_init(void)
{
    if (g_srtp_initialized) {
        return VOICE_OK;
    }

    srtp_err_status_t status = srtp_init();
    if (status != srtp_err_status_ok) {
        VOICE_LOG_E("Failed to initialize libsrtp: %d", status);
        return VOICE_ERROR_CRYPTO_FAILED;
    }

    g_srtp_initialized = true;
    VOICE_LOG_I("SRTP library initialized");

    return VOICE_OK;
}

void srtp_shutdown(void)
{
    if (g_srtp_initialized) {
        srtp_shutdown();
        g_srtp_initialized = false;
    }
}

/* ============================================
 * SRTP 会话管理
 * ============================================ */

void srtp_config_init(srtp_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(srtp_config_t));
    config->profile = SRTP_PROFILE_AES128_CM_SHA1_80;
    config->master_key_len = SRTP_MASTER_KEY_LEN;
    config->master_salt_len = SRTP_MASTER_SALT_LEN;
    config->replay_window_size = 128;
}

srtp_session_t *srtp_session_create(const srtp_config_t *config)
{
    if (!config) {
        return NULL;
    }

    if (!g_srtp_initialized) {
        if (srtp_init() != VOICE_OK) {
            return NULL;
        }
    }

    srtp_session_t *session = (srtp_session_t *)calloc(1, sizeof(srtp_session_t));
    if (!session) {
        return NULL;
    }

    session->config = *config;

    /* Set policy */
    memset(&session->policy, 0, sizeof(srtp_policy_t));

    /* Select crypto suite from config */
    srtp_crypto_policy_set_from_profile_for_rtp(
        &session->policy.rtp,
        ours_profile_to_libsrtp(config->profile)
    );

    srtp_crypto_policy_set_from_profile_for_rtcp(
        &session->policy.rtcp,
        ours_profile_to_libsrtp(config->profile)
    );

    /* Set key */
    size_t key_len = config->master_key_len + config->master_salt_len;
    uint8_t *key_salt = (uint8_t *)malloc(key_len);
    if (!key_salt) {
        free(session);
        return NULL;
    }

    memcpy(key_salt, config->master_key, config->master_key_len);
    memcpy(key_salt + config->master_key_len,
           config->master_salt, config->master_salt_len);

    session->policy.key = key_salt;
    session->policy.ssrc.type = config->ssrc ? ssrc_specific : ssrc_any_inbound;
    session->policy.ssrc.value = config->ssrc;
    session->policy.window_size = (unsigned long)config->replay_window_size;
    session->policy.allow_repeat_tx = 0;
    session->policy.next = NULL;

    /* Create SRTP context */
    srtp_err_status_t status = srtp_create(&session->srtp_ctx, &session->policy);

    /* No longer need key buffer */
    free(key_salt);
    session->policy.key = NULL;

    if (status != srtp_err_status_ok) {
        VOICE_LOG_E("Failed to create SRTP context: %d", status);
        free(session);
        return NULL;
    }

    session->initialized = true;
    VOICE_LOG_I("SRTP session created: SSRC=0x%08X, profile=%d",
        config->ssrc, config->profile);

    return session;
}

void srtp_session_destroy(srtp_session_t *session)
{
    if (!session) return;

    if (session->initialized && session->srtp_ctx) {
        srtp_dealloc(session->srtp_ctx);
    }

    free(session);
}

/* ============================================
 * SRTP 保护/解保护
 * ============================================ */

voice_error_t srtp_protect(
    srtp_session_t *session,
    uint8_t *rtp_packet,
    size_t *rtp_len,
    size_t max_len)
{
    if (!session || !session->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!rtp_packet || !rtp_len) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* 检查缓冲区是否足够大 */
    size_t auth_tag_len = srtp_get_auth_tag_len(session->config.profile);
    if (*rtp_len + auth_tag_len > max_len) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    int len = (int)*rtp_len;
    srtp_err_status_t status = srtp_protect(session->srtp_ctx, rtp_packet, &len);

    if (status != srtp_err_status_ok) {
        VOICE_LOG_E("SRTP protect failed: %d", status);
        return VOICE_ERROR_CRYPTO_FAILED;
    }

    *rtp_len = (size_t)len;
    return VOICE_OK;
}

voice_error_t srtp_unprotect(
    srtp_session_t *session,
    uint8_t *srtp_packet,
    size_t *srtp_len)
{
    if (!session || !session->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!srtp_packet || !srtp_len) {
        return VOICE_ERROR_NULL_POINTER;
    }

    int len = (int)*srtp_len;
    srtp_err_status_t status = srtp_unprotect(session->srtp_ctx, srtp_packet, &len);

    if (status != srtp_err_status_ok) {
        if (status == srtp_err_status_replay_fail) {
            return VOICE_ERROR_REPLAY_ATTACK;
        }
        if (status == srtp_err_status_auth_fail) {
            return VOICE_ERROR_AUTH_FAILED;
        }
        VOICE_LOG_E("SRTP unprotect failed: %d", status);
        return VOICE_ERROR_CRYPTO_FAILED;
    }

    *srtp_len = (size_t)len;
    return VOICE_OK;
}

voice_error_t srtcp_protect(
    srtp_session_t *session,
    uint8_t *rtcp_packet,
    size_t *rtcp_len,
    size_t max_len)
{
    if (!session || !session->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!rtcp_packet || !rtcp_len) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* SRTCP 需要额外空间: auth tag + index (4 bytes) */
    size_t extra = srtp_get_auth_tag_len(session->config.profile) + 4;
    if (*rtcp_len + extra > max_len) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    int len = (int)*rtcp_len;
    srtp_err_status_t status = srtp_protect_rtcp(session->srtp_ctx, rtcp_packet, &len);

    if (status != srtp_err_status_ok) {
        VOICE_LOG_E("SRTCP protect failed: %d", status);
        return VOICE_ERROR_CRYPTO_FAILED;
    }

    *rtcp_len = (size_t)len;
    return VOICE_OK;
}

voice_error_t srtcp_unprotect(
    srtp_session_t *session,
    uint8_t *srtcp_packet,
    size_t *srtcp_len)
{
    if (!session || !session->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!srtcp_packet || !srtcp_len) {
        return VOICE_ERROR_NULL_POINTER;
    }

    int len = (int)*srtcp_len;
    srtp_err_status_t status = srtp_unprotect_rtcp(session->srtp_ctx, srtcp_packet, &len);

    if (status != srtp_err_status_ok) {
        if (status == srtp_err_status_replay_fail) {
            return VOICE_ERROR_REPLAY_ATTACK;
        }
        if (status == srtp_err_status_auth_fail) {
            return VOICE_ERROR_AUTH_FAILED;
        }
        VOICE_LOG_E("SRTCP unprotect failed: %d", status);
        return VOICE_ERROR_CRYPTO_FAILED;
    }

    *srtcp_len = (size_t)len;
    return VOICE_OK;
}

voice_error_t srtp_session_update_key(
    srtp_session_t *session,
    const uint8_t *master_key,
    size_t key_len,
    const uint8_t *master_salt,
    size_t salt_len)
{
    if (!session || !session->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (!master_key || !master_salt) {
        return VOICE_ERROR_NULL_POINTER;
    }

    /* 更新密钥需要重建会话 */
    srtp_config_t new_config = session->config;
    memcpy(new_config.master_key, master_key, key_len);
    new_config.master_key_len = key_len;
    memcpy(new_config.master_salt, master_salt, salt_len);
    new_config.master_salt_len = salt_len;

    /* 销毁旧上下文 */
    if (session->srtp_ctx) {
        srtp_dealloc(session->srtp_ctx);
        session->srtp_ctx = NULL;
    }

    /* 创建新上下文 */
    size_t total_key_len = key_len + salt_len;
    uint8_t *key_salt = (uint8_t *)malloc(total_key_len);
    if (!key_salt) {
        return VOICE_ERROR_NO_MEMORY;
    }

    memcpy(key_salt, master_key, key_len);
    memcpy(key_salt + key_len, master_salt, salt_len);

    session->policy.key = key_salt;

    srtp_err_status_t status = srtp_create(&session->srtp_ctx, &session->policy);

    free(key_salt);
    session->policy.key = NULL;

    if (status != srtp_err_status_ok) {
        VOICE_LOG_E("Failed to update SRTP key: %d", status);
        return VOICE_ERROR_CRYPTO_FAILED;
    }

    session->config = new_config;
    return VOICE_OK;
}

/* ============================================
 * 工具函数
 * ============================================ */

size_t srtp_get_auth_tag_len(srtp_profile_t profile)
{
    switch (profile) {
    case SRTP_PROFILE_AES128_CM_SHA1_80:
        return 10;
    case SRTP_PROFILE_AES128_CM_SHA1_32:
        return 4;
    case SRTP_PROFILE_AEAD_AES_128_GCM:
    case SRTP_PROFILE_AEAD_AES_256_GCM:
        return 16;
    default:
        return 10;
    }
}

size_t srtp_get_key_len(srtp_profile_t profile)
{
    switch (profile) {
    case SRTP_PROFILE_AES128_CM_SHA1_80:
    case SRTP_PROFILE_AES128_CM_SHA1_32:
    case SRTP_PROFILE_AEAD_AES_128_GCM:
        return 16;
    case SRTP_PROFILE_AEAD_AES_256_GCM:
        return 32;
    default:
        return 16;
    }
}

size_t srtp_get_salt_len(srtp_profile_t profile)
{
    switch (profile) {
    case SRTP_PROFILE_AES128_CM_SHA1_80:
    case SRTP_PROFILE_AES128_CM_SHA1_32:
        return 14;
    case SRTP_PROFILE_AEAD_AES_128_GCM:
    case SRTP_PROFILE_AEAD_AES_256_GCM:
        return 12;
    default:
        return 14;
    }
}

#else /* !VOICE_HAVE_SRTP */

/* 无 SRTP 支持时的空实现 */

voice_error_t srtp_init(void)
{
    VOICE_LOG_W("SRTP support not compiled");
    return VOICE_ERROR_NOT_SUPPORTED;
}

void srtp_shutdown(void) {}

void srtp_config_init(srtp_config_t *config)
{
    if (config) memset(config, 0, sizeof(srtp_config_t));
}

srtp_session_t *srtp_session_create(const srtp_config_t *config)
{
    (void)config;
    return NULL;
}

void srtp_session_destroy(srtp_session_t *session)
{
    (void)session;
}

voice_error_t srtp_protect(
    srtp_session_t *session,
    uint8_t *rtp_packet,
    size_t *rtp_len,
    size_t max_len)
{
    (void)session;
    (void)rtp_packet;
    (void)rtp_len;
    (void)max_len;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t srtp_unprotect(
    srtp_session_t *session,
    uint8_t *srtp_packet,
    size_t *srtp_len)
{
    (void)session;
    (void)srtp_packet;
    (void)srtp_len;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t srtcp_protect(
    srtp_session_t *session,
    uint8_t *rtcp_packet,
    size_t *rtcp_len,
    size_t max_len)
{
    (void)session;
    (void)rtcp_packet;
    (void)rtcp_len;
    (void)max_len;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t srtcp_unprotect(
    srtp_session_t *session,
    uint8_t *srtcp_packet,
    size_t *srtcp_len)
{
    (void)session;
    (void)srtcp_packet;
    (void)srtcp_len;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t srtp_session_update_key(
    srtp_session_t *session,
    const uint8_t *master_key,
    size_t key_len,
    const uint8_t *master_salt,
    size_t salt_len)
{
    (void)session;
    (void)master_key;
    (void)key_len;
    (void)master_salt;
    (void)salt_len;
    return VOICE_ERROR_NOT_SUPPORTED;
}

size_t srtp_get_auth_tag_len(srtp_profile_t profile)
{
    (void)profile;
    return 0;
}

size_t srtp_get_key_len(srtp_profile_t profile)
{
    (void)profile;
    return 0;
}

size_t srtp_get_salt_len(srtp_profile_t profile)
{
    (void)profile;
    return 0;
}

#endif /* VOICE_HAVE_SRTP */
