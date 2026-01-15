/**
 * @file pipeline.c
 * @brief Audio processing pipeline implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "voice/pipeline.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define THREAD_FUNC DWORD WINAPI
#define THREAD_RETURN return 0
typedef HANDLE thread_handle_t;
typedef CRITICAL_SECTION mutex_t;
#define mutex_init(m) InitializeCriticalSection(m)
#define mutex_destroy(m) DeleteCriticalSection(m)
#define mutex_lock(m) EnterCriticalSection(m)
#define mutex_unlock(m) LeaveCriticalSection(m)
#else
#include <pthread.h>
#define THREAD_FUNC void*
#define THREAD_RETURN return NULL
typedef pthread_t thread_handle_t;
typedef pthread_mutex_t mutex_t;
#define mutex_init(m) pthread_mutex_init(m, NULL)
#define mutex_destroy(m) pthread_mutex_destroy(m)
#define mutex_lock(m) pthread_mutex_lock(m)
#define mutex_unlock(m) pthread_mutex_unlock(m)
#endif

/* ============================================
 * 管线结构
 * ============================================ */

struct voice_pipeline_s {
    voice_pipeline_ext_config_t config;
    pipeline_state_t state;
    mutex_t state_mutex;

    /* 音频设备 */
    voice_device_t *device;

    /* DSP 模块 */
    voice_resampler_t *resampler_cap;   /* 采集重采样器 */
    voice_resampler_t *resampler_play;  /* 播放重采样器 */
    voice_denoiser_t *denoiser;
    voice_aec_t *aec;

    /* 编解码器 */
    voice_encoder_t *encoder;
    voice_decoder_t *decoder;

    /* 网络 */
    rtp_session_t *rtp_session;
    srtp_session_t *srtp_send;
    srtp_session_t *srtp_recv;
    jitter_buffer_t *jitter_buffer;
    voice_plc_t *plc;

    /* 缓冲区 */
    voice_ring_buffer_t *capture_buffer;
    voice_ring_buffer_t *playback_buffer;
    int16_t *processing_buffer;
    int16_t *output_buffer;
    uint8_t *encoded_buffer;
    uint8_t *decrypt_buffer;    /* 预分配的解密缓冲区 */
    uint8_t *rtp_send_buffer;   /* 预分配的 RTP 封包缓冲区 */
    size_t frame_samples;

    /* 时间戳 */
    uint32_t rtp_timestamp;

    /* 回调 */
    pipeline_encoded_callback_t encoded_cb;
    void *encoded_cb_data;
    pipeline_decoded_callback_t decoded_cb;
    void *decoded_cb_data;
    pipeline_state_callback_t state_cb;
    void *state_cb_data;
    pipeline_error_callback_t error_cb;
    void *error_cb_data;

    /* 统计 */
    voice_pipeline_stats_t stats;

    /* 控制标志 */
    bool capture_muted;
    bool playback_muted;
    float playback_volume;
    bool aec_enabled;
    bool denoise_enabled;
    bool agc_enabled;
};

/* ============================================
 * 内部函数声明
 * ============================================ */

static void pipeline_capture_callback(
    voice_device_t *device,
    const int16_t *input,
    size_t frame_count,
    void *user_data
);

static void pipeline_playback_callback(
    voice_device_t *device,
    int16_t *output,
    size_t frame_count,
    void *user_data
);

static void pipeline_process_capture(voice_pipeline_t *pipeline);
static void pipeline_process_playback(voice_pipeline_t *pipeline);
static void pipeline_set_state(voice_pipeline_t *pipeline, pipeline_state_t state);

/* ============================================
 * 管线 API 实现
 * ============================================ */

void voice_pipeline_ext_config_init(voice_pipeline_ext_config_t *config)
{
    if (!config) return;

    memset(config, 0, sizeof(voice_pipeline_ext_config_t));

    config->mode = PIPELINE_MODE_DUPLEX;
    config->sample_rate = 48000;
    config->channels = 1;
    config->frame_duration_ms = 20;

    config->enable_aec = true;
    config->enable_denoise = true;
    config->enable_agc = true;
    config->denoise_engine = VOICE_DENOISE_SPEEX;
    config->denoise_level = 50;

    config->codec = VOICE_CODEC_OPUS;
    config->bitrate = 32000;
    config->enable_fec = true;

    config->enable_srtp = false;
    config->srtp_profile = SRTP_PROFILE_AES128_CM_SHA1_80;

    config->jitter_min_delay_ms = 20;
    config->jitter_max_delay_ms = 200;
}

