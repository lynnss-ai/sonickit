/**
 * @file rtp.c
 * @brief RTP/RTCP protocol implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "network/rtp.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <arpa/inet.h>
#include <sys/time.h>
#endif

/* ============================================
 * RTP 会话结构
 * ============================================ */

struct rtp_session_s {
    rtp_session_config_t config;

    /* 发送状态 */
    uint16_t sequence;
    uint32_t timestamp;
    uint64_t packets_sent;
    uint64_t bytes_sent;

    /* 接收状态 */
    bool first_packet_received;
    uint16_t max_seq;           /* 最高接收序列号 */
    uint32_t cycles;            /* 序列号回绕次数 */
    uint32_t base_seq;          /* 基础序列号 */
    uint32_t bad_seq;           /* 错序阈值 */
    uint32_t probation;         /* 探测计数 */
    uint64_t packets_received;
    uint64_t bytes_received;
    uint32_t packets_lost;
    uint32_t packets_reordered;
    uint32_t packets_duplicate;

    /* 抖动计算 */
    uint32_t jitter;            /* 抖动 (Q4 定点) */
    uint32_t last_rtp_timestamp;
    uint32_t last_arrival_time;

    /* RTCP 状态 */
    uint32_t last_sr_ntp_sec;
    uint32_t last_sr_ntp_frac;
    uint32_t last_sr_time;
    uint32_t rtt_ms;
};

#define RTP_SEQ_MOD         (1 << 16)
#define RTP_MAX_DROPOUT     3000
#define RTP_MAX_MISORDER    100
#define RTP_MIN_SEQUENTIAL  2

/* ============================================
 * 工具函数实现
 * ============================================ */

uint32_t rtp_generate_ssrc(void)
{
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = true;
    }

    uint32_t ssrc = 0;
    ssrc |= (uint32_t)(rand() & 0xFFFF) << 16;
    ssrc |= (uint32_t)(rand() & 0xFFFF);

    return ssrc;
}

uint16_t rtp_generate_sequence(void)
{
    return (uint16_t)(rand() & 0xFFFF);
}

void rtp_get_ntp_timestamp(uint32_t *ntp_sec, uint32_t *ntp_frac)
{
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimeAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;

    /* FILETIME 是从 1601-01-01 开始的 100ns 间隔 */
    /* NTP 是从 1900-01-01 开始的秒数 */
    uint64_t ntp_time = uli.QuadPart - 116444736000000000ULL;
    *ntp_sec = (uint32_t)(ntp_time / 10000000);
    *ntp_frac = (uint32_t)((ntp_time % 10000000) * 429);
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);

    /* Unix 时间 + 70年偏移 = NTP 时间 */
    *ntp_sec = (uint32_t)(tv.tv_sec + 2208988800UL);
    *ntp_frac = (uint32_t)((double)tv.tv_usec * 4294.967296);
#endif
}

int32_t rtp_sequence_diff(uint16_t seq1, uint16_t seq2)
{
    int32_t diff = (int32_t)seq1 - (int32_t)seq2;

    if (diff > 32768) {
        diff -= 65536;
    } else if (diff < -32768) {
        diff += 65536;
    }

    return diff;
}

static uint32_t get_current_time_ms(void)
{
#ifdef _WIN32
    return GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint32_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

/* ============================================
 * RTP 会话实现
 * ============================================ */

void rtp_session_config_init(rtp_session_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(rtp_session_config_t));
    config->ssrc = 0;  /* 自动生成 */
    config->payload_type = 111;  /* Opus */
    config->clock_rate = 48000;
    config->max_packet_size = RTP_MAX_PACKET_SIZE;
    config->enable_rtcp = true;
    config->rtcp_bandwidth = 5000;  /* 5% 的音频带宽 */
}

rtp_session_t *rtp_session_create(const rtp_session_config_t *config)
{
    if (!config) {
        return NULL;
    }

    rtp_session_t *session = (rtp_session_t *)calloc(1, sizeof(rtp_session_t));
    if (!session) {
        return NULL;
    }

    session->config = *config;

    /* 生成 SSRC 和初始序列号 */
    if (session->config.ssrc == 0) {
        session->config.ssrc = rtp_generate_ssrc();
    }
    session->sequence = rtp_generate_sequence();
    session->timestamp = 0;

    /* 初始化接收状态 */
    session->first_packet_received = false;
    session->probation = RTP_MIN_SEQUENTIAL;
    session->bad_seq = RTP_SEQ_MOD + 1;

    VOICE_LOG_I("RTP session created: SSRC=0x%08X, PT=%u",
        session->config.ssrc, session->config.payload_type);

    return session;
}

