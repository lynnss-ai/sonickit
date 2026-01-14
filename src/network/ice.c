/**
 * @file ice.c
 * @brief ICE (Interactive Connectivity Establishment) implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Stub implementation for ICE/STUN/TURN support.
 */

#include "network/ice.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

/* ============================================
 * 内部结构
 * ============================================ */

#define MAX_CANDIDATES 16

struct voice_ice_agent_s {
    voice_ice_config_t config;
    voice_ice_state_t state;

    /* 本地候选 */
    voice_ice_candidate_t local_candidates[MAX_CANDIDATES];
    size_t local_candidate_count;

    /* 远端候选 */
    voice_ice_candidate_t remote_candidates[MAX_CANDIDATES];
    size_t remote_candidate_count;

    /* 凭据 */
    char local_ufrag[256];
    char local_pwd[256];
    char remote_ufrag[256];
    char remote_pwd[256];

    /* 选定的候选对 */
    voice_ice_candidate_t *selected_local;
    voice_ice_candidate_t *selected_remote;

    /* Socket */
    int socket_fd;
};

struct voice_stun_client_s {
    voice_stun_config_t config;
    int socket_fd;
};

/* ============================================
 * 辅助函数
 * ============================================ */

static void generate_random_string(char *buffer, size_t length) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    static int seeded = 0;

    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    for (size_t i = 0; i < length - 1; i++) {
        buffer[i] = chars[rand() % (sizeof(chars) - 1)];
    }
    buffer[length - 1] = '\0';
}

static uint32_t calculate_priority(voice_ice_candidate_type_t type,
                                   uint32_t component_id, uint32_t local_pref) {
    uint32_t type_pref;
    switch (type) {
        case VOICE_ICE_CANDIDATE_HOST:  type_pref = 126; break;
        case VOICE_ICE_CANDIDATE_SRFLX: type_pref = 100; break;
        case VOICE_ICE_CANDIDATE_PRFLX: type_pref = 110; break;
        case VOICE_ICE_CANDIDATE_RELAY: type_pref = 0;   break;
        default: type_pref = 0;
    }

    return (type_pref << 24) | (local_pref << 8) | (256 - component_id);
}

static void addr_to_string(const voice_network_addr_t *addr, char *buffer, size_t size) {
    if (addr->family == 2) {  /* AF_INET */
        snprintf(buffer, size, "%u.%u.%u.%u",
                addr->addr.ipv4[0], addr->addr.ipv4[1],
                addr->addr.ipv4[2], addr->addr.ipv4[3]);
    } else {
        snprintf(buffer, size, "::1");  /* Stub for IPv6 */
    }
}

/* ============================================
 * ICE Config API
 * ============================================ */

void voice_ice_config_init(voice_ice_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->mode = VOICE_ICE_FULL;
    config->initial_role = VOICE_ICE_ROLE_CONTROLLING;
    config->stun_server_count = 0;
    config->turn_server_count = 0;
    config->connectivity_check_timeout_ms = 5000;
    config->nomination_timeout_ms = 3000;
    config->gather_host_candidates = true;
    config->gather_srflx_candidates = true;
    config->gather_relay_candidates = false;
    config->on_candidate = NULL;
    config->on_state_change = NULL;
    config->on_selected_pair = NULL;
    config->callback_user_data = NULL;
}

/* ============================================
 * ICE Agent API
 * ============================================ */

voice_ice_agent_t *voice_ice_agent_create(const voice_ice_config_t *config) {
    voice_ice_agent_t *agent = (voice_ice_agent_t *)calloc(1, sizeof(voice_ice_agent_t));
    if (!agent) return NULL;

    if (config) {
        agent->config = *config;
    } else {
        voice_ice_config_init(&agent->config);
    }

    agent->state = VOICE_ICE_STATE_NEW;
    agent->local_candidate_count = 0;
    agent->remote_candidate_count = 0;
    agent->socket_fd = -1;

    /* 生成本地凭据 */
    generate_random_string(agent->local_ufrag, 8);
    generate_random_string(agent->local_pwd, 24);

    return agent;
}

