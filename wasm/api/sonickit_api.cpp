/**
 * @file sonickit_api.cpp
 * @brief SonicKit WebAssembly API (Embind bindings)
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>
#include <cmath>

extern "C" {
#include "dsp/denoiser.h"
#include "dsp/echo_canceller.h"
#include "dsp/agc.h"
#include "dsp/vad.h"
#include "dsp/resampler.h"
#include "codec/codec.h"
#include "audio/audio_buffer.h"
#include "voice/error.h"
}

using namespace emscripten;

/* ============================================
 * Utility Functions
 * ============================================ */

static std::vector<int16_t> jsArrayToVector(const val& arr) {
    unsigned int length = arr["length"].as<unsigned int>();
    std::vector<int16_t> vec(length);

    for (unsigned int i = 0; i < length; i++) {
        vec[i] = arr[i].as<int16_t>();
    }

    return vec;
}

static val vectorToJsArray(const std::vector<int16_t>& vec) {
    val arr = val::array();
    for (size_t i = 0; i < vec.size(); i++) {
        arr.call<void>("push", vec[i]);
    }
    return arr;
}

/* ============================================
 * Denoiser Wrapper
 * ============================================ */

class WasmDenoiser {
private:
    voice_denoiser_t* denoiser;
    int frame_size;
    std::vector<int16_t> buffer;

public:
    WasmDenoiser(int sample_rate, int frame_size, int engine_type = 0)
        : denoiser(nullptr), frame_size(frame_size), buffer(frame_size) {

        voice_denoiser_config_t config;
        voice_denoiser_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.engine = static_cast<voice_denoise_engine_t>(engine_type);

        denoiser = voice_denoiser_create(&config);
        if (!denoiser) {
            throw std::runtime_error("Failed to create denoiser");
        }
    }

    ~WasmDenoiser() {
        if (denoiser) {
            voice_denoiser_destroy(denoiser);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToVector(input);
        if (vec.size() != static_cast<size_t>(frame_size)) {
            throw std::runtime_error("Invalid input size");
        }

        // voice_denoiser_process 返回 VAD 概率，不是错误码
        voice_denoiser_process(denoiser, vec.data(), vec.size());

        return vectorToJsArray(vec);
    }

    void reset() {
        if (denoiser) {
            voice_denoiser_reset(denoiser);
        }
    }
};

/* ============================================
 * Echo Canceller Wrapper
 * ============================================ */

class WasmEchoCanceller {
private:
    voice_aec_t* aec;
    int frame_size;

public:
    WasmEchoCanceller(int sample_rate, int frame_size, int filter_length = 2000)
        : aec(nullptr), frame_size(frame_size) {

        voice_aec_ext_config_t config;
        voice_aec_ext_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.filter_length = filter_length;

        aec = voice_aec_create(&config);
        if (!aec) {
            throw std::runtime_error("Failed to create echo canceller");
        }
    }

    ~WasmEchoCanceller() {
        if (aec) {
            voice_aec_destroy(aec);
        }
    }

    val process(const val& captured, const val& playback) {
        auto cap_vec = jsArrayToVector(captured);
        auto play_vec = jsArrayToVector(playback);

        if (cap_vec.size() != static_cast<size_t>(frame_size) ||
            play_vec.size() != static_cast<size_t>(frame_size)) {
            throw std::runtime_error("Invalid input size");
        }

        std::vector<int16_t> output(frame_size);
        voice_error_t err = voice_aec_process(aec, cap_vec.data(), play_vec.data(),
                                              output.data(), frame_size);
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("AEC processing failed");
        }

        return vectorToJsArray(output);
    }

    void reset() {
        if (aec) {
            voice_aec_reset(aec);
        }
    }
};

/* ============================================
 * AGC Wrapper
 * ============================================ */

class WasmAGC {
private:
    voice_agc_t* agc;
    int frame_size;

public:
    WasmAGC(int sample_rate, int frame_size, int mode = 1, float target_level = -3.0f)
        : agc(nullptr), frame_size(frame_size) {

        voice_agc_config_t config;
        voice_agc_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.mode = static_cast<voice_agc_mode_t>(mode);
        config.target_level_dbfs = target_level;

        agc = voice_agc_create(&config);
        if (!agc) {
            throw std::runtime_error("Failed to create AGC");
        }
    }

    ~WasmAGC() {
        if (agc) {
            voice_agc_destroy(agc);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToVector(input);
        if (vec.size() != static_cast<size_t>(frame_size)) {
            throw std::runtime_error("Invalid input size");
        }

        voice_error_t err = voice_agc_process(agc, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("AGC processing failed");
        }

        return vectorToJsArray(vec);
    }

    float getGain() {
        if (agc) {
            voice_agc_state_t state;
            if (voice_agc_get_state(agc, &state) == VOICE_SUCCESS) {
                // 将 dB 转换为线性增益
                return powf(10.0f, state.current_gain_db / 20.0f);
            }
        }
        return 1.0f;
    }

    void reset() {
        if (agc) {
            voice_agc_reset(agc);
        }
    }
};

/* ============================================
 * Resampler Wrapper
 * ============================================ */

class WasmResampler {
private:
    voice_resampler_t* resampler;
    int in_rate;
    int out_rate;

public:
    WasmResampler(int channels, int in_rate, int out_rate, int quality = 5)
        : resampler(nullptr), in_rate(in_rate), out_rate(out_rate) {

        resampler = voice_resampler_create(channels, in_rate, out_rate, quality);
        if (!resampler) {
            throw std::runtime_error("Failed to create resampler");
        }
    }