void rtp_session_destroy(rtp_session_t *session)
{
    if (!session) return;
    free(session);
}

voice_error_t rtp_session_create_packet(
    rtp_session_t *session,
    const uint8_t *payload,
    size_t payload_size,
    uint32_t timestamp,
    bool marker,
    uint8_t *output,
    size_t *output_size)
{
    if (!session || !payload || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }

    size_t packet_size = RTP_HEADER_SIZE + payload_size;
    if (*output_size < packet_size) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    /* 构建 RTP 头 */
    uint8_t *p = output;

    /* 第一个字节: V=2, P=0, X=0, CC=0 */
    *p++ = (RTP_VERSION << 6);

    /* 第二个字节: M, PT */
    *p++ = (marker ? 0x80 : 0x00) | (session->config.payload_type & 0x7F);

    /* 序列号 (网络字节序) */
    uint16_t seq_net = htons(session->sequence);
    memcpy(p, &seq_net, 2);
    p += 2;

    /* 时间戳 (网络字节序) */
    uint32_t ts_net = htonl(timestamp);
    memcpy(p, &ts_net, 4);
    p += 4;

    /* SSRC (网络字节序) */
    uint32_t ssrc_net = htonl(session->config.ssrc);
    memcpy(p, &ssrc_net, 4);
    p += 4;

    /* 复制载荷 */
    memcpy(p, payload, payload_size);

    /* 更新状态 */
    session->sequence++;
    session->packets_sent++;
    session->bytes_sent += payload_size;

    *output_size = packet_size;
    return VOICE_OK;
}

voice_error_t rtp_session_parse_packet(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size,
    rtp_packet_t *packet)
{
    (void)session;

    if (!data || !packet) {
        return VOICE_ERROR_NULL_POINTER;
    }

    if (size < RTP_HEADER_SIZE) {
        return VOICE_ERROR_INVALID_PACKET;
    }

    const uint8_t *p = data;

    /* 解析第一个字节 */
    uint8_t byte0 = *p++;
    uint8_t version = (byte0 >> 6) & 0x03;
    if (version != RTP_VERSION) {
        return VOICE_ERROR_INVALID_PACKET;
    }

    packet->header.version = version;
    packet->header.padding = (byte0 >> 5) & 0x01;
    packet->header.extension = (byte0 >> 4) & 0x01;
    packet->header.csrc_count = byte0 & 0x0F;

    /* 解析第二个字节 */
    uint8_t byte1 = *p++;
    packet->header.marker = (byte1 >> 7) & 0x01;
    packet->header.payload_type = byte1 & 0x7F;

    /* 解析序列号 */
    uint16_t seq_net;
    memcpy(&seq_net, p, 2);
    packet->header.sequence = ntohs(seq_net);
    p += 2;

    /* 解析时间戳 */
    uint32_t ts_net;
    memcpy(&ts_net, p, 4);
    packet->header.timestamp = ntohl(ts_net);
    p += 4;

    /* 解析 SSRC */
    uint32_t ssrc_net;
    memcpy(&ssrc_net, p, 4);
    packet->header.ssrc = ntohl(ssrc_net);
    p += 4;

    size_t header_size = RTP_HEADER_SIZE;

    /* 解析 CSRC 列表 */
    if (packet->header.csrc_count > 0) {
        if (packet->header.csrc_count > RTP_MAX_CSRC) {
            return VOICE_ERROR_INVALID_PACKET;
        }

        size_t csrc_size = packet->header.csrc_count * 4;
        if (size < header_size + csrc_size) {
            return VOICE_ERROR_INVALID_PACKET;
        }

        for (int i = 0; i < packet->header.csrc_count; i++) {
            uint32_t csrc_net;
            memcpy(&csrc_net, p, 4);
            packet->csrc[i] = ntohl(csrc_net);
            p += 4;
        }

        header_size += csrc_size;
    }

    /* 解析扩展头 */
    packet->has_extension = false;
    if (packet->header.extension) {
        if (size < header_size + 4) {
            return VOICE_ERROR_INVALID_PACKET;
        }

        uint16_t profile;
        memcpy(&profile, p, 2);
        packet->extension_profile = ntohs(profile);
        p += 2;

        uint16_t ext_len;
        memcpy(&ext_len, p, 2);
        ext_len = ntohs(ext_len) * 4;  /* 32位字 */
        p += 2;

        if (size < header_size + 4 + ext_len) {
            return VOICE_ERROR_INVALID_PACKET;
        }

        packet->has_extension = true;
        packet->extension_data = p;
        packet->extension_size = ext_len;

        p += ext_len;
        header_size += 4 + ext_len;
    }

    /* 处理填充 */
    size_t payload_size = size - header_size;
    if (packet->header.padding && payload_size > 0) {
        uint8_t pad_len = data[size - 1];
        if (pad_len > payload_size) {
            return VOICE_ERROR_INVALID_PACKET;
        }
        payload_size -= pad_len;
    }

    packet->payload = p;
    packet->payload_size = payload_size;

    return VOICE_OK;
}

