/**
 * @file ice.c
 * @brief ICE/STUN/TURN implementation (simplified)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 实现基本的 STUN 绑定和 ICE 候选收集
 * 完整的 ICE 实现需要更复杂的状态机
 */

#include "network/ice.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <ifaddrs.h>
#endif

/* ============================================
 * STUN 消息结构 (RFC 5389)
 * ============================================ */

#define STUN_MAGIC_COOKIE 0x2112A442
#define STUN_BINDING_REQUEST 0x0001
#define STUN_BINDING_RESPONSE 0x0101
#define STUN_BINDING_ERROR 0x0111

#define STUN_ATTR_MAPPED_ADDRESS 0x0001
#define STUN_ATTR_XOR_MAPPED_ADDRESS 0x0020
#define STUN_ATTR_USERNAME 0x0006
#define STUN_ATTR_MESSAGE_INTEGRITY 0x0008
#define STUN_ATTR_FINGERPRINT 0x8028
#define STUN_ATTR_SOFTWARE 0x8022

#pragma pack(push, 1)
typedef struct {
    uint16_t type;
    uint16_t length;
    uint32_t magic;
    uint8_t transaction_id[12];
} stun_header_t;

typedef struct {
    uint16_t type;
    uint16_t length;
} stun_attr_header_t;
#pragma pack(pop)

/* ============================================
 * 内部结构
 * ============================================ */

#define MAX_CANDIDATES 16
#define MAX_PAIRS 64

struct voice_ice_agent_s {
    voice_ice_config_t config;
    voice_ice_state_t state;
    
    /* 候选列表 */
    voice_ice_candidate_t local_candidates[MAX_CANDIDATES];
    size_t local_count;
    
    voice_ice_candidate_t remote_candidates[MAX_CANDIDATES];
    size_t remote_count;
    
    /* 选中的候选对 */
    voice_ice_candidate_t *selected_local;
    voice_ice_candidate_t *selected_remote;
    
    /* Socket */
    int socket_fd;
    
    /* 凭据 */
    char local_ufrag[32];
    char local_pwd[64];
    char remote_ufrag[32];
    char remote_pwd[64];
};

struct voice_stun_client_s {
    voice_stun_config_t config;
    int socket_fd;
    uint8_t transaction_id[12];
};

struct voice_turn_client_s {
    voice_turn_config_t config;
    int socket_fd;
    bool allocated;
    voice_ice_candidate_t relay_candidate;
};

/* ============================================
 * 辅助函数
 * ============================================ */

static void generate_random_bytes(uint8_t *buf, size_t len) {
    static uint32_t seed = 12345;
    for (size_t i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = (uint8_t)(seed >> 16);
    }
}

static void generate_ufrag(char *buf, size_t len) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < len - 1; i++) {
        uint8_t r;
        generate_random_bytes(&r, 1);
        buf[i] = chars[r % 64];
    }
    buf[len - 1] = '\0';
}

static uint16_t swap16(uint16_t val) {
    return (val << 8) | (val >> 8);
}

static uint32_t swap32(uint32_t val) {
    return ((val & 0xFF) << 24) | 
           ((val & 0xFF00) << 8) | 
           ((val & 0xFF0000) >> 8) | 
           ((val & 0xFF000000) >> 24);
}

/* ============================================
 * ICE 配置
 * ============================================ */

void voice_ice_config_init(voice_ice_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_ice_config_t));
    
    config->mode = VOICE_ICE_FULL;
    config->enable_ipv6 = false;
    
    config->stun_server_count = 0;
    config->turn_server_count = 0;
    
    config->timeout_ms = 5000;
    config->keep_alive_interval = 25000;
}

/* ============================================
 * ICE Agent 实现
 * ============================================ */

