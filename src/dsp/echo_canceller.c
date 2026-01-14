/**
 * @file echo_canceller.c
 * @brief Acoustic Echo Cancellation (AEC) implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 *
 * Stub implementation when SpeexDSP is not available.
 */

#include "dsp/echo_canceller.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* ============================================
 * 内部结构
 * ============================================ */

struct voice_aec_s {
    voice_aec_ext_config_t config;
    bool enabled;

    /* 播放缓冲区 (用于异步模式) */
    int16_t *playback_buffer;
    size_t playback_buffer_size;
    size_t playback_write_pos;
    size_t playback_read_pos;
};

/* ============================================
 * API Implementation
 * ============================================ */

void voice_aec_ext_config_init(voice_aec_ext_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));
    config->sample_rate = 16000;
    config->frame_size = 160;        /* 10ms @ 16kHz */
    config->filter_length = 2560;    /* 160ms @ 16kHz */
    config->echo_suppress_db = -40;
    config->echo_suppress_active_db = -15;
    config->enable_residual_echo_suppress = true;
    config->enable_comfort_noise = false;
}

voice_aec_t *voice_aec_create(const voice_aec_ext_config_t *config) {
    if (!config) return NULL;

    voice_aec_t *aec = (voice_aec_t *)calloc(1, sizeof(voice_aec_t));
    if (!aec) return NULL;

    aec->config = *config;
    aec->enabled = true;

    /* 分配播放缓冲区 (约 500ms) */
    aec->playback_buffer_size = config->sample_rate / 2;
    aec->playback_buffer = (int16_t *)calloc(aec->playback_buffer_size, sizeof(int16_t));
    if (!aec->playback_buffer) {
        free(aec);
        return NULL;
    }

    aec->playback_write_pos = 0;
    aec->playback_read_pos = 0;

    return aec;
}

void voice_aec_destroy(voice_aec_t *aec) {
    if (!aec) return;

    if (aec->playback_buffer) {
        free(aec->playback_buffer);
    }

    free(aec);
}

voice_error_t voice_aec_process(
    voice_aec_t *aec,
    const int16_t *mic_input,
    const int16_t *speaker_ref,
    int16_t *output,
    size_t frame_count
) {
    if (!aec || !mic_input || !output) return VOICE_ERROR_NULL_POINTER;

    (void)speaker_ref;  /* Unused in stub */

    if (!aec->enabled) {
        /* 直接复制输入到输出 */
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }

    /* Stub: 直接复制输入到输出 (无实际回声消除) */
    memcpy(output, mic_input, frame_count * sizeof(int16_t));

    return VOICE_OK;
}

voice_error_t voice_aec_playback(
    voice_aec_t *aec,
    const int16_t *speaker_data,
    size_t frame_count
) {
    if (!aec || !speaker_data) return VOICE_ERROR_NULL_POINTER;

    /* 存储播放数据用于后续处理 */
    for (size_t i = 0; i < frame_count && i < aec->playback_buffer_size; i++) {
        aec->playback_buffer[aec->playback_write_pos] = speaker_data[i];
        aec->playback_write_pos = (aec->playback_write_pos + 1) % aec->playback_buffer_size;
    }

    return VOICE_OK;
}

voice_error_t voice_aec_capture(
    voice_aec_t *aec,
    const int16_t *mic_input,
    int16_t *output,
    size_t frame_count
) {
    if (!aec || !mic_input || !output) return VOICE_ERROR_NULL_POINTER;

    if (!aec->enabled) {
        memcpy(output, mic_input, frame_count * sizeof(int16_t));
        return VOICE_OK;
    }

    /* Stub: 直接复制输入到输出 */
    memcpy(output, mic_input, frame_count * sizeof(int16_t));

    return VOICE_OK;
}

voice_error_t voice_aec_set_suppress(
    voice_aec_t *aec,
    int suppress_db,
    int suppress_active_db
) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->config.echo_suppress_db = suppress_db;
    aec->config.echo_suppress_active_db = suppress_active_db;

    return VOICE_OK;
}

voice_error_t voice_aec_set_enabled(voice_aec_t *aec, bool enabled) {
    if (!aec) return VOICE_ERROR_NULL_POINTER;

    aec->enabled = enabled;

    return VOICE_OK;
}

bool voice_aec_is_enabled(voice_aec_t *aec) {
    if (!aec) return false;

    return aec->enabled;
}

void voice_aec_reset(voice_aec_t *aec) {
    if (!aec) return;

    /* 重置播放缓冲区 */
    if (aec->playback_buffer) {
        memset(aec->playback_buffer, 0, aec->playback_buffer_size * sizeof(int16_t));
    }
    aec->playback_write_pos = 0;
    aec->playback_read_pos = 0;
}

int voice_aec_get_delay(voice_aec_t *aec) {
    if (!aec) return 0;

    /* Stub: 返回估计延迟 */
    return (int)aec->config.frame_size * 2;
}