    ~WasmResampler() {
        if (resampler) {
            voice_resampler_destroy(resampler);
        }
    }

    val process(const val& input) {
        auto in_vec = jsArrayToVector(input);

        // 计算输出大小
        size_t output_frames = (in_vec.size() * out_rate + in_rate - 1) / in_rate;
        std::vector<int16_t> out_vec(output_frames);

        int result = voice_resampler_process_int16(resampler, in_vec.data(), in_vec.size(),
                                                   out_vec.data(), output_frames);

        if (result < 0) {
            throw std::runtime_error("Resampler processing failed");
        }

        out_vec.resize(result);
        return vectorToJsArray(out_vec);
    }

    void reset() {
        if (resampler) {
            voice_resampler_reset(resampler);
        }
    }
};

/* ============================================
 * VAD Wrapper
 * ============================================ */

class WasmVAD {
private:
    voice_vad_t* vad;
    int frame_size;
    voice_vad_result_t last_result;

public:
    WasmVAD(int sample_rate, int frame_size, int mode = 1)
        : vad(nullptr), frame_size(frame_size) {

        memset(&last_result, 0, sizeof(last_result));

        voice_vad_config_t config;
        voice_vad_config_init(&config);
        config.sample_rate = sample_rate;
        config.mode = static_cast<voice_vad_mode_t>(mode);

        vad = voice_vad_create(&config);
        if (!vad) {
            throw std::runtime_error("Failed to create VAD");
        }
    }

    ~WasmVAD() {
        if (vad) {
            voice_vad_destroy(vad);
        }
    }

    bool isSpeech(const val& input) {
        auto vec = jsArrayToVector(input);
        voice_vad_process(vad, vec.data(), vec.size(), &last_result);
        return last_result.is_speech;
    }

    float getProbability() {
        return last_result.speech_probability;
    }

    void reset() {
        if (vad) {
            voice_vad_reset(vad);
        }
        memset(&last_result, 0, sizeof(last_result));
    }
};

/* ============================================
 * G.711 Codec Wrapper
 * ============================================ */

class WasmG711Codec {
private:
    voice_encoder_t* encoder;
    voice_decoder_t* decoder;
    bool use_alaw;

public:
    WasmG711Codec(bool use_alaw = true)
        : encoder(nullptr), decoder(nullptr), use_alaw(use_alaw) {

        voice_codec_detail_config_t config;
        memset(&config, 0, sizeof(config));
        config.codec_id = use_alaw ? VOICE_CODEC_G711_ALAW : VOICE_CODEC_G711_ULAW;
        config.u.g711.sample_rate = 8000;
        config.u.g711.use_alaw = use_alaw;

        encoder = voice_encoder_create(&config);
        decoder = voice_decoder_create(&config);

        if (!encoder || !decoder) {
            if (encoder) voice_encoder_destroy(encoder);
            if (decoder) voice_decoder_destroy(decoder);
            throw std::runtime_error("Failed to create G.711 codec");
        }
    }

    ~WasmG711Codec() {
        if (encoder) voice_encoder_destroy(encoder);
        if (decoder) voice_decoder_destroy(decoder);
    }

    val encode(const val& input) {
        auto vec = jsArrayToVector(input);
        std::vector<uint8_t> encoded(vec.size());

        size_t encoded_size = encoded.size();
        voice_error_t err = voice_encoder_encode(encoder, vec.data(), vec.size(),
                                                  encoded.data(), &encoded_size);
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("G.711 encode failed");
        }

        val arr = val::array();
        for (size_t i = 0; i < encoded_size; i++) {
            arr.call<void>("push", encoded[i]);
        }
        return arr;
    }

    val decode(const val& input) {
        unsigned int length = input["length"].as<unsigned int>();
        std::vector<uint8_t> encoded(length);
        for (unsigned int i = 0; i < length; i++) {
            encoded[i] = input[i].as<uint8_t>();
        }

        std::vector<int16_t> decoded(length);
        size_t decoded_size = decoded.size();

        voice_error_t err = voice_decoder_decode(decoder, encoded.data(), length,
                                                  decoded.data(), &decoded_size);
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("G.711 decode failed");
        }

        decoded.resize(decoded_size);
        return vectorToJsArray(decoded);
    }
};

/* ============================================
 * Embind Bindings
 * ============================================ */

EMSCRIPTEN_BINDINGS(sonickit) {
    class_<WasmDenoiser>("Denoiser")
        .constructor<int, int, int>()
        .function("process", &WasmDenoiser::process)
        .function("reset", &WasmDenoiser::reset);

    class_<WasmEchoCanceller>("EchoCanceller")
        .constructor<int, int, int>()
        .function("process", &WasmEchoCanceller::process)
        .function("reset", &WasmEchoCanceller::reset);

    class_<WasmAGC>("AGC")
        .constructor<int, int, int, float>()
        .function("process", &WasmAGC::process)
        .function("getGain", &WasmAGC::getGain)
        .function("reset", &WasmAGC::reset);

    class_<WasmResampler>("Resampler")
        .constructor<int, int, int, int>()
        .function("process", &WasmResampler::process)
        .function("reset", &WasmResampler::reset);

    class_<WasmVAD>("VAD")
        .constructor<int, int, int>()
        .function("isSpeech", &WasmVAD::isSpeech)
        .function("getProbability", &WasmVAD::getProbability)
        .function("reset", &WasmVAD::reset);

    class_<WasmG711Codec>("G711Codec")
        .constructor<bool>()
        .function("encode", &WasmG711Codec::encode)
        .function("decode", &WasmG711Codec::decode);
}