voice_pipeline_t *voice_pipeline_create(const voice_pipeline_ext_config_t *config)
{
    if (!config) {
        return NULL;
    }

    voice_pipeline_t *pipeline = (voice_pipeline_t *)calloc(1, sizeof(voice_pipeline_t));
    if (!pipeline) {
        return NULL;
    }

    pipeline->config = *config;
    pipeline->state = PIPELINE_STATE_STOPPED;
    mutex_init(&pipeline->state_mutex);

    pipeline->playback_volume = 1.0f;
    pipeline->aec_enabled = config->enable_aec;
    pipeline->denoise_enabled = config->enable_denoise;
    pipeline->agc_enabled = config->enable_agc;

    /* 计算帧大小 */
    pipeline->frame_samples = config->sample_rate * config->frame_duration_ms / 1000;

    /* 分配处理缓冲区 */
    pipeline->processing_buffer = (int16_t *)calloc(
        pipeline->frame_samples, sizeof(int16_t));
    pipeline->output_buffer = (int16_t *)calloc(
        pipeline->frame_samples, sizeof(int16_t));
    pipeline->encoded_buffer = (uint8_t *)malloc(1500);
    pipeline->decrypt_buffer = (uint8_t *)malloc(2048); /* 足够容纳 MTU 大小的包 */
    pipeline->rtp_send_buffer = (uint8_t *)malloc(2048);

    if (!pipeline->processing_buffer || !pipeline->output_buffer ||
        !pipeline->encoded_buffer || !pipeline->decrypt_buffer ||
        !pipeline->rtp_send_buffer) {
        voice_pipeline_destroy(pipeline);
        return NULL;
    }

    /* 创建环形缓冲区 */
    pipeline->capture_buffer = voice_ring_buffer_create(
        pipeline->frame_samples * 10 * sizeof(int16_t), pipeline->frame_samples * sizeof(int16_t));
    pipeline->playback_buffer = voice_ring_buffer_create(
        pipeline->frame_samples * 10 * sizeof(int16_t), pipeline->frame_samples * sizeof(int16_t));

    if (!pipeline->capture_buffer || !pipeline->playback_buffer) {
        voice_pipeline_destroy(pipeline);
        return NULL;
    }

    /* 创建降噪器 */
    if (config->enable_denoise) {
        voice_denoiser_config_t dn_config;
        voice_denoiser_config_init(&dn_config);
        dn_config.sample_rate = config->sample_rate;
        dn_config.frame_size = pipeline->frame_samples;
        dn_config.engine = config->denoise_engine;
        dn_config.denoise_enabled = true;
        dn_config.agc_enabled = config->enable_agc;

        pipeline->denoiser = voice_denoiser_create(&dn_config);
    }

    /* 创建AEC */
    if (config->enable_aec &&
        (config->mode == PIPELINE_MODE_DUPLEX ||
         config->mode == PIPELINE_MODE_LOOPBACK)) {
        voice_aec_ext_config_t aec_config;
        voice_aec_ext_config_init(&aec_config);
        aec_config.sample_rate = config->sample_rate;
        aec_config.frame_size = pipeline->frame_samples;

        pipeline->aec = voice_aec_create(&aec_config);
    }

    /* 创建编码器 */
    voice_codec_detail_config_t codec_config = {0};
    codec_config.codec_id = config->codec;

    /* 根据编解码器类型设置具体配置 */
    switch (config->codec) {
        case VOICE_CODEC_OPUS:
            codec_config.u.opus.sample_rate = config->sample_rate;
            codec_config.u.opus.channels = config->channels;
            codec_config.u.opus.bitrate = config->bitrate;
            codec_config.u.opus.application = 2048; /* OPUS_APPLICATION_VOIP */
            codec_config.u.opus.enable_fec = config->enable_fec;
            break;
        case VOICE_CODEC_G711_ALAW:
            codec_config.u.g711.sample_rate = 8000;
            codec_config.u.g711.use_alaw = true;
            break;
        case VOICE_CODEC_G711_ULAW:
            codec_config.u.g711.sample_rate = 8000;
            codec_config.u.g711.use_alaw = false;
            break;
        default:
            /* 使用默认配置 */
            break;
    }

    pipeline->encoder = voice_encoder_create(&codec_config);
    pipeline->decoder = voice_decoder_create(&codec_config);

    /* 创建 RTP 会话 */
    rtp_session_config_t rtp_config;
    rtp_session_config_init(&rtp_config);
    rtp_config.payload_type = voice_codec_get_rtp_payload_type(config->codec);
    rtp_config.clock_rate = config->sample_rate;

    pipeline->rtp_session = rtp_session_create(&rtp_config);

    /* 创建 Jitter Buffer */
    jitter_buffer_config_t jb_config;
    jitter_buffer_config_init(&jb_config);
    jb_config.clock_rate = config->sample_rate;
    jb_config.frame_duration_ms = config->frame_duration_ms;
    jb_config.min_delay_ms = config->jitter_min_delay_ms;
    jb_config.max_delay_ms = config->jitter_max_delay_ms;

    pipeline->jitter_buffer = jitter_buffer_create(&jb_config);

    /* 创建 PLC */
    voice_plc_config_t plc_config;
    voice_plc_config_init(&plc_config);
    plc_config.sample_rate = config->sample_rate;
    plc_config.frame_size = pipeline->frame_samples;

    pipeline->plc = voice_plc_create(&plc_config);

    VOICE_LOG_I("Pipeline created: %uHz, %ums, codec=%s",
        config->sample_rate, config->frame_duration_ms,
        voice_codec_get_name(config->codec));

    return pipeline;
}

