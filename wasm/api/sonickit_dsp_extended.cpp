/**
 * @file sonickit_dsp_extended.cpp
 * @brief Extended DSP bindings for SonicKit WebAssembly
 * @author SonicKit Team
 *
 * This file adds WASM bindings for:
 * - DTMF detection and generation
 * - Equalizer (multi-band parametric EQ)
 * - Compressor/Limiter/Gate
 * - Comfort Noise Generator (CNG)
 * - Delay Estimator
 * - Time Stretcher
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>

extern "C" {
#include "dsp/dtmf.h"
#include "dsp/equalizer.h"
#include "dsp/compressor.h"
#include "dsp/comfort_noise.h"
#include "dsp/delay_estimator.h"
#include "dsp/time_stretcher.h"
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

static std::vector<float> jsArrayToFloatVector(const val& arr) {
    unsigned int length = arr["length"].as<unsigned int>();
    std::vector<float> vec(length);
    for (unsigned int i = 0; i < length; i++) {
        vec[i] = arr[i].as<float>();
    }
    return vec;
}

static val floatVectorToJsArray(const std::vector<float>& vec) {
    val arr = val::array();
    for (size_t i = 0; i < vec.size(); i++) {
        arr.call<void>("push", vec[i]);
    }
    return arr;
}

/* ============================================
 * DTMF Detector Wrapper
 * ============================================ */

class WasmDTMFDetector {
private:
    voice_dtmf_detector_t* detector;
    int frame_size;
    std::string detected_digits;

public:
    WasmDTMFDetector(int sample_rate, int frame_size)
        : detector(nullptr), frame_size(frame_size) {

        voice_dtmf_detector_config_t config;
        voice_dtmf_detector_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;

        detector = voice_dtmf_detector_create(&config);
        if (!detector) {
            throw std::runtime_error("Failed to create DTMF detector");
        }
    }

    ~WasmDTMFDetector() {
        if (detector) {
            voice_dtmf_detector_destroy(detector);
        }
    }

    std::string process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);

        voice_dtmf_result_t result;
        voice_dtmf_digit_t digit = voice_dtmf_detector_process(
            detector, vec.data(), vec.size(), &result);

        if (digit != VOICE_DTMF_NONE && result.valid) {
            return std::string(1, static_cast<char>(digit));
        }
        return "";
    }

    std::string getDigits() {
        char buffer[256];
        size_t count = voice_dtmf_detector_get_digits(detector, buffer, sizeof(buffer));
        return std::string(buffer, count);
    }

    void clearDigits() {
        voice_dtmf_detector_clear_digits(detector);
    }

    void reset() {
        if (detector) {
            voice_dtmf_detector_reset(detector);
        }
    }
};

/* ============================================
 * DTMF Generator Wrapper
 * ============================================ */

class WasmDTMFGenerator {
private:
    voice_dtmf_generator_t* generator;
    int sample_rate;

public:
    WasmDTMFGenerator(int sample_rate, int tone_duration_ms = 100, int pause_duration_ms = 50)
        : generator(nullptr), sample_rate(sample_rate) {

        voice_dtmf_generator_config_t config;
        voice_dtmf_generator_config_init(&config);
        config.sample_rate = sample_rate;
        config.tone_duration_ms = tone_duration_ms;
        config.pause_duration_ms = pause_duration_ms;

        generator = voice_dtmf_generator_create(&config);
        if (!generator) {
            throw std::runtime_error("Failed to create DTMF generator");
        }
    }

    ~WasmDTMFGenerator() {
        if (generator) {
            voice_dtmf_generator_destroy(generator);
        }
    }

    val generateDigit(const std::string& digit) {
        if (digit.empty()) {
            throw std::runtime_error("Empty digit");
        }

        // Calculate buffer size based on sample rate and duration
        size_t max_samples = sample_rate; // 1 second max
        std::vector<int16_t> output(max_samples);

        size_t generated = voice_dtmf_generator_generate(
            generator,
            static_cast<voice_dtmf_digit_t>(digit[0]),
            output.data(),
            max_samples
        );

        output.resize(generated);
        return int16VectorToJsArray(output);
    }

    val generateSequence(const std::string& digits) {
        size_t max_samples = sample_rate * digits.length(); // rough estimate
        std::vector<int16_t> output(max_samples);

        size_t generated = voice_dtmf_generator_generate_sequence(
            generator,
            digits.c_str(),
            output.data(),
            max_samples
        );

        output.resize(generated);
        return int16VectorToJsArray(output);
    }

    void reset() {
        if (generator) {
            voice_dtmf_generator_reset(generator);
        }
    }
};

/* ============================================
 * Equalizer Wrapper
 * ============================================ */