voice_ice_agent_t *voice_ice_agent_create(const voice_ice_config_t *config) {
    voice_ice_agent_t *agent = (voice_ice_agent_t *)calloc(1, sizeof(voice_ice_agent_t));
    if (!agent) return NULL;
    
    if (config) {
        agent->config = *config;
    } else {
        voice_ice_config_init(&agent->config);
    }
    
    agent->state = VOICE_ICE_NEW;
    agent->socket_fd = -1;
    
    /* 生成本地凭据 */
    generate_ufrag(agent->local_ufrag, 8);
    generate_ufrag(agent->local_pwd, 24);
    
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
    if (!agent) return VOICE_ERROR_INVALID_PARAM;
    
    agent->state = VOICE_ICE_GATHERING;
    agent->local_count = 0;
    
#ifdef _WIN32
    /* Windows: 使用 GetAdaptersAddresses */
    ULONG flags = GAA_FLAG_INCLUDE_PREFIX;
    ULONG family = AF_INET;
    ULONG buf_len = 15000;
    PIP_ADAPTER_ADDRESSES addresses = NULL;
    
    addresses = (IP_ADAPTER_ADDRESSES *)malloc(buf_len);
    if (!addresses) return VOICE_ERROR_NO_MEMORY;
    
    if (GetAdaptersAddresses(family, flags, NULL, addresses, &buf_len) == NO_ERROR) {
        PIP_ADAPTER_ADDRESSES curr = addresses;
        while (curr && agent->local_count < MAX_CANDIDATES) {
            PIP_ADAPTER_UNICAST_ADDRESS unicast = curr->FirstUnicastAddress;
            while (unicast && agent->local_count < MAX_CANDIDATES) {
                struct sockaddr_in *addr = (struct sockaddr_in *)unicast->Address.lpSockaddr;
                if (addr->sin_family == AF_INET) {
                    voice_ice_candidate_t *cand = &agent->local_candidates[agent->local_count];
                    
                    cand->type = VOICE_ICE_HOST;
                    cand->component = 1;
                    cand->priority = (126 << 24) | (65535 << 8) | (256 - agent->local_count);
                    
                    inet_ntop(AF_INET, &addr->sin_addr, cand->address, sizeof(cand->address));
                    cand->port = 0;  /* 由系统分配 */
                    
                    snprintf(cand->foundation, sizeof(cand->foundation), "host%zu", agent->local_count);
                    
                    agent->local_count++;
                }
                unicast = unicast->Next;
            }
            curr = curr->Next;
        }
    }
    
    free(addresses);
#else
    /* Unix: 使用 getifaddrs */
    struct ifaddrs *ifaddr, *ifa;
    
    if (getifaddrs(&ifaddr) == 0) {
        for (ifa = ifaddr; ifa && agent->local_count < MAX_CANDIDATES; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;
            
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            
            /* 跳过回环地址 */
            if (ntohl(addr->sin_addr.s_addr) == INADDR_LOOPBACK) continue;
            
            voice_ice_candidate_t *cand = &agent->local_candidates[agent->local_count];
            
            cand->type = VOICE_ICE_HOST;
            cand->component = 1;
            cand->priority = (126 << 24) | (65535 << 8) | (256 - agent->local_count);
            
            inet_ntop(AF_INET, &addr->sin_addr, cand->address, sizeof(cand->address));
            cand->port = 0;
            
            snprintf(cand->foundation, sizeof(cand->foundation), "host%zu", agent->local_count);
            
            agent->local_count++;
        }
        
        freeifaddrs(ifaddr);
    }
#endif
    
    agent->state = VOICE_ICE_COMPLETE;
    
    /* 回调 */
    if (agent->config.on_candidate) {
        for (size_t i = 0; i < agent->local_count; i++) {
            agent->config.on_candidate(&agent->local_candidates[i],
                                      agent->config.callback_user_data);
        }
    }
    
    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }
    
    return VOICE_SUCCESS;
}

voice_error_t voice_ice_add_remote_candidate(
    voice_ice_agent_t *agent,
    const voice_ice_candidate_t *candidate
) {
    if (!agent || !candidate) return VOICE_ERROR_INVALID_PARAM;
    
    if (agent->remote_count >= MAX_CANDIDATES) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }
    
    agent->remote_candidates[agent->remote_count++] = *candidate;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_ice_start(voice_ice_agent_t *agent) {
    if (!agent) return VOICE_ERROR_INVALID_PARAM;
    
    if (agent->local_count == 0 || agent->remote_count == 0) {
        return VOICE_ERROR_INVALID_STATE;
    }
    
    agent->state = VOICE_ICE_CHECKING;
    
    /* 简化实现: 选择第一对 */
    agent->selected_local = &agent->local_candidates[0];
    agent->selected_remote = &agent->remote_candidates[0];
    
    agent->state = VOICE_ICE_CONNECTED;
    
    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }
    
    return VOICE_SUCCESS;
}

