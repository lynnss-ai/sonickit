/**
 * @file effects_bindings.cpp
 * @brief Audio effects Python bindings (Reverb, Delay, Chorus, Flanger, etc.)
 * @author SonicKit Team
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <vector>
#include <cstring>

extern "C" {
#include "dsp/effects.h"
#include "dsp/watermark.h"
#include "dsp/time_stretcher.h"
#include "voice/error.h"
}

namespace py = pybind11;

/* ============================================
 * Reverb Wrapper
 * ============================================ */

class PyReverb {
private:
    voice_reverb_t* reverb_;

public:
    PyReverb(int sample_rate, float room_size = 0.5f, float damping = 0.5f,
             float wet_level = 0.3f, float dry_level = 0.7f)
        : reverb_(nullptr) {

        voice_reverb_config_t config;
        voice_reverb_config_init(&config);
        config.sample_rate = sample_rate;
        config.room_size = room_size;
        config.damping = damping;
        config.wet_level = wet_level;
        config.dry_level = dry_level;

        reverb_ = voice_reverb_create(&config);
        if (!reverb_) {
            throw std::runtime_error("Failed to create reverb");
        }
    }

    ~PyReverb() {
        if (reverb_) {
            voice_reverb_destroy(reverb_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_reverb_process(
            reverb_, static_cast<int16_t*>(out_buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Reverb processing failed");
        }

        return output;
    }

    void set_room_size(float room_size) {
        voice_reverb_set_room_size(reverb_, room_size);
    }

    void set_damping(float damping) {
        voice_reverb_set_damping(reverb_, damping);
    }

    void set_wet_level(float level) {
        voice_reverb_set_wet_level(reverb_, level);
    }

    void set_dry_level(float level) {
        voice_reverb_set_dry_level(reverb_, level);
    }

    void set_preset(int preset) {
        voice_reverb_set_preset(reverb_, static_cast<voice_reverb_preset_t>(preset));
    }

    void reset() {
        voice_reverb_reset(reverb_);
    }
};

/* ============================================
 * Delay Wrapper
 * ============================================ */

class PyDelay {
private:
    voice_delay_t* delay_;

public:
    PyDelay(int sample_rate, float delay_ms = 250.0f,
            float feedback = 0.4f, float mix = 0.5f)
        : delay_(nullptr) {

        voice_delay_config_t config;
        voice_delay_config_init(&config);
        config.sample_rate = sample_rate;
        config.delay_ms = delay_ms;
        config.feedback = feedback;
        config.mix = mix;

        delay_ = voice_delay_create(&config);
        if (!delay_) {
            throw std::runtime_error("Failed to create delay");
        }
    }

    ~PyDelay() {
        if (delay_) {
            voice_delay_destroy(delay_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_delay_process(
            delay_, static_cast<int16_t*>(out_buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Delay processing failed");
        }

        return output;
    }

    void set_delay_time(float delay_ms) {
        voice_delay_set_delay(delay_, delay_ms);
    }

    void set_feedback(float feedback) {
        voice_delay_set_feedback(delay_, feedback);
    }

    void set_mix(float mix) {
        voice_delay_set_mix(delay_, mix);
    }

    void reset() {
        voice_delay_reset(delay_);
    }
};

/* ============================================
 * Pitch Shifter Wrapper
 * ============================================ */

class PyPitchShifter {
private:
    voice_pitch_shift_t* shifter_;

public:
    PyPitchShifter(int sample_rate, float shift_semitones = 0.0f)
        : shifter_(nullptr) {

        voice_pitch_shift_config_t config;
        voice_pitch_shift_config_init(&config);
        config.sample_rate = sample_rate;
        config.shift_semitones = shift_semitones;

        shifter_ = voice_pitch_shift_create(&config);
        if (!shifter_) {
            throw std::runtime_error("Failed to create pitch shifter");
        }
    }

    ~PyPitchShifter() {
        if (shifter_) {
            voice_pitch_shift_destroy(shifter_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();

        voice_error_t err = voice_pitch_shift_process(
            shifter_,
            static_cast<int16_t*>(buf.ptr),
            static_cast<int16_t*>(out_buf.ptr),
            buf.size
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Pitch shift processing failed");
        }

        return output;
    }

    void set_shift(float semitones) {
        voice_pitch_shift_set_shift(shifter_, semitones);
    }

    float get_shift() const {
        return voice_pitch_shift_get_shift(shifter_);
    }

    void reset() {
        voice_pitch_shift_reset(shifter_);
    }
};

/* ============================================
 * Chorus Wrapper
 * ============================================ */

class PyChorus {
private:
    voice_chorus_t* chorus_;

public:
    PyChorus(int sample_rate, float rate = 1.5f,
             float depth = 0.5f, float mix = 0.5f)
        : chorus_(nullptr) {

        voice_chorus_config_t config;
        voice_chorus_config_init(&config);
        config.sample_rate = sample_rate;
        config.rate = rate;
        config.depth = depth;
        config.mix = mix;

        chorus_ = voice_chorus_create(&config);
        if (!chorus_) {
            throw std::runtime_error("Failed to create chorus");
        }
    }

    ~PyChorus() {
        if (chorus_) {
            voice_chorus_destroy(chorus_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_chorus_process(
            chorus_, static_cast<int16_t*>(out_buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Chorus processing failed");
        }

        return output;
    }

    void set_rate(float rate) {
        voice_chorus_set_rate(chorus_, rate);
    }

    void set_depth(float depth) {
        voice_chorus_set_depth(chorus_, depth);
    }

    void set_mix(float mix) {
        voice_chorus_set_mix(chorus_, mix);
    }

    void reset() {
        voice_chorus_reset(chorus_);
    }
};

/* ============================================
 * Flanger Wrapper
 * ============================================ */

class PyFlanger {
private:
    voice_flanger_t* flanger_;

public:
    PyFlanger(int sample_rate, float rate = 0.5f, float depth = 0.5f,
              float feedback = 0.5f, float mix = 0.5f)
        : flanger_(nullptr) {

        voice_flanger_config_t config;
        voice_flanger_config_init(&config);
        config.sample_rate = sample_rate;
        config.rate = rate;
        config.depth = depth;
        config.feedback = feedback;
        config.mix = mix;

        flanger_ = voice_flanger_create(&config);
        if (!flanger_) {
            throw std::runtime_error("Failed to create flanger");
        }
    }

    ~PyFlanger() {
        if (flanger_) {
            voice_flanger_destroy(flanger_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_flanger_process(
            flanger_, static_cast<int16_t*>(out_buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Flanger processing failed");
        }

        return output;
    }

    void set_rate(float rate) {
        voice_flanger_set_rate(flanger_, rate);
    }

    void set_depth(float depth) {
        voice_flanger_set_depth(flanger_, depth);
    }

    void set_feedback(float feedback) {
        voice_flanger_set_feedback(flanger_, feedback);
    }

    void set_mix(float mix) {
        voice_flanger_set_mix(flanger_, mix);
    }

    void reset() {
        voice_flanger_reset(flanger_);
    }
};

/* ============================================
 * Time Stretcher Wrapper
 * ============================================ */

class PyTimeStretcher {
private:
    voice_time_stretcher_t* stretcher_;

public:
    PyTimeStretcher(int sample_rate, int channels = 1, float initial_rate = 1.0f)
        : stretcher_(nullptr) {

        voice_time_stretcher_config_t config;
        voice_time_stretcher_config_init(&config);
        config.sample_rate = sample_rate;
        config.channels = channels;
        config.initial_rate = initial_rate;

        stretcher_ = voice_time_stretcher_create(&config);
        if (!stretcher_) {
            throw std::runtime_error("Failed to create time stretcher");
        }
    }

    ~PyTimeStretcher() {
        if (stretcher_) {
            voice_time_stretcher_destroy(stretcher_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        size_t max_output = buf.size * 2 + 1024;
        std::vector<int16_t> output_buffer(max_output);
        size_t output_count = 0;

        voice_error_t err = voice_time_stretcher_process(
            stretcher_,
            static_cast<int16_t*>(buf.ptr), buf.size,
            output_buffer.data(), max_output,
            &output_count
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Time stretch processing failed");
        }

        py::array_t<int16_t> output(output_count);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, output_buffer.data(),
                   output_count * sizeof(int16_t));

        return output;
    }

    void set_rate(float rate) {
        if (rate < 0.5f || rate > 2.0f) {
            throw std::runtime_error("Rate must be between 0.5 and 2.0");
        }
        voice_time_stretcher_set_rate(stretcher_, rate);
    }

    float get_rate() const {
        return voice_time_stretcher_get_rate(stretcher_);
    }

    void reset() {
        voice_time_stretcher_reset(stretcher_);
    }
};

/* ============================================
 * Watermark Embedder Wrapper
 * ============================================ */

class PyWatermarkEmbedder {
private:
    voice_watermark_embedder_t* embedder_;

public:
    PyWatermarkEmbedder(int sample_rate, float strength = 0.1f)
        : embedder_(nullptr) {

        voice_watermark_embedder_config_t config;
        voice_watermark_embedder_config_init(&config);
        config.sample_rate = sample_rate;
        config.strength = strength;

        embedder_ = voice_watermark_embedder_create(&config);
        if (!embedder_) {
            throw std::runtime_error("Failed to create watermark embedder");
        }
    }

    ~PyWatermarkEmbedder() {
        if (embedder_) {
            voice_watermark_embedder_destroy(embedder_);
        }
    }

    py::array_t<int16_t> embed_string(py::array_t<int16_t> audio,
                                       const std::string& message) {
        py::buffer_info buf = audio.request();

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_watermark_embed_string(
            embedder_,
            static_cast<int16_t*>(out_buf.ptr),
            buf.size,
            message.c_str()
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Watermark embedding failed");
        }

        return output;
    }

    py::array_t<int16_t> embed_bytes(py::array_t<int16_t> audio,
                                      py::array_t<uint8_t> data) {
        py::buffer_info audio_buf = audio.request();
        py::buffer_info data_buf = data.request();

        py::array_t<int16_t> output(audio_buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, audio_buf.ptr, audio_buf.size * sizeof(int16_t));

        voice_error_t err = voice_watermark_embed_bytes(
            embedder_,
            static_cast<int16_t*>(out_buf.ptr),
            audio_buf.size,
            static_cast<uint8_t*>(data_buf.ptr),
            data_buf.size
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Watermark embedding failed");
        }

        return output;
    }

    void set_strength(float strength) {
        voice_watermark_embedder_set_strength(embedder_, strength);
    }

    void reset() {
        voice_watermark_embedder_reset(embedder_);
    }
};

/* ============================================
 * Watermark Detector Wrapper
 * ============================================ */

class PyWatermarkDetector {
private:
    voice_watermark_detector_t* detector_;

public:
    PyWatermarkDetector(int sample_rate)
        : detector_(nullptr) {

        voice_watermark_detector_config_t config;
        voice_watermark_detector_config_init(&config);
        config.sample_rate = sample_rate;

        detector_ = voice_watermark_detector_create(&config);
        if (!detector_) {
            throw std::runtime_error("Failed to create watermark detector");
        }
    }

    ~PyWatermarkDetector() {
        if (detector_) {
            voice_watermark_detector_destroy(detector_);
        }
    }

    py::dict detect_string(py::array_t<int16_t> audio) {
        py::buffer_info buf = audio.request();
        char buffer[1024];

        voice_watermark_detect_result_t result;
        voice_watermark_detect_string(
            detector_,
            static_cast<int16_t*>(buf.ptr),
            buf.size,
            buffer,
            sizeof(buffer),
            &result
        );

        py::dict ret;
        ret["detected"] = result.detected;
        ret["confidence"] = result.confidence;
        ret["message"] = result.detected ? std::string(buffer) : "";

        return ret;
    }

    bool has_watermark(py::array_t<int16_t> audio) {
        py::buffer_info buf = audio.request();
        return voice_watermark_has_watermark(
            detector_, static_cast<int16_t*>(buf.ptr), buf.size);
    }

    void reset() {
        voice_watermark_detector_reset(detector_);
    }
};

/* ============================================
 * Module Initialization
 * ============================================ */

void init_effects_bindings(py::module_& m) {
    // Reverb
    py::class_<PyReverb>(m, "Reverb", R"pbdoc(
        Room reverb effect.

        Args:
            sample_rate: Audio sample rate in Hz
            room_size: Room size (0.0 = small, 1.0 = large)
            damping: High frequency damping (0.0 = bright, 1.0 = dark)
            wet_level: Wet signal level (0.0 - 1.0)
            dry_level: Dry signal level (0.0 - 1.0)

        Example:
            >>> reverb = Reverb(48000, room_size=0.7, wet_level=0.3)
            >>> wet_audio = reverb.process(audio)
    )pbdoc")
        .def(py::init<int, float, float, float, float>(),
             py::arg("sample_rate"),
             py::arg("room_size") = 0.5f,
             py::arg("damping") = 0.5f,
             py::arg("wet_level") = 0.3f,
             py::arg("dry_level") = 0.7f)
        .def("process", &PyReverb::process, py::arg("input"),
             "Process audio through reverb")
        .def("set_room_size", &PyReverb::set_room_size, py::arg("room_size"))
        .def("set_damping", &PyReverb::set_damping, py::arg("damping"))
        .def("set_wet_level", &PyReverb::set_wet_level, py::arg("level"))
        .def("set_dry_level", &PyReverb::set_dry_level, py::arg("level"))
        .def("set_preset", &PyReverb::set_preset, py::arg("preset"))
        .def("reset", &PyReverb::reset, "Reset reverb state");

    // Delay
    py::class_<PyDelay>(m, "Delay", R"pbdoc(
        Echo/delay effect.

        Args:
            sample_rate: Audio sample rate in Hz
            delay_ms: Delay time in milliseconds
            feedback: Feedback amount (0.0 - 1.0)
            mix: Wet/dry mix (0.0 = dry only, 1.0 = wet only)

        Example:
            >>> delay = Delay(48000, delay_ms=300, feedback=0.4)
            >>> delayed = delay.process(audio)
    )pbdoc")
        .def(py::init<int, float, float, float>(),
             py::arg("sample_rate"),
             py::arg("delay_ms") = 250.0f,
             py::arg("feedback") = 0.4f,
             py::arg("mix") = 0.5f)
        .def("process", &PyDelay::process, py::arg("input"),
             "Process audio through delay")
        .def("set_delay_time", &PyDelay::set_delay_time, py::arg("delay_ms"))
        .def("set_feedback", &PyDelay::set_feedback, py::arg("feedback"))
        .def("set_mix", &PyDelay::set_mix, py::arg("mix"))
        .def("reset", &PyDelay::reset, "Reset delay state");

    // Pitch Shifter
    py::class_<PyPitchShifter>(m, "PitchShifter", R"pbdoc(
        Pitch shifter effect.

        Args:
            sample_rate: Audio sample rate in Hz
            shift_semitones: Pitch shift in semitones (-12 to +12)

        Example:
            >>> shifter = PitchShifter(48000, shift_semitones=5)
            >>> higher = shifter.process(audio)
    )pbdoc")
        .def(py::init<int, float>(),
             py::arg("sample_rate"),
             py::arg("shift_semitones") = 0.0f)
        .def("process", &PyPitchShifter::process, py::arg("input"),
             "Process audio through pitch shifter")
        .def("set_shift", &PyPitchShifter::set_shift, py::arg("semitones"),
             "Set pitch shift in semitones")
        .def("get_shift", &PyPitchShifter::get_shift,
             "Get current pitch shift")
        .def("reset", &PyPitchShifter::reset, "Reset pitch shifter state");

    // Chorus
    py::class_<PyChorus>(m, "Chorus", R"pbdoc(
        Chorus effect for thickening sound.

        Args:
            sample_rate: Audio sample rate in Hz
            rate: Modulation rate in Hz
            depth: Modulation depth (0.0 - 1.0)
            mix: Wet/dry mix (0.0 - 1.0)

        Example:
            >>> chorus = Chorus(48000, rate=1.5, depth=0.5)
            >>> thick = chorus.process(audio)
    )pbdoc")
        .def(py::init<int, float, float, float>(),
             py::arg("sample_rate"),
             py::arg("rate") = 1.5f,
             py::arg("depth") = 0.5f,
             py::arg("mix") = 0.5f)
        .def("process", &PyChorus::process, py::arg("input"),
             "Process audio through chorus")
        .def("set_rate", &PyChorus::set_rate, py::arg("rate"))
        .def("set_depth", &PyChorus::set_depth, py::arg("depth"))
        .def("set_mix", &PyChorus::set_mix, py::arg("mix"))
        .def("reset", &PyChorus::reset, "Reset chorus state");

    // Flanger
    py::class_<PyFlanger>(m, "Flanger", R"pbdoc(
        Flanger effect for modulation.

        Args:
            sample_rate: Audio sample rate in Hz
            rate: Modulation rate in Hz
            depth: Modulation depth (0.0 - 1.0)
            feedback: Feedback amount (-1.0 to 1.0)
            mix: Wet/dry mix (0.0 - 1.0)

        Example:
            >>> flanger = Flanger(48000, rate=0.5, depth=0.7)
            >>> flanged = flanger.process(audio)
    )pbdoc")
        .def(py::init<int, float, float, float, float>(),
             py::arg("sample_rate"),
             py::arg("rate") = 0.5f,
             py::arg("depth") = 0.5f,
             py::arg("feedback") = 0.5f,
             py::arg("mix") = 0.5f)
        .def("process", &PyFlanger::process, py::arg("input"),
             "Process audio through flanger")
        .def("set_rate", &PyFlanger::set_rate, py::arg("rate"))
        .def("set_depth", &PyFlanger::set_depth, py::arg("depth"))
        .def("set_feedback", &PyFlanger::set_feedback, py::arg("feedback"))
        .def("set_mix", &PyFlanger::set_mix, py::arg("mix"))
        .def("reset", &PyFlanger::reset, "Reset flanger state");

    // Time Stretcher
    py::class_<PyTimeStretcher>(m, "TimeStretcher", R"pbdoc(
        Time stretcher for changing tempo without pitch change.

        Args:
            sample_rate: Audio sample rate in Hz
            channels: Number of audio channels
            initial_rate: Initial time stretch rate (1.0 = normal)

        Example:
            >>> stretcher = TimeStretcher(48000)
            >>> stretcher.set_rate(0.75)  # Slow down to 75%
            >>> slower = stretcher.process(audio)
    )pbdoc")
        .def(py::init<int, int, float>(),
             py::arg("sample_rate"),
             py::arg("channels") = 1,
             py::arg("initial_rate") = 1.0f)
        .def("process", &PyTimeStretcher::process, py::arg("input"),
             "Process audio through time stretcher")
        .def("set_rate", &PyTimeStretcher::set_rate, py::arg("rate"),
             "Set time stretch rate (0.5 - 2.0)")
        .def("get_rate", &PyTimeStretcher::get_rate,
             "Get current time stretch rate")
        .def("reset", &PyTimeStretcher::reset, "Reset time stretcher state");

    // Watermark Embedder
    py::class_<PyWatermarkEmbedder>(m, "WatermarkEmbedder", R"pbdoc(
        Audio watermark embedder for hiding data in audio.

        Args:
            sample_rate: Audio sample rate in Hz
            strength: Watermark strength (0.0 = inaudible, 1.0 = strong)

        Example:
            >>> embedder = WatermarkEmbedder(48000, strength=0.1)
            >>> marked = embedder.embed_string(audio, "Copyright 2024")
    )pbdoc")
        .def(py::init<int, float>(),
             py::arg("sample_rate"),
             py::arg("strength") = 0.1f)
        .def("embed_string", &PyWatermarkEmbedder::embed_string,
             py::arg("audio"), py::arg("message"),
             "Embed a string message as watermark")
        .def("embed_bytes", &PyWatermarkEmbedder::embed_bytes,
             py::arg("audio"), py::arg("data"),
             "Embed binary data as watermark")
        .def("set_strength", &PyWatermarkEmbedder::set_strength,
             py::arg("strength"), "Set watermark strength")
        .def("reset", &PyWatermarkEmbedder::reset, "Reset embedder state");

    // Watermark Detector
    py::class_<PyWatermarkDetector>(m, "WatermarkDetector", R"pbdoc(
        Audio watermark detector for extracting hidden data.

        Args:
            sample_rate: Audio sample rate in Hz

        Example:
            >>> detector = WatermarkDetector(48000)
            >>> result = detector.detect_string(audio)
            >>> if result['detected']:
            ...     print(f"Found: {result['message']}")
    )pbdoc")
        .def(py::init<int>(),
             py::arg("sample_rate"))
        .def("detect_string", &PyWatermarkDetector::detect_string,
             py::arg("audio"),
             "Detect and extract string watermark")
        .def("has_watermark", &PyWatermarkDetector::has_watermark,
             py::arg("audio"), "Check if audio contains a watermark")
        .def("reset", &PyWatermarkDetector::reset, "Reset detector state");

    // Reverb presets enum
    py::enum_<voice_reverb_preset_t>(m, "ReverbPreset")
        .value("SMALL_ROOM", VOICE_REVERB_SMALL_ROOM)
        .value("MEDIUM_ROOM", VOICE_REVERB_MEDIUM_ROOM)
        .value("LARGE_ROOM", VOICE_REVERB_LARGE_ROOM)
        .value("HALL", VOICE_REVERB_HALL)
        .value("CATHEDRAL", VOICE_REVERB_CATHEDRAL)
        .value("PLATE", VOICE_REVERB_PLATE)
        .export_values();
}