class WasmEqualizer {
private:
    voice_eq_t* eq;
    int num_bands;
    std::vector<voice_eq_band_t> bands;

public:
    WasmEqualizer(int sample_rate, int num_bands = 5)
        : eq(nullptr), num_bands(num_bands), bands(num_bands) {

        voice_eq_config_t config;
        voice_eq_config_init(&config);
        config.sample_rate = sample_rate;
        config.num_bands = num_bands;

        // Initialize default bands
        for (int i = 0; i < num_bands; i++) {
            bands[i].enabled = true;
            bands[i].type = VOICE_EQ_PEAK;
            bands[i].gain_db = 0.0f;
            bands[i].q = 1.0f;
        }

        // Set default frequencies for 5-band EQ
        if (num_bands >= 5) {
            bands[0].frequency = 60.0f;    // Sub-bass
            bands[1].frequency = 250.0f;   // Bass
            bands[2].frequency = 1000.0f;  // Midrange
            bands[3].frequency = 4000.0f;  // Presence
            bands[4].frequency = 12000.0f; // Brilliance
        }

        config.bands = bands.data();

        eq = voice_eq_create(&config);
        if (!eq) {
            throw std::runtime_error("Failed to create equalizer");
        }
    }

    ~WasmEqualizer() {
        if (eq) {
            voice_eq_destroy(eq);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);

        voice_error_t err = voice_eq_process(eq, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("EQ processing failed");
        }

        return int16VectorToJsArray(vec);
    }

    void setBand(int band_index, float frequency, float gain_db, float q) {
        if (band_index < 0 || band_index >= num_bands) {
            throw std::runtime_error("Invalid band index");
        }

        voice_eq_band_t band;
        band.enabled = true;
        band.type = VOICE_EQ_PEAK;
        band.frequency = frequency;
        band.gain_db = gain_db;
        band.q = q;

        voice_error_t err = voice_eq_set_band(eq, band_index, &band);
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Failed to set EQ band");
        }
    }

    void setMasterGain(float gain_db) {
        voice_eq_set_master_gain(eq, gain_db);
    }

    void applyPreset(int preset) {
        voice_eq_apply_preset(eq, static_cast<voice_eq_preset_t>(preset));
    }

    void reset() {
        if (eq) {
            voice_eq_reset(eq);
        }
    }
};

/* ============================================
 * Compressor Wrapper
 * ============================================ */

class WasmCompressor {
private:
    voice_compressor_t* comp;

public:
    WasmCompressor(int sample_rate, float threshold_db = -20.0f,
                   float ratio = 4.0f, float attack_ms = 10.0f,
                   float release_ms = 100.0f)
        : comp(nullptr) {

        voice_compressor_config_t config;
        voice_compressor_config_init(&config);
        config.sample_rate = sample_rate;
        config.threshold_db = threshold_db;
        config.ratio = ratio;
        config.attack_ms = attack_ms;
        config.release_ms = release_ms;
        config.type = VOICE_DRC_COMPRESSOR;

        comp = voice_compressor_create(&config);
        if (!comp) {
            throw std::runtime_error("Failed to create compressor");
        }
    }

    ~WasmCompressor() {
        if (comp) {
            voice_compressor_destroy(comp);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);

        voice_error_t err = voice_compressor_process(comp, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Compressor processing failed");
        }

        return int16VectorToJsArray(vec);
    }

    void setThreshold(float threshold_db) {
        voice_compressor_set_threshold(comp, threshold_db);
    }

    void setRatio(float ratio) {
        voice_compressor_set_ratio(comp, ratio);
    }

    void setTimes(float attack_ms, float release_ms) {
        voice_compressor_set_times(comp, attack_ms, release_ms);
    }
};

/* ============================================
 * Limiter Wrapper
 * ============================================ */

class WasmLimiter {
private:
    voice_compressor_t* limiter;

public:
    WasmLimiter(int sample_rate, float threshold_db = -1.0f)
        : limiter(nullptr) {

        voice_compressor_config_t config;
        voice_limiter_config_init(&config);
        config.sample_rate = sample_rate;
        config.threshold_db = threshold_db;

        limiter = voice_compressor_create(&config);
        if (!limiter) {
            throw std::runtime_error("Failed to create limiter");
        }
    }

    ~WasmLimiter() {
        if (limiter) {
            voice_compressor_destroy(limiter);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);

        voice_error_t err = voice_compressor_process(limiter, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Limiter processing failed");
        }

        return int16VectorToJsArray(vec);
    }

    void setThreshold(float threshold_db) {
        voice_compressor_set_threshold(limiter, threshold_db);
    }
};

/* ============================================
 * Noise Gate Wrapper
 * ============================================ */

