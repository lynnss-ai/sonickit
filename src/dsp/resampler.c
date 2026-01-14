/**
 * @file resampler.c
 * @brief SpeexDSP resampler wrapper implementation
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include "dsp/resampler.h"
#include <stdlib.h>
#include <string.h>

#ifdef SONICKIT_HAVE_SPEEXDSP

#include <speex/speex_resampler.h>

struct voice_resampler_s {
    SpeexResamplerState *state;
    uint8_t channels;
    uint32_t in_rate;
    uint32_t out_rate;
    int quality;
    bool initialized;
};

voice_resampler_t *voice_resampler_create(
    uint8_t channels,
    uint32_t in_rate,
    uint32_t out_rate,
    int quality)
{
    if (channels == 0 || in_rate == 0 || out_rate == 0) {
        return NULL;
    }

    if (quality < 0) quality = 0;
    if (quality > 10) quality = 10;

    voice_resampler_t *rs = (voice_resampler_t *)calloc(1, sizeof(voice_resampler_t));
    if (!rs) {
        return NULL;
    }

    int err;
    rs->state = speex_resampler_init(channels, in_rate, out_rate, quality, &err);
    if (err != RESAMPLER_ERR_SUCCESS || !rs->state) {
        free(rs);
        return NULL;
    }

    rs->channels = channels;
    rs->in_rate = in_rate;
    rs->out_rate = out_rate;
    rs->quality = quality;
    rs->initialized = true;

    return rs;
}

void voice_resampler_destroy(voice_resampler_t *rs)
{
    if (rs) {
        if (rs->state) {
            speex_resampler_destroy(rs->state);
        }
        free(rs);
    }
}

int voice_resampler_process_int16(
    voice_resampler_t *rs,
    const int16_t *in,
    size_t in_frames,
    int16_t *out,
    size_t out_frames)
{
    if (!rs || !rs->initialized || !in || !out) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    spx_uint32_t in_len = (spx_uint32_t)in_frames;
    spx_uint32_t out_len = (spx_uint32_t)out_frames;

    int err;
    if (rs->channels == 1) {
        err = speex_resampler_process_int(rs->state, 0, in, &in_len, out, &out_len);
    } else {
        err = speex_resampler_process_interleaved_int(rs->state, in, &in_len, out, &out_len);
    }

    if (err != RESAMPLER_ERR_SUCCESS) {
        return VOICE_ERROR_DSP_RESAMPLE_FAILED;
    }

    return (int)out_len;
}

int voice_resampler_process_float(
    voice_resampler_t *rs,
    const float *in,
    size_t in_frames,
    float *out,
    size_t out_frames)
{
    if (!rs || !rs->initialized || !in || !out) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    spx_uint32_t in_len = (spx_uint32_t)in_frames;
    spx_uint32_t out_len = (spx_uint32_t)out_frames;

    int err;
    if (rs->channels == 1) {
        err = speex_resampler_process_float(rs->state, 0, in, &in_len, out, &out_len);
    } else {
        err = speex_resampler_process_interleaved_float(rs->state, in, &in_len, out, &out_len);
    }

    if (err != RESAMPLER_ERR_SUCCESS) {
        return VOICE_ERROR_DSP_RESAMPLE_FAILED;
    }

    return (int)out_len;
}

voice_error_t voice_resampler_set_rate(
    voice_resampler_t *rs,
    uint32_t in_rate,
    uint32_t out_rate)
{
    if (!rs || !rs->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (in_rate == 0 || out_rate == 0) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    int err = speex_resampler_set_rate(rs->state, in_rate, out_rate);
    if (err != RESAMPLER_ERR_SUCCESS) {
        return VOICE_ERROR_DSP_RESAMPLE_FAILED;
    }

    rs->in_rate = in_rate;
    rs->out_rate = out_rate;

    return VOICE_OK;
}

voice_error_t voice_resampler_set_quality(voice_resampler_t *rs, int quality)
{
    if (!rs || !rs->initialized) {
        return VOICE_ERROR_NOT_INITIALIZED;
    }

    if (quality < 0) quality = 0;
    if (quality > 10) quality = 10;

    int err = speex_resampler_set_quality(rs->state, quality);
    if (err != RESAMPLER_ERR_SUCCESS) {
        return VOICE_ERROR_DSP_RESAMPLE_FAILED;
    }

    rs->quality = quality;

    return VOICE_OK;
}

int voice_resampler_get_input_latency(voice_resampler_t *rs)
{
    if (!rs || !rs->initialized) {
        return 0;
    }
    return speex_resampler_get_input_latency(rs->state);
}

int voice_resampler_get_output_latency(voice_resampler_t *rs)
{
    if (!rs || !rs->initialized) {
        return 0;
    }
    return speex_resampler_get_output_latency(rs->state);
}

void voice_resampler_reset(voice_resampler_t *rs)
{
    if (rs && rs->initialized && rs->state) {
        speex_resampler_reset_mem(rs->state);
    }
}

size_t voice_resampler_get_output_frames(voice_resampler_t *rs, size_t in_frames)
{
    if (!rs || !rs->initialized || rs->in_rate == 0) {
        return 0;
    }
    return (in_frames * rs->out_rate + rs->in_rate - 1) / rs->in_rate;
}

size_t voice_resampler_get_input_frames(voice_resampler_t *rs, size_t out_frames)
{
    if (!rs || !rs->initialized || rs->out_rate == 0) {
        return 0;
    }
    return (out_frames * rs->in_rate + rs->out_rate - 1) / rs->out_rate;
}

#else /* !SONICKIT_HAVE_SPEEXDSP - Stub implementation */