voice_error_t rtp_session_process_received(
    rtp_session_t *session,
    const rtp_packet_t *packet)
{
    if (!session || !packet) {
        return VOICE_ERROR_NULL_POINTER;
    }

    uint16_t seq = packet->header.sequence;
    uint32_t ts = packet->header.timestamp;
    uint32_t arrival = get_current_time_ms();

    /* 首包初始化 */
    if (!session->first_packet_received) {
        session->first_packet_received = true;
        session->base_seq = seq;
        session->max_seq = seq;
        session->bad_seq = RTP_SEQ_MOD + 1;
        session->cycles = 0;
        session->probation = 0;
        session->packets_received = 1;
        session->bytes_received = packet->payload_size;
        session->last_rtp_timestamp = ts;
        session->last_arrival_time = arrival;
        return VOICE_OK;
    }

    /* 序列号检查 */
    int32_t delta = rtp_sequence_diff(seq, session->max_seq);

    if (delta >= 0) {
        /* 正常或跳跃 */
        if (delta > RTP_MAX_DROPOUT) {
            /* 大跳跃，可能是新的源 */
            session->bad_seq = seq + 1;
            return VOICE_OK;
        }

        if (seq < session->max_seq) {
            /* 序列号回绕 */
            session->cycles += RTP_SEQ_MOD;
        }
        session->max_seq = seq;

    } else {
        /* 乱序 */
        if (delta < -RTP_MAX_MISORDER) {
            /* 太旧的包，丢弃 */
            return VOICE_OK;
        }
        session->packets_reordered++;
    }

    /* 抖动计算 (RFC 3550) */
    uint32_t transit = arrival - ts;
    int32_t d = (int32_t)(transit - session->last_arrival_time + session->last_rtp_timestamp);
    if (d < 0) d = -d;
    session->jitter += (d - session->jitter) / 16;

    session->last_rtp_timestamp = ts;
    session->last_arrival_time = arrival;

    /* 更新统计 */
    session->packets_received++;
    session->bytes_received += packet->payload_size;

    return VOICE_OK;
}

voice_error_t rtp_session_get_statistics(
    rtp_session_t *session,
    rtp_statistics_t *stats)
{
    if (!session || !stats) {
        return VOICE_ERROR_NULL_POINTER;
    }

    memset(stats, 0, sizeof(rtp_statistics_t));

    stats->packets_sent = session->packets_sent;
    stats->bytes_sent = session->bytes_sent;
    stats->packets_received = session->packets_received;
    stats->bytes_received = session->bytes_received;
    stats->packets_reordered = session->packets_reordered;
    stats->packets_duplicate = session->packets_duplicate;
    stats->jitter = session->jitter / session->config.clock_rate;
    stats->rtt_ms = session->rtt_ms;

    /* 计算丢包 */
    if (session->first_packet_received) {
        uint32_t extended_max = session->cycles + session->max_seq;
        uint32_t expected = extended_max - session->base_seq + 1;
        int32_t lost = (int32_t)expected - (int32_t)session->packets_received;
        if (lost < 0) lost = 0;
        stats->packets_lost_recv = (uint32_t)lost;
        stats->fraction_lost = expected > 0 ? (float)lost / expected : 0;
    }

    return VOICE_OK;
}

void rtp_session_reset_statistics(rtp_session_t *session)
{
    if (!session) return;

    session->packets_sent = 0;
    session->bytes_sent = 0;
    session->packets_received = 0;
    session->bytes_received = 0;
    session->packets_lost = 0;
    session->packets_reordered = 0;
    session->packets_duplicate = 0;
    session->jitter = 0;
    session->first_packet_received = false;
}

uint32_t rtp_session_get_ssrc(rtp_session_t *session)
{
    return session ? session->config.ssrc : 0;
}