voice_error_t voice_ice_stop(voice_ice_agent_t *agent) {
    if (!agent) return VOICE_ERROR_INVALID_PARAM;
    
    agent->state = VOICE_ICE_DISCONNECTED;
    
    if (agent->config.on_state_change) {
        agent->config.on_state_change(agent->state, agent->config.callback_user_data);
    }
    
    return VOICE_SUCCESS;
}

voice_ice_state_t voice_ice_get_state(voice_ice_agent_t *agent) {
    if (!agent) return VOICE_ICE_FAILED;
    return agent->state;
}

size_t voice_ice_get_local_candidates(
    voice_ice_agent_t *agent,
    voice_ice_candidate_t *candidates,
    size_t max_count
) {
    if (!agent || !candidates) return 0;
    
    size_t count = agent->local_count;
    if (count > max_count) count = max_count;
    
    memcpy(candidates, agent->local_candidates, count * sizeof(voice_ice_candidate_t));
    return count;
}

voice_error_t voice_ice_get_selected_pair(
    voice_ice_agent_t *agent,
    voice_ice_candidate_t *local,
    voice_ice_candidate_t *remote
) {
    if (!agent) return VOICE_ERROR_INVALID_PARAM;
    
    if (!agent->selected_local || !agent->selected_remote) {
        return VOICE_ERROR_NOT_READY;
    }
    
    if (local) *local = *agent->selected_local;
    if (remote) *remote = *agent->selected_remote;
    
    return VOICE_SUCCESS;
}

voice_error_t voice_ice_set_credentials(
    voice_ice_agent_t *agent,
    const char *remote_ufrag,
    const char *remote_pwd
) {
    if (!agent || !remote_ufrag || !remote_pwd) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    strncpy(agent->remote_ufrag, remote_ufrag, sizeof(agent->remote_ufrag) - 1);
    strncpy(agent->remote_pwd, remote_pwd, sizeof(agent->remote_pwd) - 1);
    
    return VOICE_SUCCESS;
}

voice_error_t voice_ice_get_credentials(
    voice_ice_agent_t *agent,
    char *ufrag,
    size_t ufrag_size,
    char *pwd,
    size_t pwd_size
) {
    if (!agent || !ufrag || !pwd) {
        return VOICE_ERROR_INVALID_PARAM;
    }
    
    strncpy(ufrag, agent->local_ufrag, ufrag_size - 1);
    strncpy(pwd, agent->local_pwd, pwd_size - 1);
    
    return VOICE_SUCCESS;
}

/* ============================================
 * STUN 客户端
 * ============================================ */

void voice_stun_config_init(voice_stun_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_stun_config_t));
    
    strcpy(config->server, "stun.l.google.com");
    config->port = 19302;
    config->timeout_ms = 3000;
    config->retries = 3;
}

