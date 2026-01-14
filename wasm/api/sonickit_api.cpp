/**
 * @file sonickit_api.cpp
 * @brief SonicKit WebAssembly JavaScript API（使用 Emscripten Embind）
 * @author wangxuebing <lynnss.codeai@gmail.com>
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <memory>
#include <stdexcept>

extern "C" {
    #include "dsp/denoiser.h"
    #include "dsp/echo_canceller.h"
    #include "dsp/agc.h"
    #include "dsp/vad.h"
    #include "dsp/resampler.h"
    #include "dsp/equalizer.h"
    #include "dsp/compressor.h"
    #include "dsp/dtmf.h"
    #include "codec/codec.h"
    #include "audio/audio_buffer.h"
    #include "audio/audio_mixer.h"
    #include "audio/audio_quality.h"
    #include "audio/audio_level.h"
}

using namespace emscripten;

/* ============================================
 * 辅助函数
 * ============================================ */

// 将 JavaScript Int16Array 转换为 C++ vector
std::vector<int16_t> jsArrayToVector(const val& jsArray) {
    unsigned int length = jsArray["length"].as<unsigned int>();
    std::vector<int16_t> vec(length);
    
    val memory = val::module_property("HEAP16");
    val buffer = memory["buffer"];
    
    // 使用 JavaScript TypedArray.set() 方法
    for (unsigned int i = 0; i < length; i++) {
        vec[i] = jsArray[i].as<int16_t>();
    }
    
    return vec;
}

// 将 C++ vector 转换为 JavaScript Int16Array
val vectorToJsArray(const std::vector<int16_t>& vec) {
    val Int16Array = val::global("Int16Array");
    val jsArray = Int16Array.new_(vec.size());
    
    for (size_t i = 0; i < vec.size(); i++) {
        jsArray.set(i, vec[i]);
    }
    
    return jsArray;
}

/* ============================================
 * 降噪器封装
 * ============================================ */

class WasmDenoiser {
private:
    voice_denoiser_t* denoiser;
    int frame_size;
    
public:
    WasmDenoiser(int sample_rate, int frame_size, int engine_type = 0) 
        : denoiser(nullptr), frame_size(frame_size) {
        
        voice_denoiser_config_t config;
        voice_denoiser_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.engine = static_cast<voice_denoiser_engine_t>(engine_type);
        
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
        
        voice_error_t err = voice_denoiser_process(denoiser, vec.data(), vec.size());
        if (err != VOICE_ERROR_NONE) {
            throw std::runtime_error("Denoiser processing failed");
        }
        
        return vectorToJsArray(vec);
    }
    
    void reset() {
        if (denoiser) {
            voice_denoiser_reset(denoiser);
        }
    }
};

/* ============================================
 * 回声消除器封装
 * ============================================ */

class WasmEchoCanceller {
private:
    voice_aec_t* aec;
    int frame_size;
    
public:
    WasmEchoCanceller(int sample_rate, int frame_size, int filter_length = 200) 
        : aec(nullptr), frame_size(frame_size) {
        
        voice_aec_config_t config;
        voice_aec_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.filter_length_ms = filter_length;
        
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
        if (err != VOICE_ERROR_NONE) {
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
 * AGC 封装
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
        if (err != VOICE_ERROR_NONE) {
            throw std::runtime_error("AGC processing failed");
        }
        
        return vectorToJsArray(vec);
    }
    
    void setTargetLevel(float level_dbfs) {
        if (agc) {
            voice_agc_set_target_level(agc, level_dbfs);
        }
    }
    
    float getGain() {
        if (agc) {
            return voice_agc_get_gain(agc);
        }
        return 1.0f;
    }
};

/* ============================================
 * 重采样器封装
 * ============================================ */

class WasmResampler {
private:
    voice_resampler_t* resampler;
    int in_rate;
    int out_rate;
    int channels;
    
public:
    WasmResampler(int input_rate, int output_rate, int num_channels = 1, int quality = 5) 
        : resampler(nullptr), in_rate(input_rate), out_rate(output_rate), channels(num_channels) {
        
        voice_resampler_config_t config;
        voice_resampler_config_init(&config);
        config.input_rate = input_rate;
        config.output_rate = output_rate;
        config.channels = num_channels;
        config.quality = quality;
        
        resampler = voice_resampler_create(&config);
        if (!resampler) {
            throw std::runtime_error("Failed to create resampler");
        }
    }
    
    ~WasmResampler() {
        if (resampler) {
            voice_resampler_destroy(resampler);
        }
    }
    
    val process(const val& input, int input_frames) {
        auto in_vec = jsArrayToVector(input);
        
        // 计算输出大小
        int output_frames = (input_frames * out_rate) / in_rate + 10; // 加一些余量
        std::vector<int16_t> output(output_frames * channels);
        
        int actual_output;
        voice_error_t err = voice_resampler_process(resampler, in_vec.data(), input_frames,
                                                     output.data(), output_frames, &actual_output);
        if (err != VOICE_ERROR_NONE) {
            throw std::runtime_error("Resampler processing failed");
        }
        
        output.resize(actual_output * channels);
        return vectorToJsArray(output);
    }
    