class WasmNoiseGate {
private:
    voice_compressor_t* gate;

public:
    WasmNoiseGate(int sample_rate, float threshold_db = -40.0f,
                  float attack_ms = 1.0f, float release_ms = 50.0f)
        : gate(nullptr) {

        voice_compressor_config_t config;
        voice_gate_config_init(&config);
        config.sample_rate = sample_rate;
        config.threshold_db = threshold_db;
        config.attack_ms = attack_ms;
        config.release_ms = release_ms;

        gate = voice_compressor_create(&config);
        if (!gate) {
            throw std::runtime_error("Failed to create noise gate");
        }
    }

    ~WasmNoiseGate() {
        if (gate) {
            voice_compressor_destroy(gate);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);

        voice_error_t err = voice_compressor_process(gate, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Noise gate processing failed");
        }

        return int16VectorToJsArray(vec);
    }

    void setThreshold(float threshold_db) {
        voice_compressor_set_threshold(gate, threshold_db);
    }
};

/* ============================================
 * Comfort Noise Generator Wrapper
 * ============================================ */

class WasmComfortNoise {
private:
    voice_cng_t* cng;
    int frame_size;

public:
    WasmComfortNoise(int sample_rate, int frame_size, float noise_level_db = -50.0f)
        : cng(nullptr), frame_size(frame_size) {

        voice_cng_config_t config;
        voice_cng_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.noise_level_db = noise_level_db;

        cng = voice_cng_create(&config);
        if (!cng) {
            throw std::runtime_error("Failed to create CNG");
        }
    }

    ~WasmComfortNoise() {
        if (cng) {
            voice_cng_destroy(cng);
        }
    }

    void analyze(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_cng_analyze(cng, vec.data(), vec.size());
    }

    val generate(int num_samples) {
        std::vector<int16_t> output(num_samples);

        voice_error_t err = voice_cng_generate(cng, output.data(), num_samples);
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("CNG generation failed");
        }

        return int16VectorToJsArray(output);
    }

    void setLevel(float level_db) {
        voice_cng_set_level(cng, level_db);
    }

    float getLevel() {
        return voice_cng_get_level(cng);
    }

    void reset() {
        if (cng) {
            voice_cng_reset(cng);
        }
    }
};

/* ============================================
 * Delay Estimator Wrapper
 * ============================================ */

class WasmDelayEstimator {
private:
    voice_delay_estimator_t* estimator;

public:
    WasmDelayEstimator(int sample_rate, int frame_size, int max_delay_ms = 500)
        : estimator(nullptr) {

        voice_delay_estimator_config_t config;
        voice_delay_estimator_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.max_delay_samples = (sample_rate * max_delay_ms) / 1000;

        estimator = voice_delay_estimator_create(&config);
        if (!estimator) {
            throw std::runtime_error("Failed to create delay estimator");
        }
    }

    ~WasmDelayEstimator() {
        if (estimator) {
            voice_delay_estimator_destroy(estimator);
        }
    }

    val estimate(const val& reference, const val& capture) {
        auto ref_vec = jsArrayToInt16Vector(reference);
        auto cap_vec = jsArrayToInt16Vector(capture);

        voice_delay_estimate_t result;
        voice_error_t err = voice_delay_estimator_estimate(
            estimator, ref_vec.data(), cap_vec.data(), ref_vec.size(), &result);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Delay estimation failed");
        }

        val obj = val::object();
        obj.set("delaySamples", result.delay_samples);
        obj.set("delayMs", result.delay_ms);
        obj.set("confidence", result.confidence);
        obj.set("valid", result.valid);

        return obj;
    }

    float getDelayMs() {
        return voice_delay_estimator_get_delay_ms(estimator);
    }

    bool isStable() {
        return voice_delay_estimator_is_stable(estimator);
    }

    void reset() {
        if (estimator) {
            voice_delay_estimator_reset(estimator);
        }
    }
};

/* ============================================
 * Time Stretcher Wrapper
 * ============================================ */

class WasmTimeStretcher {
private:
    voice_time_stretcher_t* stretcher;

public:
    WasmTimeStretcher(int sample_rate, int channels = 1, float initial_rate = 1.0f)
        : stretcher(nullptr) {

        voice_time_stretcher_config_t config;
        voice_time_stretcher_config_init(&config);
        config.sample_rate = sample_rate;
        config.channels = channels;
        config.initial_rate = initial_rate;

        stretcher = voice_time_stretcher_create(&config);
        if (!stretcher) {
            throw std::runtime_error("Failed to create time stretcher");
        }
    }

    ~WasmTimeStretcher() {
        if (stretcher) {
            voice_time_stretcher_destroy(stretcher);
        }
    }