/* Simple linear interpolation resampler stub */
struct voice_resampler_s {
    uint8_t channels;
    uint32_t in_rate;
    uint32_t out_rate;
    int quality;
    bool initialized;
};

voice_resampler_t *voice_resampler_create(
    uint8_t channels,
    uint32_t in_rate,
    uint32_t out_rate,
    int quality)
{
    if (channels == 0 || in_rate == 0 || out_rate == 0) {
        return NULL;
    }

    voice_resampler_t *rs = (voice_resampler_t *)calloc(1, sizeof(voice_resampler_t));
    if (!rs) {
        return NULL;
    }

    rs->channels = channels;
    rs->in_rate = in_rate;
    rs->out_rate = out_rate;
    rs->quality = quality;
    rs->initialized = true;

    return rs;
}

void voice_resampler_destroy(voice_resampler_t *rs)
{
    free(rs);
}

int voice_resampler_process_int16(
    voice_resampler_t *rs,
    const int16_t *in,
    size_t in_frames,
    int16_t *out,
    size_t out_frames)
{
    if (!rs || !rs->initialized || !in || !out) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    /* Simple linear interpolation */
    double ratio = (double)rs->in_rate / (double)rs->out_rate;
    size_t actual_out = 0;

    for (size_t i = 0; i < out_frames && actual_out < out_frames; i++) {
        double src_idx = i * ratio;
        size_t idx0 = (size_t)src_idx;

        if (idx0 >= in_frames - 1) {
            idx0 = in_frames - 1;
        }

        for (uint8_t ch = 0; ch < rs->channels; ch++) {
            out[i * rs->channels + ch] = in[idx0 * rs->channels + ch];
        }
        actual_out++;
    }

    return (int)actual_out;
}

int voice_resampler_process_float(
    voice_resampler_t *rs,
    const float *in,
    size_t in_frames,
    float *out,
    size_t out_frames)
{
    if (!rs || !rs->initialized || !in || !out) {
        return VOICE_ERROR_INVALID_PARAM;
    }

    double ratio = (double)rs->in_rate / (double)rs->out_rate;
    size_t actual_out = 0;

    for (size_t i = 0; i < out_frames && actual_out < out_frames; i++) {
        double src_idx = i * ratio;
        size_t idx0 = (size_t)src_idx;

        if (idx0 >= in_frames - 1) {
            idx0 = in_frames - 1;
        }

        for (uint8_t ch = 0; ch < rs->channels; ch++) {
            out[i * rs->channels + ch] = in[idx0 * rs->channels + ch];
        }
        actual_out++;
    }

    return (int)actual_out;
}

voice_error_t voice_resampler_set_rate(voice_resampler_t *rs, uint32_t in_rate, uint32_t out_rate)
{
    if (!rs || !rs->initialized) return VOICE_ERROR_NOT_INITIALIZED;
    rs->in_rate = in_rate;
    rs->out_rate = out_rate;
    return VOICE_OK;
}

voice_error_t voice_resampler_set_quality(voice_resampler_t *rs, int quality)
{
    if (!rs || !rs->initialized) return VOICE_ERROR_NOT_INITIALIZED;
    rs->quality = quality;
    return VOICE_OK;
}

int voice_resampler_get_input_latency(voice_resampler_t *rs) { (void)rs; return 0; }
int voice_resampler_get_output_latency(voice_resampler_t *rs) { (void)rs; return 0; }
void voice_resampler_reset(voice_resampler_t *rs) { (void)rs; }

size_t voice_resampler_get_output_frames(voice_resampler_t *rs, size_t in_frames)
{
    if (!rs || !rs->initialized || rs->in_rate == 0) return 0;
    return (in_frames * rs->out_rate + rs->in_rate - 1) / rs->in_rate;
}

size_t voice_resampler_get_input_frames(voice_resampler_t *rs, size_t out_frames)
{
    if (!rs || !rs->initialized || rs->out_rate == 0) return 0;
    return (out_frames * rs->in_rate + rs->out_rate - 1) / rs->out_rate;
}

#endif /* SONICKIT_HAVE_SPEEXDSP */
