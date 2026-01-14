/**
 * @file codec.h
 * @brief Audio codec abstraction interface
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#ifndef CODEC_CODEC_H
#define CODEC_CODEC_H

#include "voice/types.h"
#include "voice/error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================
 * 编解码器类型 - 使用 types.h 中定义的类型
 * ============================================ */

/** 编解码器类型别名 (兼容旧代码) */
typedef voice_codec_type_t voice_codec_id_t;

#define VOICE_CODEC_UNKNOWN VOICE_CODEC_NONE
#define VOICE_CODEC_MAX VOICE_CODEC_COUNT

/* ============================================
 * 编解码器句柄
 * ============================================ */

typedef struct voice_encoder_s voice_encoder_t;
typedef struct voice_decoder_s voice_decoder_t;

/* ============================================
 * 编解码器配置
 * ============================================ */

/** Opus编解码器配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率: 8000/12000/16000/24000/48000 */
    uint8_t channels;           /**< 通道数: 1或2 */
    uint32_t bitrate;           /**< 比特率 (bps): 6000-510000 */
    int application;            /**< 应用类型: VOIP/AUDIO/LOWDELAY */
    int complexity;             /**< 复杂度 (0-10) */
    bool enable_fec;            /**< 启用FEC */
    int packet_loss_perc;       /**< 预期丢包率 (0-100) */
    bool enable_dtx;            /**< 启用DTX */
    bool enable_vbr;            /**< 启用VBR */
    int signal_type;            /**< 信号类型: AUTO/VOICE/MUSIC */
} voice_opus_config_t;

/** G.711编解码器配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 (必须是8000) */
    bool use_alaw;              /**< true=A-law, false=μ-law */
} voice_g711_config_t;

/** G.722编解码器配置 */
typedef struct {
    uint32_t sample_rate;       /**< 采样率 (必须是16000) */
    int bitrate_mode;           /**< 比特率模式: 0=64kbps, 1=56kbps, 2=48kbps */
} voice_g722_config_t;

/** 详细编解码器配置 (codec模块专用) */
typedef struct {
    voice_codec_id_t codec_id;
    union {
        voice_opus_config_t opus;
        voice_g711_config_t g711;
        voice_g722_config_t g722;
    } u;
} voice_codec_detail_config_t;

/* ============================================
 * 编解码器信息
 * ============================================ */

typedef struct {
    voice_codec_id_t codec_id;
    const char *name;           /**< 编解码器名称 */
    uint8_t rtp_payload_type;   /**< RTP载荷类型 */
    uint32_t sample_rate;       /**< 采样率 */
    uint8_t channels;           /**< 通道数 */
    uint32_t frame_duration_ms; /**< 帧时长(ms) */
    uint32_t frame_size;        /**< 帧大小(样本数) */
    uint32_t bitrate;           /**< 比特率(bps) */
    bool is_vbr;                /**< 是否VBR */
} voice_codec_info_t;

/* ============================================
 * 默认配置初始化
 * ============================================ */

/**
 * @brief 初始化默认Opus配置
 */
void voice_opus_config_init(voice_opus_config_t *config);

/**
 * @brief 初始化默认G.711配置
 */
void voice_g711_config_init(voice_g711_config_t *config, bool use_alaw);

/**
 * @brief 初始化默认G.722配置
 */
void voice_g722_config_init(voice_g722_config_t *config);

/* ============================================
 * 编码器API
 * ============================================ */

/**
 * @brief 创建编码器
 * @param config 编解码器配置
 * @return 编码器句柄
 */
voice_encoder_t *voice_encoder_create(const voice_codec_detail_config_t *config);

/**
 * @brief 销毁编码器
 * @param encoder 编码器句柄
 */
void voice_encoder_destroy(voice_encoder_t *encoder);

/**
 * @brief 编码音频帧
 * @param encoder 编码器句柄
 * @param pcm_input 输入PCM数据
 * @param pcm_samples 输入样本数
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小(输入)/实际输出字节数(输出)
 * @return 错误码
 */