void voice_ice_agent_destroy(voice_ice_agent_t *agent) {
    if (!agent) return;

    if (agent->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(agent->socket_fd);
#else
        close(agent->socket_fd);
#endif
    }

    free(agent);
}

voice_error_t voice_ice_gather_candidates(voice_ice_agent_t *agent) {
    if (!agent) return VOICE_ERROR_NULL_POINTER;

    agent->state = VOICE_ICE_STATE_CHECKING;

    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }

    /* 添加模拟的本地候选 */
    if (agent->config.gather_host_candidates &&
        agent->local_candidate_count < MAX_CANDIDATES) {

        voice_ice_candidate_t *cand = &agent->local_candidates[agent->local_candidate_count];
        memset(cand, 0, sizeof(*cand));

        snprintf(cand->foundation, sizeof(cand->foundation), "1");
        cand->component_id = 1;
        strncpy(cand->transport, "udp", sizeof(cand->transport) - 1);
        cand->type = VOICE_ICE_CANDIDATE_HOST;
        cand->priority = calculate_priority(VOICE_ICE_CANDIDATE_HOST, 1, 65535);

        /* 模拟本地地址 */
        cand->address.family = 2;  /* AF_INET */
        cand->address.port = 0;    /* 由系统分配 */
        cand->address.addr.ipv4[0] = 192;
        cand->address.addr.ipv4[1] = 168;
        cand->address.addr.ipv4[2] = 1;
        cand->address.addr.ipv4[3] = 100;

        strncpy(cand->ufrag, agent->local_ufrag, sizeof(cand->ufrag) - 1);
        strncpy(cand->pwd, agent->local_pwd, sizeof(cand->pwd) - 1);

        agent->local_candidate_count++;

        if (agent->config.on_candidate) {
            agent->config.on_candidate(cand, agent->config.callback_user_data);
        }
    }

    agent->state = VOICE_ICE_STATE_COMPLETED;

    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }

    return VOICE_OK;
}

voice_error_t voice_ice_get_local_candidates(
    voice_ice_agent_t *agent,
    voice_ice_candidate_t *candidates,
    size_t *count
) {
    if (!agent || !count) return VOICE_ERROR_NULL_POINTER;

    size_t max_count = *count;
    *count = agent->local_candidate_count;

    if (candidates && max_count > 0) {
        size_t copy_count = (agent->local_candidate_count < max_count) ?
                           agent->local_candidate_count : max_count;
        memcpy(candidates, agent->local_candidates,
               copy_count * sizeof(voice_ice_candidate_t));
    }

    return VOICE_OK;
}

voice_error_t voice_ice_add_remote_candidate(
    voice_ice_agent_t *agent,
    const voice_ice_candidate_t *candidate
) {
    if (!agent || !candidate) return VOICE_ERROR_NULL_POINTER;

    if (agent->remote_candidate_count >= MAX_CANDIDATES) {
        return VOICE_ERROR_OUT_OF_MEMORY;
    }

    agent->remote_candidates[agent->remote_candidate_count++] = *candidate;

    return VOICE_OK;
}

voice_error_t voice_ice_set_remote_credentials(
    voice_ice_agent_t *agent,
    const char *ufrag,
    const char *pwd
) {
    if (!agent || !ufrag || !pwd) return VOICE_ERROR_NULL_POINTER;

    strncpy(agent->remote_ufrag, ufrag, sizeof(agent->remote_ufrag) - 1);
    strncpy(agent->remote_pwd, pwd, sizeof(agent->remote_pwd) - 1);

    return VOICE_OK;
}

voice_error_t voice_ice_get_local_credentials(
    voice_ice_agent_t *agent,
    char *ufrag,
    size_t ufrag_size,
    char *pwd,
    size_t pwd_size
) {
    if (!agent) return VOICE_ERROR_NULL_POINTER;

    if (ufrag && ufrag_size > 0) {
        strncpy(ufrag, agent->local_ufrag, ufrag_size - 1);
        ufrag[ufrag_size - 1] = '\0';
    }

    if (pwd && pwd_size > 0) {
        strncpy(pwd, agent->local_pwd, pwd_size - 1);
        pwd[pwd_size - 1] = '\0';
    }

    return VOICE_OK;
}

