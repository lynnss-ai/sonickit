/**
 * @file sonickit_watermark.cpp
 * @brief Audio watermarking bindings for SonicKit WebAssembly
 * @author SonicKit Team
 *
 * This file adds WASM bindings for:
 * - Watermark Embedder (embed hidden data in audio)
 * - Watermark Detector (extract hidden data from audio)
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>

extern "C" {
#include "dsp/watermark.h"
#include "voice/error.h"
}

using namespace emscripten;

/* ============================================
 * Utility Functions
 * ============================================ */

static std::vector<int16_t> jsArrayToInt16Vector(const val& arr) {
    unsigned int length = arr["length"].as<unsigned int>();
    std::vector<int16_t> vec(length);
    for (unsigned int i = 0; i < length; i++) {
        vec[i] = arr[i].as<int16_t>();
    }
    return vec;
}

static val int16VectorToJsArray(const std::vector<int16_t>& vec) {
    val arr = val::array();
    for (size_t i = 0; i < vec.size(); i++) {
        arr.call<void>("push", vec[i]);
    }
    return arr;
}

static std::vector<uint8_t> jsArrayToUint8Vector(const val& arr) {
    unsigned int length = arr["length"].as<unsigned int>();
    std::vector<uint8_t> vec(length);
    for (unsigned int i = 0; i < length; i++) {
        vec[i] = arr[i].as<uint8_t>();
    }
    return vec;
}

static val uint8VectorToJsArray(const std::vector<uint8_t>& vec) {
    val arr = val::array();
    for (size_t i = 0; i < vec.size(); i++) {
        arr.call<void>("push", vec[i]);
    }
    return arr;
}

/* ============================================
 * Watermark Embedder Wrapper
 * ============================================ */

class WasmWatermarkEmbedder {
private:
    voice_watermark_embedder_t* embedder;
    voice_watermark_embedder_config_t config;

public:
    /**
     * @brief Create a watermark embedder for hiding data in audio
     * @param sample_rate Audio sample rate in Hz
     * @param strength Embedding strength: 0=LOW, 1=MEDIUM, 2=HIGH
     * @param seed Secret key for watermark (default: 12345)
     */
    WasmWatermarkEmbedder(int sample_rate, int strength = 1, uint32_t seed = 12345)
        : embedder(nullptr) {

        voice_watermark_embedder_config_init(&config);
        config.sample_rate = sample_rate;
        config.seed = seed;

        // Map strength level
        if (strength <= 0) {
            config.strength = VOICE_WATERMARK_STRENGTH_LOW;
        } else if (strength >= 2) {
            config.strength = VOICE_WATERMARK_STRENGTH_HIGH;
        } else {
            config.strength = VOICE_WATERMARK_STRENGTH_MEDIUM;
        }

        embedder = voice_watermark_embedder_create(&config);
        if (!embedder) {
            throw std::runtime_error("Failed to create watermark embedder");
        }
    }

    ~WasmWatermarkEmbedder() {
        if (embedder) {
            voice_watermark_embedder_destroy(embedder);
        }
    }

    /**
     * @brief Set payload data to embed
     * @param data Payload bytes (max 256 bytes)
     */
    bool setPayload(const val& data) {
        auto payload = jsArrayToUint8Vector(data);
        if (payload.size() > VOICE_WATERMARK_MAX_PAYLOAD_SIZE) {
            return false;
        }

        voice_error_t err = voice_watermark_embedder_set_payload(
            embedder, payload.data(), payload.size());
        return err == VOICE_SUCCESS;
    }

    /**
     * @brief Set payload from a string
     * @param message String message to embed
     */
    bool setPayloadString(const std::string& message) {
        if (message.size() > VOICE_WATERMARK_MAX_PAYLOAD_SIZE) {
            return false;
        }

        voice_error_t err = voice_watermark_embedder_set_payload(
            embedder,
            reinterpret_cast<const uint8_t*>(message.c_str()),
            message.size());
        return err == VOICE_SUCCESS;
    }

    /**
     * @brief Embed watermark into audio samples
     * @param audio Input audio samples (int16)
     * @return Watermarked audio samples
     */
    val embed(const val& audio) {
        auto audio_vec = jsArrayToInt16Vector(audio);

        voice_error_t err = voice_watermark_embed_int16(
            embedder,
            audio_vec.data(),
            audio_vec.size()
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Watermark embedding failed");
        }

        return int16VectorToJsArray(audio_vec);
    }

    /**
     * @brief Get number of bits embedded so far
     */
    size_t getBitsEmbedded() {
        return voice_watermark_embedder_get_bits_embedded(embedder);
    }

    void reset() {
        if (embedder) {
            voice_watermark_embedder_reset(embedder);
        }
    }
};

/* ============================================
 * Watermark Detector Wrapper
 * ============================================ */