    val process(const val& input) {
        auto in_vec = jsArrayToInt16Vector(input);

        // Calculate max output size (2x for slowdown)
        size_t max_output = in_vec.size() * 2 + 1024;
        std::vector<int16_t> output(max_output);
        size_t output_count = 0;

        voice_error_t err = voice_time_stretcher_process(
            stretcher,
            in_vec.data(), in_vec.size(),
            output.data(), max_output,
            &output_count
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Time stretch processing failed");
        }

        output.resize(output_count);
        return int16VectorToJsArray(output);
    }

    void setRate(float rate) {
        if (rate < 0.5f || rate > 2.0f) {
            throw std::runtime_error("Rate must be between 0.5 and 2.0");
        }
        voice_time_stretcher_set_rate(stretcher, rate);
    }

    float getRate() {
        return voice_time_stretcher_get_rate(stretcher);
    }

    void reset() {
        if (stretcher) {
            voice_time_stretcher_reset(stretcher);
        }
    }
};

/* ============================================
 * Embind Bindings
 * ============================================ */

EMSCRIPTEN_BINDINGS(sonickit_dsp_extended) {
    // DTMF
    class_<WasmDTMFDetector>("DTMFDetector")
        .constructor<int, int>()
        .function("process", &WasmDTMFDetector::process)
        .function("getDigits", &WasmDTMFDetector::getDigits)
        .function("clearDigits", &WasmDTMFDetector::clearDigits)
        .function("reset", &WasmDTMFDetector::reset);

    class_<WasmDTMFGenerator>("DTMFGenerator")
        .constructor<int, int, int>()
        .function("generateDigit", &WasmDTMFGenerator::generateDigit)
        .function("generateSequence", &WasmDTMFGenerator::generateSequence)
        .function("reset", &WasmDTMFGenerator::reset);

    // Equalizer
    class_<WasmEqualizer>("Equalizer")
        .constructor<int, int>()
        .function("process", &WasmEqualizer::process)
        .function("setBand", &WasmEqualizer::setBand)
        .function("setMasterGain", &WasmEqualizer::setMasterGain)
        .function("applyPreset", &WasmEqualizer::applyPreset)
        .function("reset", &WasmEqualizer::reset);

    // Dynamics
    class_<WasmCompressor>("Compressor")
        .constructor<int, float, float, float, float>()
        .function("process", &WasmCompressor::process)
        .function("setThreshold", &WasmCompressor::setThreshold)
        .function("setRatio", &WasmCompressor::setRatio)
        .function("setTimes", &WasmCompressor::setTimes);

    class_<WasmLimiter>("Limiter")
        .constructor<int, float>()
        .function("process", &WasmLimiter::process)
        .function("setThreshold", &WasmLimiter::setThreshold);

    class_<WasmNoiseGate>("NoiseGate")
        .constructor<int, float, float, float>()
        .function("process", &WasmNoiseGate::process)
        .function("setThreshold", &WasmNoiseGate::setThreshold);

    // Comfort Noise
    class_<WasmComfortNoise>("ComfortNoise")
        .constructor<int, int, float>()
        .function("analyze", &WasmComfortNoise::analyze)
        .function("generate", &WasmComfortNoise::generate)
        .function("setLevel", &WasmComfortNoise::setLevel)
        .function("getLevel", &WasmComfortNoise::getLevel)
        .function("reset", &WasmComfortNoise::reset);

    // Delay Estimator
    class_<WasmDelayEstimator>("DelayEstimator")
        .constructor<int, int, int>()
        .function("estimate", &WasmDelayEstimator::estimate)
        .function("getDelayMs", &WasmDelayEstimator::getDelayMs)
        .function("isStable", &WasmDelayEstimator::isStable)
        .function("reset", &WasmDelayEstimator::reset);

    // Time Stretcher
    class_<WasmTimeStretcher>("TimeStretcher")
        .constructor<int, int, float>()
        .function("process", &WasmTimeStretcher::process)
        .function("setRate", &WasmTimeStretcher::setRate)
        .function("getRate", &WasmTimeStretcher::getRate)
        .function("reset", &WasmTimeStretcher::reset);

    // EQ Presets enum
    enum_<voice_eq_preset_t>("EQPreset")
        .value("FLAT", VOICE_EQ_PRESET_FLAT)
        .value("VOICE_ENHANCE", VOICE_EQ_PRESET_VOICE_ENHANCE)
        .value("TELEPHONE", VOICE_EQ_PRESET_TELEPHONE)
        .value("BASS_BOOST", VOICE_EQ_PRESET_BASS_BOOST)
        .value("TREBLE_BOOST", VOICE_EQ_PRESET_TREBLE_BOOST)
        .value("REDUCE_NOISE", VOICE_EQ_PRESET_REDUCE_NOISE)
        .value("CLARITY", VOICE_EQ_PRESET_CLARITY);
}
