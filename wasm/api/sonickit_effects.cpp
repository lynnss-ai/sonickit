/**
 * @file sonickit_effects.cpp
 * @brief Audio effects bindings for SonicKit WebAssembly
 * @author SonicKit Team
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>

extern "C" {
#include "dsp/effects.h"
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

/* ============================================
 * Reverb Wrapper
 * ============================================ */

class WasmReverb {
private:
    voice_reverb_t* reverb;

public:
    WasmReverb(int sample_rate, float room_size = 0.5f, float damping = 0.5f,
               float wet_level = 0.3f, float dry_level = 0.7f)
        : reverb(nullptr) {

        voice_reverb_config_t config;
        voice_reverb_config_init(&config);
        config.sample_rate = sample_rate;
        config.room_size = room_size;
        config.damping = damping;
        config.wet_level = wet_level;
        config.dry_level = dry_level;

        reverb = voice_reverb_create(&config);
        if (!reverb) {
            throw std::runtime_error("Failed to create reverb");
        }
    }

    ~WasmReverb() {
        if (reverb) {
            voice_reverb_destroy(reverb);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_error_t err = voice_reverb_process_int16(reverb, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Reverb processing failed");
        }
        return int16VectorToJsArray(vec);
    }

    void setRoomSize(float size) {
        voice_reverb_set_room_size(reverb, size);
    }

    void setDamping(float damping) {
        voice_reverb_set_damping(reverb, damping);
    }

    void setWetLevel(float level) {
        voice_reverb_set_wet_level(reverb, level);
    }

    void setDryLevel(float level) {
        voice_reverb_set_dry_level(reverb, level);
    }

    void reset() {
        voice_reverb_reset(reverb);
    }
};

/* ============================================
 * Delay Wrapper
 * ============================================ */

class WasmDelay {
private:
    voice_delay_t* delay;

public:
    WasmDelay(int sample_rate, float delay_ms = 200.0f, float feedback = 0.3f, float mix = 0.5f)
        : delay(nullptr) {

        voice_delay_config_t config;
        voice_delay_config_init(&config);
        config.sample_rate = sample_rate;
        config.delay_ms = delay_ms;
        config.feedback = feedback;
        config.mix = mix;

        delay = voice_delay_create(&config);
        if (!delay) {
            throw std::runtime_error("Failed to create delay");
        }
    }

    ~WasmDelay() {
        if (delay) {
            voice_delay_destroy(delay);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_error_t err = voice_delay_process_int16(delay, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Delay processing failed");
        }
        return int16VectorToJsArray(vec);
    }

    void setDelayTime(float delay_ms) {
        voice_delay_set_time(delay, delay_ms);
    }

    void setFeedback(float feedback) {
        voice_delay_set_feedback(delay, feedback);
    }

    void reset() {
        voice_delay_reset(delay);
    }
};

/* ============================================
 * Pitch Shifter Wrapper
 * ============================================ */

class WasmPitchShifter {
private:
    voice_pitch_shift_t* shifter;

public:
    WasmPitchShifter(int sample_rate, float shift_semitones = 0.0f)
        : shifter(nullptr) {

        voice_pitch_shift_config_t config;
        voice_pitch_shift_config_init(&config);
        config.sample_rate = sample_rate;
        config.semitones = shift_semitones;

        shifter = voice_pitch_shift_create(&config);
        if (!shifter) {
            throw std::runtime_error("Failed to create pitch shifter");
        }
    }

    ~WasmPitchShifter() {
        if (shifter) {
            voice_pitch_shift_destroy(shifter);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_error_t err = voice_pitch_shift_process_int16(shifter, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Pitch shift processing failed");
        }
        return int16VectorToJsArray(vec);
    }

    void setShift(float semitones) {
        voice_pitch_shift_set_shift(shifter, semitones, 0.0f);
    }

    void reset() {
        voice_pitch_shift_reset(shifter);
    }
};

/* ============================================
 * Chorus Wrapper
 * ============================================ */

class WasmChorus {
private:
    voice_chorus_t* chorus;

public:
    WasmChorus(int sample_rate, float rate_hz = 1.5f, float depth = 0.5f, float mix = 0.5f)
        : chorus(nullptr) {

        voice_chorus_config_t config;
        voice_chorus_config_init(&config);
        config.sample_rate = sample_rate;
        config.rate = rate_hz;
        config.depth = depth;
        config.mix = mix;

        chorus = voice_chorus_create(&config);
        if (!chorus) {
            throw std::runtime_error("Failed to create chorus");
        }
    }

    ~WasmChorus() {
        if (chorus) {
            voice_chorus_destroy(chorus);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_error_t err = voice_chorus_process_int16(chorus, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Chorus processing failed");
        }
        return int16VectorToJsArray(vec);
    }

    void reset() {
        voice_chorus_reset(chorus);
    }
};

/* ============================================
 * Flanger Wrapper
 * ============================================ */

class WasmFlanger {
private:
    voice_flanger_t* flanger;

public:
    WasmFlanger(int sample_rate, float rate_hz = 0.5f, float depth = 0.5f,
                float feedback = 0.5f, float mix = 0.5f)
        : flanger(nullptr) {

        voice_flanger_config_t config;
        voice_flanger_config_init(&config);
        config.sample_rate = sample_rate;
        config.rate = rate_hz;
        config.depth = depth;
        config.feedback = feedback;
        config.mix = mix;

        flanger = voice_flanger_create(&config);
        if (!flanger) {
            throw std::runtime_error("Failed to create flanger");
        }
    }

    ~WasmFlanger() {
        if (flanger) {
            voice_flanger_destroy(flanger);
        }
    }

    val process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_error_t err = voice_flanger_process_int16(flanger, vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Flanger processing failed");
        }
        return int16VectorToJsArray(vec);
    }

    void reset() {
        voice_flanger_reset(flanger);
    }
};

/* ============================================
 * Emscripten Bindings
 * ============================================ */

EMSCRIPTEN_BINDINGS(sonickit_effects) {
    // Reverb
    class_<WasmReverb>("Reverb")
        .constructor<int, float, float, float, float>()
        .function("process", &WasmReverb::process)
        .function("setRoomSize", &WasmReverb::setRoomSize)
        .function("setDamping", &WasmReverb::setDamping)
        .function("setWetLevel", &WasmReverb::setWetLevel)
        .function("setDryLevel", &WasmReverb::setDryLevel)
        .function("reset", &WasmReverb::reset);

    // Delay
    class_<WasmDelay>("Delay")
        .constructor<int, float, float, float>()
        .function("process", &WasmDelay::process)
        .function("setDelayTime", &WasmDelay::setDelayTime)
        .function("setFeedback", &WasmDelay::setFeedback)
        .function("reset", &WasmDelay::reset);

    // Pitch Shifter
    class_<WasmPitchShifter>("PitchShifter")
        .constructor<int, float>()
        .function("process", &WasmPitchShifter::process)
        .function("setShift", &WasmPitchShifter::setShift)
        .function("reset", &WasmPitchShifter::reset);

    // Chorus
    class_<WasmChorus>("Chorus")
        .constructor<int, float, float, float>()
        .function("process", &WasmChorus::process)
        .function("reset", &WasmChorus::reset);

    // Flanger
    class_<WasmFlanger>("Flanger")
        .constructor<int, float, float, float, float>()
        .function("process", &WasmFlanger::process)
        .function("reset", &WasmFlanger::reset);
}