voice_error_t voice_ice_start_checks(voice_ice_agent_t *agent) {
    if (!agent) return VOICE_ERROR_NULL_POINTER;

    agent->state = VOICE_ICE_STATE_CHECKING;

    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }

    /* Stub: 立即转为连接状态 */
    if (agent->local_candidate_count > 0 && agent->remote_candidate_count > 0) {
        agent->state = VOICE_ICE_STATE_CONNECTED;
        agent->selected_local = &agent->local_candidates[0];
        agent->selected_remote = &agent->remote_candidates[0];

        if (agent->config.on_selected_pair) {
            agent->config.on_selected_pair(agent->selected_local,
                                          agent->selected_remote,
                                          agent->config.callback_user_data);
        }
    } else {
        agent->state = VOICE_ICE_STATE_FAILED;
    }

    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }

    return VOICE_OK;
}

voice_ice_state_t voice_ice_get_state(voice_ice_agent_t *agent) {
    if (!agent) return VOICE_ICE_STATE_FAILED;

    return agent->state;
}

voice_error_t voice_ice_send(
    voice_ice_agent_t *agent,
    uint32_t component_id,
    const uint8_t *data,
    size_t size
) {
    if (!agent || !data) return VOICE_ERROR_NULL_POINTER;

    (void)component_id;
    (void)size;

    if (agent->state != VOICE_ICE_STATE_CONNECTED &&
        agent->state != VOICE_ICE_STATE_COMPLETED) {
        return VOICE_ERROR_INVALID_STATE;
    }

    /* Stub: 不实际发送 */
    return VOICE_OK;
}

voice_error_t voice_ice_process_incoming(
    voice_ice_agent_t *agent,
    const uint8_t *data,
    size_t size,
    const voice_network_addr_t *from
) {
    if (!agent || !data || !from) return VOICE_ERROR_NULL_POINTER;

    (void)size;

    /* Stub: 不处理 */
    return VOICE_OK;
}

void voice_ice_close(voice_ice_agent_t *agent) {
    if (!agent) return;

    agent->state = VOICE_ICE_STATE_CLOSED;

    if (agent->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(agent->socket_fd);
#else
        close(agent->socket_fd);
#endif
        agent->socket_fd = -1;
    }

    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }
}

/* ============================================
 * STUN Client API
 * ============================================ */

voice_stun_client_t *voice_stun_client_create(const voice_stun_config_t *config) {
    voice_stun_client_t *client = (voice_stun_client_t *)calloc(1, sizeof(voice_stun_client_t));
    if (!client) return NULL;

    if (config) {
        client->config = *config;
    } else {
        memset(&client->config, 0, sizeof(client->config));
        strncpy(client->config.server, "stun.l.google.com", sizeof(client->config.server) - 1);
        client->config.port = 19302;
        client->config.timeout_ms = 3000;
        client->config.retries = 3;
    }

    client->socket_fd = -1;

    return client;
}

void voice_stun_client_destroy(voice_stun_client_t *client) {
    if (!client) return;

    if (client->socket_fd >= 0) {
#ifdef _WIN32
        closesocket(client->socket_fd);
#else
        close(client->socket_fd);
#endif
    }

    free(client);
}

voice_error_t voice_stun_binding_request(
    voice_stun_client_t *client,
    voice_network_addr_t *mapped_addr
) {
    if (!client || !mapped_addr) return VOICE_ERROR_NULL_POINTER;

    /* Stub: 返回模拟的映射地址 */
    memset(mapped_addr, 0, sizeof(*mapped_addr));
    mapped_addr->family = 2;  /* AF_INET */
    mapped_addr->port = 12345;
    mapped_addr->addr.ipv4[0] = 203;
    mapped_addr->addr.ipv4[1] = 0;
    mapped_addr->addr.ipv4[2] = 113;
    mapped_addr->addr.ipv4[3] = 1;

    return VOICE_OK;
}

/* ============================================
 * SDP 工具函数
 * ============================================ */