void voice_pipeline_destroy(voice_pipeline_t *pipeline)
{
    if (!pipeline) return;

    voice_pipeline_stop(pipeline);

    if (pipeline->device) {
        voice_device_destroy(pipeline->device);
    }

    if (pipeline->denoiser) {
        voice_denoiser_destroy(pipeline->denoiser);
    }

    if (pipeline->aec) {
        voice_aec_destroy(pipeline->aec);
    }

    if (pipeline->encoder) {
        voice_encoder_destroy(pipeline->encoder);
    }

    if (pipeline->decoder) {
        voice_decoder_destroy(pipeline->decoder);
    }

    if (pipeline->rtp_session) {
        rtp_session_destroy(pipeline->rtp_session);
    }

    if (pipeline->srtp_send) {
        srtp_session_destroy(pipeline->srtp_send);
    }

    if (pipeline->srtp_recv) {
        srtp_session_destroy(pipeline->srtp_recv);
    }

    if (pipeline->jitter_buffer) {
        jitter_buffer_destroy(pipeline->jitter_buffer);
    }

    if (pipeline->plc) {
        voice_plc_destroy(pipeline->plc);
    }

    if (pipeline->capture_buffer) {
        voice_ring_buffer_destroy(pipeline->capture_buffer);
    }

    if (pipeline->playback_buffer) {
        voice_ring_buffer_destroy(pipeline->playback_buffer);
    }

    if (pipeline->processing_buffer) {
        free(pipeline->processing_buffer);
    }

    if (pipeline->output_buffer) {
        free(pipeline->output_buffer);
    }

    if (pipeline->encoded_buffer) {
        free(pipeline->encoded_buffer);
    }

    if (pipeline->decrypt_buffer) {
        free(pipeline->decrypt_buffer);
    }

    if (pipeline->rtp_send_buffer) {
        free(pipeline->rtp_send_buffer);
    }

    if (pipeline->resampler_cap) {
        voice_resampler_destroy(pipeline->resampler_cap);
    }

    if (pipeline->resampler_play) {
        voice_resampler_destroy(pipeline->resampler_play);
    }

    mutex_destroy(&pipeline->state_mutex);
    free(pipeline);
}