class WasmWatermarkDetector {
private:
    voice_watermark_detector_t* detector;
    voice_watermark_result_t last_result;

public:
    /**
     * @brief Create a watermark detector for extracting hidden data
     * @param sample_rate Audio sample rate in Hz
     * @param seed Secret key (must match embedder's seed)
     */
    WasmWatermarkDetector(int sample_rate, uint32_t seed = 12345)
        : detector(nullptr) {

        memset(&last_result, 0, sizeof(last_result));

        voice_watermark_detector_config_t config;
        voice_watermark_detector_config_init(&config);
        config.sample_rate = sample_rate;
        config.seed = seed;

        detector = voice_watermark_detector_create(&config);
        if (!detector) {
            throw std::runtime_error("Failed to create watermark detector");
        }
    }

    ~WasmWatermarkDetector() {
        if (detector) {
            voice_watermark_detector_destroy(detector);
        }
    }

    /**
     * @brief Process audio samples for watermark detection
     * @param audio Input audio samples (int16)
     * @return Detection result object
     */
    val detect(const val& audio) {
        auto audio_vec = jsArrayToInt16Vector(audio);

        voice_error_t err = voice_watermark_detect_int16(
            detector,
            audio_vec.data(),
            audio_vec.size(),
            &last_result
        );

        val obj = val::object();
        obj.set("detected", last_result.detected);
        obj.set("confidence", last_result.confidence);
        obj.set("correlation", last_result.correlation);
        obj.set("snrDb", last_result.snr_estimate_db);

        if (last_result.detected && last_result.payload_size > 0) {
            std::vector<uint8_t> payload(
                last_result.payload,
                last_result.payload + last_result.payload_size
            );
            obj.set("payload", uint8VectorToJsArray(payload));

            // Try to interpret as string
            std::string message(
                reinterpret_cast<const char*>(last_result.payload),
                last_result.payload_size
            );
            obj.set("message", message);
        } else {
            obj.set("payload", val::array());
            obj.set("message", std::string(""));
        }

        return obj;
    }

    /**
     * @brief Get the last detection result
     */
    val getResult() {
        voice_watermark_detector_get_result(detector, &last_result);

        val obj = val::object();
        obj.set("detected", last_result.detected);
        obj.set("confidence", last_result.confidence);
        obj.set("correlation", last_result.correlation);
        obj.set("snrDb", last_result.snr_estimate_db);

        if (last_result.detected && last_result.payload_size > 0) {
            std::vector<uint8_t> payload(
                last_result.payload,
                last_result.payload + last_result.payload_size
            );
            obj.set("payload", uint8VectorToJsArray(payload));
        } else {
            obj.set("payload", val::array());
        }

        return obj;
    }

    /**
     * @brief Check if detection is in progress
     */
    bool isDetecting() {
        return voice_watermark_detector_is_detecting(detector);
    }

    void reset() {
        if (detector) {
            voice_watermark_detector_reset(detector);
            memset(&last_result, 0, sizeof(last_result));
        }
    }
};

/* ============================================
 * Embind Bindings
 * ============================================ */

EMSCRIPTEN_BINDINGS(sonickit_watermark) {
    // Watermark Embedder
    class_<WasmWatermarkEmbedder>("WatermarkEmbedder")
        .constructor<int, int, uint32_t>()
        .function("setPayload", &WasmWatermarkEmbedder::setPayload)
        .function("setPayloadString", &WasmWatermarkEmbedder::setPayloadString)
        .function("embed", &WasmWatermarkEmbedder::embed)
        .function("getBitsEmbedded", &WasmWatermarkEmbedder::getBitsEmbedded)
        .function("reset", &WasmWatermarkEmbedder::reset);

    // Watermark Detector
    class_<WasmWatermarkDetector>("WatermarkDetector")
        .constructor<int, uint32_t>()
        .function("detect", &WasmWatermarkDetector::detect)
        .function("getResult", &WasmWatermarkDetector::getResult)
        .function("isDetecting", &WasmWatermarkDetector::isDetecting)
        .function("reset", &WasmWatermarkDetector::reset);

    // Watermark strength enum
    enum_<voice_watermark_strength_t>("WatermarkStrength")
        .value("LOW", VOICE_WATERMARK_STRENGTH_LOW)
        .value("MEDIUM", VOICE_WATERMARK_STRENGTH_MEDIUM)
        .value("HIGH", VOICE_WATERMARK_STRENGTH_HIGH);

    // Watermark algorithm enum
    enum_<voice_watermark_algorithm_t>("WatermarkAlgorithm")
        .value("SPREAD_SPECTRUM", VOICE_WATERMARK_SPREAD_SPECTRUM)
        .value("ECHO_HIDING", VOICE_WATERMARK_ECHO_HIDING)
        .value("PHASE_CODING", VOICE_WATERMARK_PHASE_CODING)
        .value("QUANTIZATION", VOICE_WATERMARK_QUANTIZATION);
}