voice_error_t voice_encoder_encode(
    voice_encoder_t *encoder,
    const int16_t *pcm_input,
    size_t pcm_samples,
    uint8_t *output,
    size_t *output_size
);

/**
 * @brief 重置编码器状态
 * @param encoder 编码器句柄
 */
void voice_encoder_reset(voice_encoder_t *encoder);

/**
 * @brief 获取编解码器信息
 * @param encoder 编码器句柄
 * @param info 输出信息结构
 * @return 错误码
 */
voice_error_t voice_encoder_get_info(
    voice_encoder_t *encoder,
    voice_codec_info_t *info
);

/**
 * @brief 设置比特率 (仅Opus)
 * @param encoder 编码器句柄
 * @param bitrate 比特率(bps)
 * @return 错误码
 */
voice_error_t voice_encoder_set_bitrate(
    voice_encoder_t *encoder,
    uint32_t bitrate
);

/**
 * @brief 设置丢包率 (仅Opus)
 * @param encoder 编码器句柄
 * @param packet_loss_perc 丢包率 (0-100)
 * @return 错误码
 */
voice_error_t voice_encoder_set_packet_loss(
    voice_encoder_t *encoder,
    int packet_loss_perc
);

/* ============================================
 * 解码器API
 * ============================================ */

/**
 * @brief 创建解码器
 * @param config 编解码器配置
 * @return 解码器句柄
 */
voice_decoder_t *voice_decoder_create(const voice_codec_detail_config_t *config);

/**
 * @brief 销毁解码器
 * @param decoder 解码器句柄
 */
void voice_decoder_destroy(voice_decoder_t *decoder);

/**
 * @brief 解码音频帧
 * @param decoder 解码器句柄
 * @param input 编码数据
 * @param input_size 编码数据字节数
 * @param pcm_output 输出PCM缓冲区
 * @param pcm_samples 缓冲区大小(输入)/输出样本数(输出)
 * @return 错误码
 */
voice_error_t voice_decoder_decode(
    voice_decoder_t *decoder,
    const uint8_t *input,
    size_t input_size,
    int16_t *pcm_output,
    size_t *pcm_samples
);

/**
 * @brief 丢包补偿 (PLC)
 * @param decoder 解码器句柄
 * @param pcm_output 输出PCM缓冲区
 * @param pcm_samples 请求样本数(输入)/输出样本数(输出)
 * @return 错误码
 */
voice_error_t voice_decoder_plc(
    voice_decoder_t *decoder,
    int16_t *pcm_output,
    size_t *pcm_samples
);

/**
 * @brief 重置解码器状态
 * @param decoder 解码器句柄
 */
void voice_decoder_reset(voice_decoder_t *decoder);

/**
 * @brief 获取编解码器信息
 * @param decoder 解码器句柄
 * @param info 输出信息结构
 * @return 错误码
 */
voice_error_t voice_decoder_get_info(
    voice_decoder_t *decoder,
    voice_codec_info_t *info
);

/* ============================================
 * 编解码器工具函数
 * ============================================ */

/**
 * @brief 获取编解码器名称
 * @param codec_id 编解码器ID
 * @return 名称字符串
 */
const char *voice_codec_get_name(voice_codec_id_t codec_id);

/**
 * @brief 获取RTP载荷类型
 * @param codec_id 编解码器ID
 * @return RTP载荷类型 (0-127)
 */
uint8_t voice_codec_get_rtp_payload_type(voice_codec_id_t codec_id);

/**
 * @brief 从RTP载荷类型获取编解码器ID
 * @param payload_type RTP载荷类型
 * @return 编解码器ID
 */
voice_codec_id_t voice_codec_from_rtp_payload_type(uint8_t payload_type);

/**
 * @brief 计算编码后最大字节数
 * @param codec_id 编解码器ID
 * @param samples 样本数
 * @return 最大字节数
 */
size_t voice_codec_get_max_encoded_size(
    voice_codec_id_t codec_id,
    size_t samples
);

#ifdef __cplusplus
}
#endif

#endif /* CODEC_CODEC_H */