voice_error_t voice_pipeline_start(voice_pipeline_t *pipeline)
{
    if (!pipeline) {
        return VOICE_ERROR_NULL_POINTER;
    }

    mutex_lock(&pipeline->state_mutex);

    if (pipeline->state != PIPELINE_STATE_STOPPED) {
        mutex_unlock(&pipeline->state_mutex);
        return VOICE_ERROR_ALREADY_RUNNING;
    }

    pipeline->state = PIPELINE_STATE_STARTING;
    mutex_unlock(&pipeline->state_mutex);

    /* 创建音频设备 */
    voice_device_ext_config_t dev_config;
    voice_device_config_init(&dev_config);
    dev_config.sample_rate = pipeline->config.sample_rate;
    dev_config.channels = pipeline->config.channels;
    dev_config.frame_size = pipeline->frame_samples;

    switch (pipeline->config.mode) {
    case PIPELINE_MODE_CAPTURE:
        dev_config.mode = VOICE_DEVICE_MODE_CAPTURE;
        dev_config.capture_callback = pipeline_capture_callback;
        dev_config.capture_user_data = pipeline;
        break;

    case PIPELINE_MODE_PLAYBACK:
        dev_config.mode = VOICE_DEVICE_MODE_PLAYBACK;
        dev_config.playback_callback = pipeline_playback_callback;
        dev_config.playback_user_data = pipeline;
        break;

    case PIPELINE_MODE_DUPLEX:
    case PIPELINE_MODE_LOOPBACK:
        dev_config.mode = VOICE_DEVICE_MODE_DUPLEX;
        dev_config.capture_callback = pipeline_capture_callback;
        dev_config.capture_user_data = pipeline;
        dev_config.playback_callback = pipeline_playback_callback;
        dev_config.playback_user_data = pipeline;
        break;
    }

    pipeline->device = voice_device_create_simple(&dev_config);
    if (!pipeline->device) {
        pipeline_set_state(pipeline, PIPELINE_STATE_ERROR);
        return VOICE_ERROR_DEVICE_OPEN;
    }

    voice_error_t err = voice_device_start(pipeline->device);
    if (err != VOICE_OK) {
        voice_device_destroy(pipeline->device);
        pipeline->device = NULL;
        pipeline_set_state(pipeline, PIPELINE_STATE_ERROR);
        return err;
    }

    pipeline_set_state(pipeline, PIPELINE_STATE_RUNNING);

    VOICE_LOG_I("Pipeline started");
    return VOICE_OK;
}

voice_error_t voice_pipeline_stop(voice_pipeline_t *pipeline)
{
    if (!pipeline) {
        return VOICE_ERROR_NULL_POINTER;
    }

    mutex_lock(&pipeline->state_mutex);

    if (pipeline->state != PIPELINE_STATE_RUNNING) {
        mutex_unlock(&pipeline->state_mutex);
        return VOICE_OK;
    }

    pipeline->state = PIPELINE_STATE_STOPPING;
    mutex_unlock(&pipeline->state_mutex);

    if (pipeline->device) {
        voice_device_stop(pipeline->device);
        voice_device_destroy(pipeline->device);
        pipeline->device = NULL;
    }

    pipeline_set_state(pipeline, PIPELINE_STATE_STOPPED);

    VOICE_LOG_I("Pipeline stopped");
    return VOICE_OK;
}

pipeline_state_t voice_pipeline_get_state(voice_pipeline_t *pipeline)
{
    if (!pipeline) {
        return PIPELINE_STATE_STOPPED;
    }

    mutex_lock(&pipeline->state_mutex);
    pipeline_state_t state = pipeline->state;
    mutex_unlock(&pipeline->state_mutex);

    return state;
}

/* ============================================
 * 回调设置
 * ============================================ */

void voice_pipeline_set_encoded_callback(
    voice_pipeline_t *pipeline,
    pipeline_encoded_callback_t callback,
    void *user_data)
{
    if (pipeline) {
        pipeline->encoded_cb = callback;
        pipeline->encoded_cb_data = user_data;
    }
}

void voice_pipeline_set_decoded_callback(
    voice_pipeline_t *pipeline,
    pipeline_decoded_callback_t callback,
    void *user_data)
{
    if (pipeline) {
        pipeline->decoded_cb = callback;
        pipeline->decoded_cb_data = user_data;
    }
}

void voice_pipeline_set_state_callback(
    voice_pipeline_t *pipeline,
    pipeline_state_callback_t callback,
    void *user_data)
{
    if (pipeline) {
        pipeline->state_cb = callback;
        pipeline->state_cb_data = user_data;
    }
}