void rtp_session_set_ssrc(rtp_session_t *session, uint32_t ssrc)
{
    if (session) {
        session->config.ssrc = ssrc;
    }
}

/* ============================================
 * RTCP 实现
 * ============================================ */

voice_error_t rtcp_create_sr(
    rtp_session_t *session,
    uint8_t *output,
    size_t *output_size)
{
    if (!session || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }

    size_t sr_size = 4 + 24;  /* header + SR info */
    if (*output_size < sr_size) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    uint8_t *p = output;

    /* RTCP 头 */
    *p++ = (RTP_VERSION << 6);  /* V=2, P=0, RC=0 */
    *p++ = RTCP_TYPE_SR;
    uint16_t length = htons((sr_size / 4) - 1);
    memcpy(p, &length, 2);
    p += 2;

    /* SSRC */
    uint32_t ssrc = htonl(session->config.ssrc);
    memcpy(p, &ssrc, 4);
    p += 4;

    /* NTP 时间戳 */
    uint32_t ntp_sec, ntp_frac;
    rtp_get_ntp_timestamp(&ntp_sec, &ntp_frac);

    uint32_t ntp_sec_net = htonl(ntp_sec);
    memcpy(p, &ntp_sec_net, 4);
    p += 4;

    uint32_t ntp_frac_net = htonl(ntp_frac);
    memcpy(p, &ntp_frac_net, 4);
    p += 4;

    /* RTP 时间戳 */
    uint32_t rtp_ts = htonl(session->timestamp);
    memcpy(p, &rtp_ts, 4);
    p += 4;

    /* 发送包数 */
    uint32_t pkts = htonl((uint32_t)session->packets_sent);
    memcpy(p, &pkts, 4);
    p += 4;

    /* 发送字节数 */
    uint32_t bytes = htonl((uint32_t)session->bytes_sent);
    memcpy(p, &bytes, 4);
    p += 4;

    *output_size = sr_size;
    return VOICE_OK;
}

voice_error_t rtcp_create_rr(
    rtp_session_t *session,
    uint8_t *output,
    size_t *output_size)
{
    if (!session || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }

    size_t rr_size = 4 + 4 + 24;  /* header + SSRC + RR block */
    if (*output_size < rr_size) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    uint8_t *p = output;

    /* RTCP 头 */
    *p++ = (RTP_VERSION << 6) | 1;  /* V=2, P=0, RC=1 */
    *p++ = RTCP_TYPE_RR;
    uint16_t length = htons((rr_size / 4) - 1);
    memcpy(p, &length, 2);
    p += 2;

    /* 报告者 SSRC */
    uint32_t ssrc = htonl(session->config.ssrc);
    memcpy(p, &ssrc, 4);
    p += 4;

    /* Report Block */
    /* SSRC of source (使用已知的远端 SSRC) */
    uint32_t src_ssrc = htonl(0);  /* TODO: 跟踪远端SSRC */
    memcpy(p, &src_ssrc, 4);
    p += 4;

    /* 丢包率和累计丢包 */
    uint32_t extended_max = session->cycles + session->max_seq;
    uint32_t expected = extended_max - session->base_seq + 1;
    int32_t lost = (int32_t)expected - (int32_t)session->packets_received;
    if (lost < 0) lost = 0;

    uint8_t fraction = expected > 0 ? (uint8_t)((lost * 256) / expected) : 0;
    uint32_t frac_lost = (fraction << 24) | (lost & 0x00FFFFFF);
    frac_lost = htonl(frac_lost);
    memcpy(p, &frac_lost, 4);
    p += 4;

    /* 最高接收序列号 */
    uint32_t highest = htonl(extended_max);
    memcpy(p, &highest, 4);
    p += 4;

    /* 抖动 */
    uint32_t jitter = htonl(session->jitter);
    memcpy(p, &jitter, 4);
    p += 4;

    /* LSR */
    uint32_t lsr = htonl((session->last_sr_ntp_sec << 16) |
        (session->last_sr_ntp_frac >> 16));
    memcpy(p, &lsr, 4);
    p += 4;

    /* DLSR */
    uint32_t dlsr = 0;
    if (session->last_sr_time > 0) {
        dlsr = get_current_time_ms() - session->last_sr_time;
        dlsr = dlsr * 65536 / 1000;  /* 转换为 1/65536 秒 */
    }
    dlsr = htonl(dlsr);
    memcpy(p, &dlsr, 4);
    p += 4;

    *output_size = rr_size;
    return VOICE_OK;
}