    void reset() {
        if (resampler) {
            voice_resampler_reset(resampler);
        }
    }
};

/* ============================================
 * VAD 封装
 * ============================================ */

class WasmVAD {
private:
    voice_vad_t* vad;
    int frame_size;
    
public:
    WasmVAD(int sample_rate, int frame_size, int mode = 1) 
        : vad(nullptr), frame_size(frame_size) {
        
        voice_vad_config_t config;
        voice_vad_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
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
        if (vec.size() != static_cast<size_t>(frame_size)) {
            throw std::runtime_error("Invalid input size");
        }
        
        return voice_vad_is_speech(vad, vec.data(), vec.size());
    }
    
    float getProbability() {
        if (vad) {
            return voice_vad_get_probability(vad);
        }
        return 0.0f;
    }
};

/* ============================================
 * Opus 编码器封装
 * ============================================ */

#ifdef SONICKIT_HAVE_OPUS

class WasmOpusEncoder {
private:
    voice_encoder_t* encoder;
    std::vector<uint8_t> output_buffer;
    
public:
    WasmOpusEncoder(int sample_rate, int channels, int bitrate = 64000) 
        : encoder(nullptr), output_buffer(4000) {
        
        voice_encoder_config_t config;
        voice_encoder_config_init(&config);
        config.codec = VOICE_CODEC_OPUS;
        config.sample_rate = sample_rate;
        config.channels = channels;
        config.bitrate = bitrate;
        
        encoder = voice_encoder_create(&config);
        if (!encoder) {
            throw std::runtime_error("Failed to create Opus encoder");
        }
    }
    
    ~WasmOpusEncoder() {
        if (encoder) {
            voice_encoder_destroy(encoder);
        }
    }
    
    val encode(const val& input, int num_samples) {
        auto vec = jsArrayToVector(input);
        
        int encoded_size = voice_encoder_encode(encoder, vec.data(), num_samples,
                                                output_buffer.data(), output_buffer.size());
        if (encoded_size < 0) {
            throw std::runtime_error("Encoding failed");
        }
        
        val Uint8Array = val::global("Uint8Array");
        val result = Uint8Array.new_(encoded_size);
        for (int i = 0; i < encoded_size; i++) {
            result.set(i, output_buffer[i]);
        }
        
        return result;
    }
    
    void setBitrate(int bitrate) {
        if (encoder) {
            voice_encoder_set_bitrate(encoder, bitrate);
        }
    }
};

class WasmOpusDecoder {
private:
    voice_decoder_t* decoder;
    std::vector<int16_t> output_buffer;
    
public:
    WasmOpusDecoder(int sample_rate, int channels) 
        : decoder(nullptr), output_buffer(5760 * channels) { // 120ms @ 48kHz
        
        voice_decoder_config_t config;
        voice_decoder_config_init(&config);
        config.codec = VOICE_CODEC_OPUS;
        config.sample_rate = sample_rate;
        config.channels = channels;
        
        decoder = voice_decoder_create(&config);
        if (!decoder) {
            throw std::runtime_error("Failed to create Opus decoder");
        }
    }
    
    ~WasmOpusDecoder() {
        if (decoder) {
            voice_decoder_destroy(decoder);
        }
    }
    
    val decode(const val& input, int input_size) {
        std::vector<uint8_t> in_vec(input_size);
        for (int i = 0; i < input_size; i++) {
            in_vec[i] = input[i].as<uint8_t>();
        }
        
        int decoded_samples = voice_decoder_decode(decoder, in_vec.data(), input_size,
                                                    output_buffer.data(), output_buffer.size());
        if (decoded_samples < 0) {
            throw std::runtime_error("Decoding failed");
        }
        
        output_buffer.resize(decoded_samples);
        return vectorToJsArray(output_buffer);
    }
};

#endif // SONICKIT_HAVE_OPUS

/* ============================================
 * Embind 绑定
 * ============================================ */

EMSCRIPTEN_BINDINGS(sonickit) {
    // 降噪器
    class_<WasmDenoiser>("Denoiser")
        .constructor<int, int, int>()
        .function("process", &WasmDenoiser::process)
        .function("reset", &WasmDenoiser::reset);
    
    // 回声消除
    class_<WasmEchoCanceller>("EchoCanceller")
        .constructor<int, int, int>()
        .function("process", &WasmEchoCanceller::process)
        .function("reset", &WasmEchoCanceller::reset);
    
    // AGC
    class_<WasmAGC>("AGC")
        .constructor<int, int, int, float>()
        .function("process", &WasmAGC::process)
        .function("setTargetLevel", &WasmAGC::setTargetLevel)
        .function("getGain", &WasmAGC::getGain);
    
    // 重采样器
    class_<WasmResampler>("Resampler")
        .constructor<int, int, int, int>()
        .function("process", &WasmResampler::process)
        .function("reset", &WasmResampler::reset);
    
    // VAD
    class_<WasmVAD>("VAD")
        .constructor<int, int, int>()
        .function("isSpeech", &WasmVAD::isSpeech)
        .function("getProbability", &WasmVAD::getProbability);
    
#ifdef SONICKIT_HAVE_OPUS
    // Opus 编码器
    class_<WasmOpusEncoder>("OpusEncoder")
        .constructor<int, int, int>()
        .function("encode", &WasmOpusEncoder::encode)
        .function("setBitrate", &WasmOpusEncoder::setBitrate);
    
    // Opus 解码器
    class_<WasmOpusDecoder>("OpusDecoder")
        .constructor<int, int>()
        .function("decode", &WasmOpusDecoder::decode);
#endif
}