void voice_pipeline_set_error_callback(
    voice_pipeline_t *pipeline,
    pipeline_error_callback_t callback,
    void *user_data)
{
    if (pipeline) {
        pipeline->error_cb = callback;
        pipeline->error_cb_data = user_data;
    }
}

/* ============================================
 * 数据输入输出
 * ============================================ */

voice_error_t voice_pipeline_receive_packet(
    voice_pipeline_t *pipeline,
    const uint8_t *data,
    size_t size)
{
    if (!pipeline || !data) {
        return VOICE_ERROR_NULL_POINTER;
    }

    const uint8_t *payload = data;
    size_t payload_size = size;

    /* SRTP 解密 */
    if (pipeline->srtp_recv) {
        if (size > 2048) return VOICE_ERROR_OVERFLOW;
        memcpy(pipeline->decrypt_buffer, data, size);

        voice_error_t err = srtp_unprotect(pipeline->srtp_recv, pipeline->decrypt_buffer, &payload_size);
        if (err != VOICE_OK) {
            return err;
        }

        payload = pipeline->decrypt_buffer;
    }

    /* 解析 RTP */
    rtp_packet_t packet;
    voice_error_t err = rtp_session_parse_packet(pipeline->rtp_session, payload, payload_size, &packet);

    if (err != VOICE_OK) {
        return err;
    }

    /* 添加到 Jitter Buffer */
    err = jitter_buffer_put(
        pipeline->jitter_buffer,
        packet.payload,
        packet.payload_size,
        packet.header.timestamp,
        packet.header.sequence,
        packet.header.marker
    );

    pipeline->stats.packets_received++;

    return err;
}

voice_error_t voice_pipeline_get_stats(
    voice_pipeline_t *pipeline,
    voice_pipeline_stats_t *stats)
{
    if (!pipeline || !stats) {
        return VOICE_ERROR_NULL_POINTER;
    }

    *stats = pipeline->stats;

    /* 添加 RTP 统计 */
    if (pipeline->rtp_session) {
        rtp_statistics_t rtp_stats;
        rtp_session_get_statistics(pipeline->rtp_session, &rtp_stats);
        stats->packets_sent = rtp_stats.packets_sent;
        stats->packets_lost = rtp_stats.packets_lost_recv;
        stats->packet_loss_rate = rtp_stats.fraction_lost;
        stats->jitter_ms = rtp_stats.jitter;
        stats->rtt_ms = rtp_stats.rtt_ms;
    }

    return VOICE_OK;
}

void voice_pipeline_reset_stats(voice_pipeline_t *pipeline)
{
    if (pipeline) {
        memset(&pipeline->stats, 0, sizeof(voice_pipeline_stats_t));

        if (pipeline->rtp_session) {
            rtp_session_reset_statistics(pipeline->rtp_session);
        }

        if (pipeline->jitter_buffer) {
            jitter_buffer_reset_stats(pipeline->jitter_buffer);
        }
    }
}

/* ============================================
 * 控制函数
 * ============================================ */

voice_error_t voice_pipeline_set_aec_enabled(voice_pipeline_t *pipeline, bool enabled)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    pipeline->aec_enabled = enabled;
    if (pipeline->aec) {
        voice_aec_set_enabled(pipeline->aec, enabled);
    }
    return VOICE_OK;
}

voice_error_t voice_pipeline_set_denoise_enabled(voice_pipeline_t *pipeline, bool enabled)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    pipeline->denoise_enabled = enabled;
    if (pipeline->denoiser) {
        voice_denoiser_set_enabled(pipeline->denoiser, enabled);
    }
    return VOICE_OK;
}

voice_error_t voice_pipeline_set_denoise_level(voice_pipeline_t *pipeline, int level)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    /* TODO: 实现降噪级别设置 */
    return VOICE_OK;
}

voice_error_t voice_pipeline_set_agc_enabled(voice_pipeline_t *pipeline, bool enabled)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    pipeline->agc_enabled = enabled;
    /* TODO: 传递给降噪器的AGC */
    return VOICE_OK;
}

