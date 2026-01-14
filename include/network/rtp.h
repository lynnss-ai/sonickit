/**
 * @file rtp.h
 * @brief RTP/RTCP protocol interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * RFC 3550: RTP - Real-time Transport Protocol
 * RFC 3551: RTP Profile for Audio and Video Conferences
 */

#ifndef NETWORK_RTP_H
#define NETWORK_RTP_H

#include "voice/types.h"
#include "voice/error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * RTP 常量定义
 * ============================================ */

#define RTP_VERSION             2
#define RTP_HEADER_SIZE         12
#define RTP_MAX_CSRC            15
#define RTP_MAX_PACKET_SIZE     1500
#define RTP_MAX_PAYLOAD_SIZE    (RTP_MAX_PACKET_SIZE - RTP_HEADER_SIZE)

/* 动态载荷类型范围 */
#define RTP_PT_DYNAMIC_MIN      96
#define RTP_PT_DYNAMIC_MAX      127

/* ============================================
 * RTP 头部结构
 * ============================================ */

/** RTP 固定头部 (12字节) */
typedef struct {
#if defined(__BIG_ENDIAN__)
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t extension:1;
    uint8_t csrc_count:4;
    uint8_t marker:1;
    uint8_t payload_type:7;
#else
    uint8_t csrc_count:4;
    uint8_t extension:1;
    uint8_t padding:1;
    uint8_t version:2;
    uint8_t payload_type:7;
    uint8_t marker:1;
#endif
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

/** RTP 扩展头 */
typedef struct {
    uint16_t profile_specific;
    uint16_t length;  /* 扩展数据长度 (32位字) */
    /* 扩展数据跟随其后 */
} rtp_extension_t;

/** RTP 包结构 */
typedef struct {
    rtp_header_t header;
    uint32_t csrc[RTP_MAX_CSRC];
    const uint8_t *payload;
    size_t payload_size;
    
    /* 扩展头信息 */
    bool has_extension;
    uint16_t extension_profile;
    const uint8_t *extension_data;
    size_t extension_size;
} rtp_packet_t;

/* ============================================
 * RTCP 类型定义
 * ============================================ */

/** RTCP 包类型 */
typedef enum {
    RTCP_TYPE_SR   = 200,   /**< Sender Report */
    RTCP_TYPE_RR   = 201,   /**< Receiver Report */
    RTCP_TYPE_SDES = 202,   /**< Source Description */
    RTCP_TYPE_BYE  = 203,   /**< Goodbye */
    RTCP_TYPE_APP  = 204,   /**< Application-defined */
    RTCP_TYPE_RTPFB = 205,  /**< Transport Layer Feedback */
    RTCP_TYPE_PSFB = 206,   /**< Payload-specific Feedback */
} rtcp_type_t;

/** RTCP 公共头 */
typedef struct {
#if defined(__BIG_ENDIAN__)
    uint8_t version:2;
    uint8_t padding:1;
    uint8_t count:5;
#else
    uint8_t count:5;
    uint8_t padding:1;
    uint8_t version:2;
#endif
    uint8_t packet_type;
    uint16_t length;  /* 长度 (32位字) - 1 */
} rtcp_header_t;

/** RTCP SR (Sender Report) */
typedef struct {
    uint32_t ssrc;
    uint32_t ntp_sec;       /**< NTP 时间戳 (秒) */
    uint32_t ntp_frac;      /**< NTP 时间戳 (小数) */
    uint32_t rtp_timestamp; /**< RTP 时间戳 */
    uint32_t packet_count;  /**< 发送包数 */
    uint32_t octet_count;   /**< 发送字节数 */
} rtcp_sr_t;

/** RTCP RR (Receiver Report Block) */
typedef struct {
    uint32_t ssrc;          /**< 报告源 SSRC */
    uint32_t fraction_lost:8;
    uint32_t cumulative_lost:24;
    uint32_t highest_seq;   /**< 最高接收序列号 */
    uint32_t jitter;        /**< 抖动 */
    uint32_t last_sr;       /**< 最后SR时间戳 */
    uint32_t delay_since_sr;/**< 距离最后SR的延迟 */
} rtcp_rr_block_t;

/* ============================================
 * RTP 会话句柄
 * ============================================ */

typedef struct rtp_session_s rtp_session_t;

/** RTP 会话配置 */
typedef struct {
    uint32_t ssrc;              /**< SSRC (0=自动生成) */
    uint8_t payload_type;       /**< 载荷类型 */
    uint32_t clock_rate;        /**< 时钟频率 */
    uint32_t max_packet_size;   /**< 最大包大小 */
    bool enable_rtcp;           /**< 启用RTCP */
    uint32_t rtcp_bandwidth;    /**< RTCP带宽 (bps) */
} rtp_session_config_t;

/** RTP 统计信息 */
typedef struct {
    /* 发送统计 */
    uint64_t packets_sent;
    uint64_t bytes_sent;
    uint32_t packets_lost;
    
    /* 接收统计 */
    uint64_t packets_received;
    uint64_t bytes_received;
    uint32_t packets_lost_recv;
    uint32_t packets_reordered;
    uint32_t packets_duplicate;
    
    /* 质量指标 */
    uint32_t jitter;            /**< 抖动 (时钟单位) */
    float fraction_lost;        /**< 丢包率 (0-1) */
    uint32_t rtt_ms;            /**< 往返延迟 (ms) */
} rtp_statistics_t;

/* ============================================
 * RTP 会话 API
 * ============================================ */

/**
 * @brief 初始化默认配置
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void rtp_session_config_init(rtp_session_config_t *config);

/**
 * @brief 创建 RTP 会话
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
rtp_session_t *rtp_session_create(const rtp_session_config_t *config);

/**
 * @brief 销毁 RTP 会话
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void rtp_session_destroy(rtp_session_t *session);

/**
 * @brief 创建 RTP 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * @param session RTP会话
 * @param payload 载荷数据
 * @param payload_size 载荷大小
 * @param timestamp RTP时间戳
 * @param marker 标记位
 * @param output 输出缓冲区
 * @param output_size 缓冲区大小(输入)/实际大小(输出)
 * @return 错误码
 */
voice_error_t rtp_session_create_packet(
    rtp_session_t *session,
    const uint8_t *payload,
    size_t payload_size,
    uint32_t timestamp,
    bool marker,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief 解析 RTP 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * @param session RTP会话 (可为NULL)
 * @param data 包数据
 * @param size 包大小
 * @param packet 输出解析结果
 * @return 错误码
 */
voice_error_t rtp_session_parse_packet(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size,
    rtp_packet_t *packet
);

/**
 * @brief 处理接收的 RTP 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 * 
 * 更新接收统计，处理序列号等
 */
voice_error_t rtp_session_process_received(
    rtp_session_t *session,
    const rtp_packet_t *packet
);

/**
 * @brief 获取统计信息
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtp_session_get_statistics(
    rtp_session_t *session,
    rtp_statistics_t *stats
);

/**
 * @brief 重置统计信息
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void rtp_session_reset_statistics(rtp_session_t *session);

/**
 * @brief 获取 SSRC
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint32_t rtp_session_get_ssrc(rtp_session_t *session);

/**
 * @brief 设置 SSRC
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void rtp_session_set_ssrc(rtp_session_t *session, uint32_t ssrc);

/* ============================================
 * RTCP API
 * ============================================ */

/**
 * @brief 创建 RTCP SR 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtcp_create_sr(
    rtp_session_t *session,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief 创建 RTCP RR 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtcp_create_rr(
    rtp_session_t *session,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief 创建 RTCP BYE 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtcp_create_bye(
    rtp_session_t *session,
    const char *reason,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief 解析 RTCP 包
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtcp_parse(
    const uint8_t *data,
    size_t size,
    rtcp_header_t *header
);

/**
 * @brief 处理接收的 RTCP SR
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtcp_process_sr(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size
);

/**
 * @brief 处理接收的 RTCP RR
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
voice_error_t rtcp_process_rr(
    rtp_session_t *session,
    const uint8_t *data,
    size_t size
);

/* ============================================
 * 工具函数
 * ============================================ */

/**
 * @brief 生成随机 SSRC
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint32_t rtp_generate_ssrc(void);

/**
 * @brief 生成随机初始序列号
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
uint16_t rtp_generate_sequence(void);

/**
 * @brief 获取当前 NTP 时间戳
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
void rtp_get_ntp_timestamp(uint32_t *ntp_sec, uint32_t *ntp_frac);

/**
 * @brief 计算序列号差值 (处理回绕)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */
int32_t rtp_sequence_diff(uint16_t seq1, uint16_t seq2);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_RTP_H */