voice_stun_client_t *voice_stun_client_create(const voice_stun_config_t *config) {
    voice_stun_client_t *client = (voice_stun_client_t *)calloc(1, sizeof(voice_stun_client_t));
    if (!client) return NULL;
    
    if (config) {
        client->config = *config;
    } else {
        voice_stun_config_init(&client->config);
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
    voice_ice_candidate_t *result
) {
    if (!client || !result) return VOICE_ERROR_INVALID_PARAM;
    
    /* 创建 socket */
    client->socket_fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (client->socket_fd < 0) {
        return VOICE_ERROR_NETWORK;
    }
    
    /* 设置超时 */
#ifdef _WIN32
    DWORD timeout = client->config.timeout_ms;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, 
               (const char *)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = client->config.timeout_ms / 1000;
    tv.tv_usec = (client->config.timeout_ms % 1000) * 1000;
    setsockopt(client->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
    
    /* 解析服务器地址 */
    struct addrinfo hints = {0}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%u", client->config.port);
    
    if (getaddrinfo(client->config.server, port_str, &hints, &res) != 0) {
#ifdef _WIN32
        closesocket(client->socket_fd);
#else
        close(client->socket_fd);
#endif
        client->socket_fd = -1;
        return VOICE_ERROR_NETWORK;
    }
    
    /* 构建 STUN 请求 */
    uint8_t request[20];
    stun_header_t *hdr = (stun_header_t *)request;
    
    hdr->type = swap16(STUN_BINDING_REQUEST);
    hdr->length = 0;
    hdr->magic = swap32(STUN_MAGIC_COOKIE);
    generate_random_bytes(hdr->transaction_id, 12);
    memcpy(client->transaction_id, hdr->transaction_id, 12);
    
    /* 发送请求 */
    ssize_t sent = sendto(client->socket_fd, (const char *)request, sizeof(request), 0,
                          res->ai_addr, (int)res->ai_addrlen);
    freeaddrinfo(res);
    
    if (sent != sizeof(request)) {
#ifdef _WIN32
        closesocket(client->socket_fd);
#else
        close(client->socket_fd);
#endif
        client->socket_fd = -1;
        return VOICE_ERROR_NETWORK;
    }
    
    /* 接收响应 */
    uint8_t response[512];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    
    ssize_t received = recvfrom(client->socket_fd, (char *)response, sizeof(response), 0,
                                (struct sockaddr *)&from, &from_len);
    
#ifdef _WIN32
    closesocket(client->socket_fd);
#else
    close(client->socket_fd);
#endif
    client->socket_fd = -1;
    
    if (received < (ssize_t)sizeof(stun_header_t)) {
        return VOICE_ERROR_TIMEOUT;
    }
    
    /* 解析响应 */
    stun_header_t *resp_hdr = (stun_header_t *)response;
    
    if (swap16(resp_hdr->type) != STUN_BINDING_RESPONSE) {
        return VOICE_ERROR_PROTOCOL;
    }
    
    if (memcmp(resp_hdr->transaction_id, client->transaction_id, 12) != 0) {
        return VOICE_ERROR_PROTOCOL;
    }
    
    /* 解析属性 */
    size_t offset = sizeof(stun_header_t);
    size_t msg_len = swap16(resp_hdr->length);
    
    while (offset < sizeof(stun_header_t) + msg_len && offset + 4 <= (size_t)received) {
        stun_attr_header_t *attr = (stun_attr_header_t *)(response + offset);
        uint16_t attr_type = swap16(attr->type);
        uint16_t attr_len = swap16(attr->length);
        
        if (attr_type == STUN_ATTR_XOR_MAPPED_ADDRESS && attr_len >= 8) {
            uint8_t *data = response + offset + 4;
            uint8_t family = data[1];
            
            if (family == 0x01) {  /* IPv4 */
                uint16_t port = (data[2] << 8) | data[3];
                port ^= (STUN_MAGIC_COOKIE >> 16) & 0xFFFF;
                
                uint32_t addr = (data[4] << 24) | (data[5] << 16) | 
                               (data[6] << 8) | data[7];
                addr ^= STUN_MAGIC_COOKIE;
                
                result->type = VOICE_ICE_SRFLX;
                result->component = 1;
                result->port = port;
                result->priority = (100 << 24) | (65535 << 8) | 255;
                
                snprintf(result->address, sizeof(result->address),
                         "%u.%u.%u.%u",
                         (addr >> 24) & 0xFF,
                         (addr >> 16) & 0xFF,
                         (addr >> 8) & 0xFF,
                         addr & 0xFF);
                
                strcpy(result->foundation, "srflx");
                
                return VOICE_SUCCESS;
            }
        }
        
        offset += 4 + ((attr_len + 3) & ~3);  /* 4字节对齐 */
    }
    
    return VOICE_ERROR_PROTOCOL;
}

/* ============================================
 * TURN 客户端 (简化实现)
 * ============================================ */

void voice_turn_config_init(voice_turn_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(voice_turn_config_t));
    
    config->port = 3478;
    config->timeout_ms = 5000;
    config->protocol = VOICE_TURN_UDP;
}

voice_turn_client_t *voice_turn_client_create(const voice_turn_config_t *config) {
    voice_turn_client_t *client = (voice_turn_client_t *)calloc(1, sizeof(voice_turn_client_t));
    if (!client) return NULL;
    
    if (config) {
        client->config = *config;
    } else {
        voice_turn_config_init(&client->config);
    }
    
    client->socket_fd = -1;
    
    return client;
}

void voice_turn_client_destroy(voice_turn_client_t *client) {
    if (client) {
        if (client->socket_fd >= 0) {
#ifdef _WIN32
            closesocket(client->socket_fd);
#else
            close(client->socket_fd);
#endif
        }
        free(client);
    }
}

voice_error_t voice_turn_allocate(
    voice_turn_client_t *client,
    voice_ice_candidate_t *result
) {
    /* TODO: 实现 TURN Allocate */
    (void)client;
    (void)result;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_turn_refresh(voice_turn_client_t *client) {
    (void)client;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_turn_create_permission(
    voice_turn_client_t *client,
    const char *peer_address
) {
    (void)client;
    (void)peer_address;
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_turn_channel_bind(
    voice_turn_client_t *client,
    const char *peer_address,
    uint16_t peer_port,
    uint16_t *channel
) {
    (void)client;
    (void)peer_address;
    (void)peer_port;
    (void)channel;
    return VOICE_ERROR_NOT_SUPPORTED;
}

/* ============================================
 * SDP 辅助函数
 * ============================================ */

size_t voice_ice_candidate_to_sdp(
    const voice_ice_candidate_t *candidate,
    char *buffer,
    size_t buffer_size
) {
    if (!candidate || !buffer || buffer_size < 64) {
        return 0;
    }
    
    const char *type_str;
    switch (candidate->type) {
        case VOICE_ICE_HOST:  type_str = "host"; break;
        case VOICE_ICE_SRFLX: type_str = "srflx"; break;
        case VOICE_ICE_PRFLX: type_str = "prflx"; break;
        case VOICE_ICE_RELAY: type_str = "relay"; break;
        default: type_str = "unknown"; break;
    }
    
    int written = snprintf(buffer, buffer_size,
        "candidate:%s %u UDP %u %s %u typ %s",
        candidate->foundation,
        candidate->component,
        candidate->priority,
        candidate->address,
        candidate->port,
        type_str
    );
    
    if (candidate->related_address[0] && candidate->related_port > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            " raddr %s rport %u",
            candidate->related_address,
            candidate->related_port
        );
    }
    
    return (size_t)written;
}

voice_error_t voice_ice_candidate_from_sdp(
    const char *sdp,
    voice_ice_candidate_t *candidate
) {
    if (!sdp || !candidate) return VOICE_ERROR_INVALID_PARAM;
    
    memset(candidate, 0, sizeof(voice_ice_candidate_t));
    
    char type_str[16] = {0};
    
    int matched = sscanf(sdp, "candidate:%31s %u UDP %u %63s %hu typ %15s",
        candidate->foundation,
        &candidate->component,
        &candidate->priority,
        candidate->address,
        &candidate->port,
        type_str
    );
    
    if (matched < 6) return VOICE_ERROR_INVALID_PARAM;
    
    if (strcmp(type_str, "host") == 0) {
        candidate->type = VOICE_ICE_HOST;
    } else if (strcmp(type_str, "srflx") == 0) {
        candidate->type = VOICE_ICE_SRFLX;
    } else if (strcmp(type_str, "prflx") == 0) {
        candidate->type = VOICE_ICE_PRFLX;
    } else if (strcmp(type_str, "relay") == 0) {
        candidate->type = VOICE_ICE_RELAY;
    }
    
    /* 尝试解析 raddr/rport */
    const char *raddr = strstr(sdp, "raddr ");
    if (raddr) {
        sscanf(raddr, "raddr %63s", candidate->related_address);
    }
    
    const char *rport = strstr(sdp, "rport ");
    if (rport) {
        sscanf(rport, "rport %hu", &candidate->related_port);
    }
    
    return VOICE_SUCCESS;
}