voice_error_t voice_pipeline_set_bitrate(voice_pipeline_t *pipeline, uint32_t bitrate)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    if (pipeline->encoder) {
        return voice_encoder_set_bitrate(pipeline->encoder, bitrate);
    }
    return VOICE_ERROR_NOT_SUPPORTED;
}

voice_error_t voice_pipeline_set_capture_muted(voice_pipeline_t *pipeline, bool muted)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    pipeline->capture_muted = muted;
    return VOICE_OK;
}

voice_error_t voice_pipeline_set_playback_muted(voice_pipeline_t *pipeline, bool muted)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    pipeline->playback_muted = muted;
    return VOICE_OK;
}

voice_error_t voice_pipeline_set_playback_volume(voice_pipeline_t *pipeline, float volume)
{
    if (!pipeline) return VOICE_ERROR_NULL_POINTER;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    pipeline->playback_volume = volume;
    return VOICE_OK;
}

/* ============================================
 * 内部函数实现
 * ============================================ */

static void pipeline_set_state(voice_pipeline_t *pipeline, pipeline_state_t state)
{
    mutex_lock(&pipeline->state_mutex);
    pipeline->state = state;
    mutex_unlock(&pipeline->state_mutex);

    if (pipeline->state_cb) {
        pipeline->state_cb(pipeline, state, pipeline->state_cb_data);
    }
}

static void pipeline_capture_callback(
    voice_device_t *device,
    const int16_t *input,
    size_t frame_count,
    void *user_data)
{
    voice_pipeline_t *pipeline = (voice_pipeline_t *)user_data;

    if (!pipeline || pipeline->state != PIPELINE_STATE_RUNNING) {
        return;
    }

    pipeline->stats.frames_captured += frame_count;

    /* 静音处理 */
    if (pipeline->capture_muted) {
        memset(pipeline->processing_buffer, 0, frame_count * sizeof(int16_t));
    } else {
        memcpy(pipeline->processing_buffer, input, frame_count * sizeof(int16_t));
    }

    /* AEC 处理 */
    if (pipeline->aec && pipeline->aec_enabled) {
        voice_aec_capture(
            pipeline->aec,
            pipeline->processing_buffer,
            pipeline->output_buffer,
            frame_count
        );
        memcpy(pipeline->processing_buffer, pipeline->output_buffer,
            frame_count * sizeof(int16_t));
    }

    /* 降噪处理 */
    if (pipeline->denoiser && pipeline->denoise_enabled) {
        voice_denoiser_process(
            pipeline->denoiser,
            pipeline->processing_buffer,
            frame_count
        );
    }

    /* 编码 */
    if (pipeline->encoder) {
        size_t encoded_size = 1500;
        voice_error_t err = voice_encoder_encode(
            pipeline->encoder,
            pipeline->processing_buffer,
            frame_count,
            pipeline->encoded_buffer,
            &encoded_size
        );

        if (err == VOICE_OK && encoded_size > 0) {
            pipeline->stats.frames_encoded++;

            /* 创建 RTP 包 */
            size_t rtp_size = 2048;
            uint8_t *rtp_packet = pipeline->rtp_send_buffer;

            err = rtp_session_create_packet(
                pipeline->rtp_session,
                pipeline->encoded_buffer,
                encoded_size,
                pipeline->rtp_timestamp,
                false,
                pipeline->rtp_send_buffer,
                &rtp_size
            );

            if (err == VOICE_OK) {
                /* SRTP 加密 */
                if (pipeline->srtp_send) {
                    size_t srtp_size = rtp_size;
                    err = srtp_protect(pipeline->srtp_send, pipeline->rtp_send_buffer, &srtp_size, 2048);
                    if (err == VOICE_OK) {
                        rtp_size = srtp_size;
                    }
                }

                /* 回调 */
                if (err == VOICE_OK && pipeline->encoded_cb) {
                    pipeline->encoded_cb(
                        pipeline,
                        rtp_packet,
                        rtp_size,
                        pipeline->rtp_timestamp,
                        pipeline->encoded_cb_data
                    );
                    pipeline->stats.packets_sent++;
                }
            }

            pipeline->rtp_timestamp += frame_count;
        }
    }

    /* 回环模式 */
    if (pipeline->config.mode == PIPELINE_MODE_LOOPBACK) {
        voice_ring_buffer_write(
            pipeline->playback_buffer,
            (const uint8_t *)pipeline->processing_buffer,
            frame_count * sizeof(int16_t)
        );
    }
}