voice_error_t rtcp_create_bye(
    rtp_session_t *session,
    const char *reason,
    uint8_t *output,
    size_t *output_size)
{
    if (!session || !output || !output_size) {
        return VOICE_ERROR_NULL_POINTER;
    }

    size_t reason_len = reason ? strlen(reason) : 0;
    size_t bye_size = 4 + 4;  /* header + SSRC */

    if (reason_len > 0) {
        bye_size += 1 + reason_len;  /* length + reason */
        /* 填充到4字节边界 */
        bye_size = (bye_size + 3) & ~3;
    }

    if (*output_size < bye_size) {
        return VOICE_ERROR_BUFFER_TOO_SMALL;
    }

    uint8_t *p = output;

    /* RTCP 头 */
    *p++ = (RTP_VERSION << 6) | 1;  /* V=2, P=0, SC=1 */
    *p++ = RTCP_TYPE_BYE;
    uint16_t length = htons((bye_size / 4) - 1);
    memcpy(p, &length, 2);
    p += 2;

    /* SSRC */
    uint32_t ssrc = htonl(session->config.ssrc);
    memcpy(p, &ssrc, 4);
    p += 4;

    /* 原因 */
    if (reason_len > 0) {
        *p++ = (uint8_t)reason_len;
        memcpy(p, reason, reason_len);
        p += reason_len;

        /* 填充 */
        while ((p - output) % 4 != 0) {
            *p++ = 0;
        }
    }

    *output_size = bye_size;
    return VOICE_OK;
}

voice_error_t rtcp_parse(
    const uint8_t *data,
    size_t size,
    rtcp_header_t *header)
{
    if (!data || !header) {
        return VOICE_ERROR_NULL_POINTER;
    }

    if (size < 4) {
        return VOICE_ERROR_INVALID_PACKET;
    }

    header->version = (data[0] >> 6) & 0x03;
    header->padding = (data[0] >> 5) & 0x01;
    header->count = data[0] & 0x1F;
    header->packet_type = data[1];

    uint16_t length;
    memcpy(&length, data + 2, 2);
    header->length = ntohs(length);

    if (header->version != RTP_VERSION) {
        return VOICE_ERROR_INVALID_PACKET;
    }

    return VOICE_OK;
}

voice_error_t rtcp_process_sr(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size)
{
    if (!session || !data) {
        return VOICE_ERROR_NULL_POINTER;
    }

    if (size < 28) {  /* header + SR info */
        return VOICE_ERROR_INVALID_PACKET;
    }

    const uint8_t *p = data + 4;  /* 跳过 header */

    /* SSRC */
    uint32_t ssrc;
    memcpy(&ssrc, p, 4);
    ssrc = ntohl(ssrc);
    p += 4;

    /* NTP 时间戳 */
    uint32_t ntp_sec, ntp_frac;
    memcpy(&ntp_sec, p, 4);
    session->last_sr_ntp_sec = ntohl(ntp_sec);
    p += 4;

    memcpy(&ntp_frac, p, 4);
    session->last_sr_ntp_frac = ntohl(ntp_frac);
    p += 4;

    session->last_sr_time = get_current_time_ms();

    return VOICE_OK;
}

voice_error_t rtcp_process_rr(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size)
{
    if (!session || !data) {
        return VOICE_ERROR_NULL_POINTER;
    }

    if (size < 32) {  /* header + SSRC + RR block */
        return VOICE_ERROR_INVALID_PACKET;
    }

    const uint8_t *p = data + 8;  /* 跳过 header + SSRC */

    /* 跳过 SSRC of source, fraction lost, cumulative lost, highest seq */
    p += 16;

    /* 获取 LSR 和 DLSR 用于计算 RTT */
    uint32_t lsr, dlsr;
    memcpy(&lsr, p, 4);
    lsr = ntohl(lsr);
    p += 4;

    memcpy(&dlsr, p, 4);
    dlsr = ntohl(dlsr);

    if (lsr != 0) {
        /* 计算 RTT */
        uint32_t ntp_sec, ntp_frac;
        rtp_get_ntp_timestamp(&ntp_sec, &ntp_frac);

        uint32_t now = (ntp_sec << 16) | (ntp_frac >> 16);
        uint32_t rtt_ntp = now - lsr - dlsr;

        /* 转换为毫秒 */
        session->rtt_ms = (rtt_ntp * 1000) >> 16;
    }

    return VOICE_OK;
}