size_t voice_ice_candidate_to_sdp(
    const voice_ice_candidate_t *candidate,
    char *buffer,
    size_t buffer_size
) {
    if (!candidate || !buffer || buffer_size == 0) return 0;

    const char *type_str;
    switch (candidate->type) {
        case VOICE_ICE_CANDIDATE_HOST:  type_str = "host";  break;
        case VOICE_ICE_CANDIDATE_SRFLX: type_str = "srflx"; break;
        case VOICE_ICE_CANDIDATE_PRFLX: type_str = "prflx"; break;
        case VOICE_ICE_CANDIDATE_RELAY: type_str = "relay"; break;
        default: type_str = "host";
    }

    char addr_str[64];
    addr_to_string(&candidate->address, addr_str, sizeof(addr_str));

    int written = snprintf(buffer, buffer_size,
        "a=candidate:%s %u %s %u %s %u typ %s",
        candidate->foundation,
        candidate->component_id,
        candidate->transport,
        candidate->priority,
        addr_str,
        candidate->address.port,
        type_str
    );

    /* 添加相关地址 (如果有) */
    if (candidate->type != VOICE_ICE_CANDIDATE_HOST &&
        candidate->related.port > 0) {
        char related_str[64];
        addr_to_string(&candidate->related, related_str, sizeof(related_str));

        written += snprintf(buffer + written, buffer_size - written,
            " raddr %s rport %u",
            related_str, candidate->related.port);
    }

    return (size_t)written;
}

voice_error_t voice_ice_candidate_from_sdp(
    const char *sdp_line,
    voice_ice_candidate_t *candidate
) {
    if (!sdp_line || !candidate) return VOICE_ERROR_NULL_POINTER;

    memset(candidate, 0, sizeof(*candidate));

    char addr_str[64] = {0};
    char type_str[16] = {0};
    uint32_t port = 0;

    int parsed = sscanf(sdp_line,
        "a=candidate:%32s %u %7s %u %63s %u typ %15s",
        candidate->foundation,
        &candidate->component_id,
        candidate->transport,
        &candidate->priority,
        addr_str,
        &port,
        type_str
    );

    if (parsed < 7) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* 解析地址 */
    candidate->address.family = 2;  /* AF_INET */
    candidate->address.port = (uint16_t)port;

    int a, b, c, d;
    if (sscanf(addr_str, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        candidate->address.addr.ipv4[0] = (uint8_t)a;
        candidate->address.addr.ipv4[1] = (uint8_t)b;
        candidate->address.addr.ipv4[2] = (uint8_t)c;
        candidate->address.addr.ipv4[3] = (uint8_t)d;
    }

    /* 解析类型 */
    if (strcmp(type_str, "host") == 0) {
        candidate->type = VOICE_ICE_CANDIDATE_HOST;
    } else if (strcmp(type_str, "srflx") == 0) {
        candidate->type = VOICE_ICE_CANDIDATE_SRFLX;
    } else if (strcmp(type_str, "prflx") == 0) {
        candidate->type = VOICE_ICE_CANDIDATE_PRFLX;
    } else if (strcmp(type_str, "relay") == 0) {
        candidate->type = VOICE_ICE_CANDIDATE_RELAY;
    } else {
        candidate->type = VOICE_ICE_CANDIDATE_HOST;
    }

    /* 解析相关地址 (如果有) */
    const char *raddr = strstr(sdp_line, "raddr ");
    const char *rport = strstr(sdp_line, "rport ");

    if (raddr && rport) {
        char related_addr[64] = {0};
        uint32_t related_port = 0;

        sscanf(raddr, "raddr %63s", related_addr);
        sscanf(rport, "rport %u", &related_port);

        candidate->related.family = 2;
        candidate->related.port = (uint16_t)related_port;

        if (sscanf(related_addr, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
            candidate->related.addr.ipv4[0] = (uint8_t)a;
            candidate->related.addr.ipv4[1] = (uint8_t)b;
            candidate->related.addr.ipv4[2] = (uint8_t)c;
            candidate->related.addr.ipv4[3] = (uint8_t)d;
        }
    }

    return VOICE_OK;
}