static void pipeline_playback_callback(
    voice_device_t *device,
    int16_t *output,
    size_t frame_count,
    void *user_data)
{
    voice_pipeline_t *pipeline = (voice_pipeline_t *)user_data;

    if (!pipeline || pipeline->state != PIPELINE_STATE_RUNNING) {
        memset(output, 0, frame_count * sizeof(int16_t));
        return;
    }

    /* 从 Jitter Buffer 获取数据 */
    jitter_packet_status_t status;
    size_t packet_size = 1500;
    uint8_t packet_data[1500];

    voice_error_t err = jitter_buffer_get(
        pipeline->jitter_buffer,
        packet_data,
        sizeof(packet_data),
        &packet_size,
        &status
    );

    bool have_audio = false;

    if (err == VOICE_OK && status == JITTER_PACKET_OK && packet_size > 0) {
        /* 解码 */
        size_t decoded_samples = frame_count;
        err = voice_decoder_decode(
            pipeline->decoder,
            packet_data,
            packet_size,
            pipeline->processing_buffer,
            &decoded_samples
        );

        if (err == VOICE_OK) {
            have_audio = true;
            pipeline->stats.frames_decoded++;

            /* 更新 PLC */
            voice_plc_update_good_frame(pipeline->plc, pipeline->processing_buffer, decoded_samples);

            /* 回调 */
            if (pipeline->decoded_cb) {
                pipeline->decoded_cb(
                    pipeline,
                    pipeline->processing_buffer,
                    decoded_samples,
                    pipeline->decoded_cb_data
                );
            }
        }
    } else if (status == JITTER_PACKET_LOST) {
        /* PLC */
        size_t plc_samples = frame_count;
        err = voice_plc_generate(pipeline->plc, pipeline->processing_buffer, plc_samples);
        if (err == VOICE_OK) {
            have_audio = true;
        }
    }

    /* 输出 */
    if (have_audio && !pipeline->playback_muted) {
        /* 应用音量 */
        if (pipeline->playback_volume != 1.0f) {
            for (size_t i = 0; i < frame_count; i++) {
                int32_t sample = (int32_t)(pipeline->processing_buffer[i] * pipeline->playback_volume);
                if (sample > 32767) sample = 32767;
                if (sample < -32768) sample = -32768;
                output[i] = (int16_t)sample;
            }
        } else {
            memcpy(output, pipeline->processing_buffer, frame_count * sizeof(int16_t));
        }

        /* AEC 播放参考 */
        if (pipeline->aec && pipeline->aec_enabled) {
            voice_aec_playback(pipeline->aec, output, frame_count);
        }

        pipeline->stats.frames_played += frame_count;
    } else {
        memset(output, 0, frame_count * sizeof(int16_t));
    }
}

/* ============================================
 * SRTP Key Setup
 * ============================================ */

voice_error_t voice_pipeline_set_srtp_send_key(
    voice_pipeline_t *pipeline,
    const uint8_t *key,
    size_t key_len,
    const uint8_t *salt,
    size_t salt_len)
{
    if (!pipeline) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* TODO: 实现 SRTP 发送密钥设置 */
    /* 当启用 SONICKIT_ENABLE_SRTP 时实现完整功能 */
    (void)key;
    (void)key_len;
    (void)salt;
    (void)salt_len;

    VOICE_LOG_W("SRTP send key setup not implemented (SRTP disabled)");
    return VOICE_SUCCESS;
}

voice_error_t voice_pipeline_set_srtp_recv_key(
    voice_pipeline_t *pipeline,
    const uint8_t *key,
    size_t key_len,
    const uint8_t *salt,
    size_t salt_len)
{
    if (!pipeline) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* TODO: 实现 SRTP 接收密钥设置 */
    /* 当启用 SONICKIT_ENABLE_SRTP 时实现完整功能 */
    (void)key;
    (void)key_len;
    (void)salt;
    (void)salt_len;

    VOICE_LOG_W("SRTP recv key setup not implemented (SRTP disabled)");
    return VOICE_SUCCESS;
}
