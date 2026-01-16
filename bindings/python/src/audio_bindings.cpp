/**
 * @file audio_bindings.cpp
 * @brief Audio utilities Python bindings (Buffer, Level, Mixer, JitterBuffer, Spatial)
 * @author SonicKit Team
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <vector>
#include <cstring>

extern "C" {
#include "audio/audio_buffer.h"
#include "audio/audio_level.h"
#include "audio/audio_mixer.h"
#include "audio/jitter_buffer.h"
#include "dsp/spatial_audio.h"
#include "dsp/hrtf.h"
#include "voice/error.h"
}

namespace py = pybind11;

/* ============================================
 * Audio Buffer Wrapper
 * ============================================ */

class PyAudioBuffer {
private:
    voice_audio_buffer_t* buffer_;

public:
    PyAudioBuffer(size_t capacity_samples, int channels = 1)
        : buffer_(nullptr) {

        voice_audio_buffer_config_t config;
        voice_audio_buffer_config_init(&config);
        config.capacity_samples = capacity_samples;
        config.channels = channels;
        config.sample_format = VOICE_SAMPLE_S16;

        buffer_ = voice_audio_buffer_create(&config);
        if (!buffer_) {
            throw std::runtime_error("Failed to create audio buffer");
        }
    }

    ~PyAudioBuffer() {
        if (buffer_) {
            voice_audio_buffer_destroy(buffer_);
        }
    }

    void write(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        voice_error_t err = voice_audio_buffer_write(
            buffer_, static_cast<int16_t*>(buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Buffer write failed");
        }
    }

    py::array_t<int16_t> read(size_t num_samples) {
        py::array_t<int16_t> output(num_samples);
        auto out_buf = output.request();
        size_t read_count = 0;

        voice_error_t err = voice_audio_buffer_read(
            buffer_, static_cast<int16_t*>(out_buf.ptr), num_samples, &read_count);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Buffer read failed");
        }

        output.resize({static_cast<py::ssize_t>(read_count)});
        return output;
    }

    py::array_t<int16_t> peek(size_t num_samples) {
        py::array_t<int16_t> output(num_samples);
        auto out_buf = output.request();
        size_t peek_count = 0;

        voice_error_t err = voice_audio_buffer_peek(
            buffer_, static_cast<int16_t*>(out_buf.ptr), num_samples, &peek_count);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Buffer peek failed");
        }

        output.resize({static_cast<py::ssize_t>(peek_count)});
        return output;
    }

    size_t available() const {
        return voice_audio_buffer_available(buffer_);
    }

    size_t capacity() const {
        return voice_audio_buffer_capacity(buffer_);
    }

    size_t free_space() const {
        return voice_audio_buffer_free_space(buffer_);
    }

    void clear() {
        voice_audio_buffer_clear(buffer_);
    }
};

/* ============================================
 * Audio Level Wrapper
 * ============================================ */

class PyAudioLevel {
private:
    voice_audio_level_t* level_;

public:
    PyAudioLevel(int sample_rate, int frame_size)
        : level_(nullptr) {

        voice_audio_level_config_t config;
        voice_audio_level_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;

        level_ = voice_audio_level_create(&config);
        if (!level_) {
            throw std::runtime_error("Failed to create audio level meter");
        }
    }

    ~PyAudioLevel() {
        if (level_) {
            voice_audio_level_destroy(level_);
        }
    }

    void process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        voice_audio_level_process(level_, static_cast<int16_t*>(buf.ptr), buf.size);
    }

    float get_level_db() const {
        return voice_audio_level_get_db(level_);
    }

    float get_peak_db() const {
        return voice_audio_level_get_peak_db(level_);
    }

    float get_rms_db() const {
        return voice_audio_level_get_rms_db(level_);
    }

    bool is_silence(float threshold_db = -50.0f) const {
        return voice_audio_level_is_silence(level_, threshold_db);
    }

    bool is_clipping() const {
        return voice_audio_level_is_clipping(level_);
    }

    void reset() {
        voice_audio_level_reset(level_);
    }
};

/* ============================================
 * Audio Mixer Wrapper
 * ============================================ */

class PyAudioMixer {
private:
    voice_audio_mixer_t* mixer_;
    int num_inputs_;
    int frame_size_;

public:
    PyAudioMixer(int sample_rate, int num_inputs, int frame_size)
        : mixer_(nullptr), num_inputs_(num_inputs), frame_size_(frame_size) {

        voice_audio_mixer_config_t config;
        voice_audio_mixer_config_init(&config);
        config.sample_rate = sample_rate;
        config.num_inputs = num_inputs;
        config.frame_size = frame_size;

        mixer_ = voice_audio_mixer_create(&config);
        if (!mixer_) {
            throw std::runtime_error("Failed to create audio mixer");
        }
    }

    ~PyAudioMixer() {
        if (mixer_) {
            voice_audio_mixer_destroy(mixer_);
        }
    }

    void set_input(int index, py::array_t<int16_t> input) {
        if (index < 0 || index >= num_inputs_) {
            throw std::runtime_error("Invalid input index");
        }

        py::buffer_info buf = input.request();
        voice_error_t err = voice_audio_mixer_set_input(
            mixer_, index, static_cast<int16_t*>(buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Failed to set mixer input");
        }
    }

    void set_input_gain(int index, float gain) {
        if (index < 0 || index >= num_inputs_) {
            throw std::runtime_error("Invalid input index");
        }
        voice_audio_mixer_set_input_gain(mixer_, index, gain);
    }

    void set_master_gain(float gain) {
        voice_audio_mixer_set_master_gain(mixer_, gain);
    }

    py::array_t<int16_t> mix(int output_samples) {
        py::array_t<int16_t> output(output_samples);
        auto out_buf = output.request();

        voice_error_t err = voice_audio_mixer_mix(
            mixer_, static_cast<int16_t*>(out_buf.ptr), output_samples);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Mixing failed");
        }

        return output;
    }

    void reset() {
        voice_audio_mixer_reset(mixer_);
    }

    int num_inputs() const { return num_inputs_; }
};

/* ============================================
 * Jitter Buffer Wrapper
 * ============================================ */

class PyJitterBuffer {
private:
    voice_jitter_buffer_t* jbuf_;

public:
    PyJitterBuffer(int sample_rate, int frame_size_ms = 20,
                   int min_delay_ms = 20, int max_delay_ms = 200)
        : jbuf_(nullptr) {

        voice_jitter_buffer_config_t config;
        voice_jitter_buffer_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size_ms = frame_size_ms;
        config.min_delay_ms = min_delay_ms;
        config.max_delay_ms = max_delay_ms;

        jbuf_ = voice_jitter_buffer_create(&config);
        if (!jbuf_) {
            throw std::runtime_error("Failed to create jitter buffer");
        }
    }

    ~PyJitterBuffer() {
        if (jbuf_) {
            voice_jitter_buffer_destroy(jbuf_);
        }
    }

    void put(py::array_t<int16_t> input, int timestamp, int sequence) {
        py::buffer_info buf = input.request();

        voice_jitter_packet_t packet;
        packet.data = buf.ptr;
        packet.size = buf.size * sizeof(int16_t);
        packet.timestamp = timestamp;
        packet.sequence = sequence;

        voice_jitter_buffer_put(jbuf_, &packet);
    }

    py::array_t<int16_t> get(int num_samples) {
        py::array_t<int16_t> output(num_samples);
        auto out_buf = output.request();

        voice_jitter_get_result_t result;
        voice_error_t err = voice_jitter_buffer_get(
            jbuf_, static_cast<int16_t*>(out_buf.ptr), num_samples, &result);

        if (err != VOICE_SUCCESS) {
            std::memset(out_buf.ptr, 0, num_samples * sizeof(int16_t));
        }

        return output;
    }

    py::dict get_stats() const {
        voice_jitter_stats_t stats;
        voice_jitter_buffer_get_stats(jbuf_, &stats);

        py::dict result;
        result["packets_received"] = stats.packets_received;
        result["packets_lost"] = stats.packets_lost;
        result["packets_discarded"] = stats.packets_discarded;
        result["current_delay_ms"] = stats.current_delay_ms;
        result["average_delay_ms"] = stats.average_delay_ms;
        result["buffer_level"] = stats.buffer_level;

        return result;
    }

    float get_delay_ms() const {
        return voice_jitter_buffer_get_delay(jbuf_);
    }

    void reset() {
        voice_jitter_buffer_reset(jbuf_);
    }
};

/* ============================================
 * Spatial Renderer Wrapper
 * ============================================ */

class PySpatialRenderer {
private:
    voice_spatial_renderer_t* renderer_;
    int frame_size_;

public:
    PySpatialRenderer(int sample_rate, int frame_size)
        : renderer_(nullptr), frame_size_(frame_size) {

        voice_spatial_config_t config;
        voice_spatial_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;

        renderer_ = voice_spatial_renderer_create(&config);
        if (!renderer_) {
            throw std::runtime_error("Failed to create spatial renderer");
        }
    }

    ~PySpatialRenderer() {
        if (renderer_) {
            voice_spatial_renderer_destroy(renderer_);
        }
    }

    void set_source_position(float x, float y, float z) {
        voice_spatial_position_t pos = {x, y, z};
        voice_spatial_renderer_set_source_position(renderer_, &pos);
    }

    void set_listener_position(float x, float y, float z) {
        voice_spatial_position_t pos = {x, y, z};
        voice_spatial_renderer_set_listener_position(renderer_, &pos);
    }

    void set_listener_orientation(float yaw, float pitch, float roll) {
        voice_spatial_orientation_t orient = {yaw, pitch, roll};
        voice_spatial_renderer_set_listener_orientation(renderer_, &orient);
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        std::vector<int16_t> left(buf.size);
        std::vector<int16_t> right(buf.size);

        voice_error_t err = voice_spatial_renderer_process(
            renderer_,
            static_cast<int16_t*>(buf.ptr),
            left.data(), right.data(), buf.size
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Spatial rendering failed");
        }

        // Interleave L/R to stereo output
        py::array_t<int16_t> output(buf.size * 2);
        auto out_buf = output.request();
        int16_t* out_ptr = static_cast<int16_t*>(out_buf.ptr);

        for (size_t i = 0; i < static_cast<size_t>(buf.size); i++) {
            out_ptr[i * 2] = left[i];
            out_ptr[i * 2 + 1] = right[i];
        }

        return output;
    }

    void set_attenuation(float min_distance, float max_distance, float rolloff) {
        voice_spatial_renderer_set_attenuation(renderer_, min_distance, max_distance, rolloff);
    }

    void reset() {
        voice_spatial_renderer_reset(renderer_);
    }
};

/* ============================================
 * HRTF Wrapper
 * ============================================ */

class PyHRTF {
private:
    voice_hrtf_t* hrtf_;

public:
    PyHRTF(int sample_rate)
        : hrtf_(nullptr) {

        voice_hrtf_config_t config;
        voice_hrtf_config_init(&config);
        config.sample_rate = sample_rate;

        hrtf_ = voice_hrtf_create(&config);
        if (!hrtf_) {
            throw std::runtime_error("Failed to create HRTF processor");
        }
    }

    ~PyHRTF() {
        if (hrtf_) {
            voice_hrtf_destroy(hrtf_);
        }
    }

    void set_azimuth(float azimuth_deg) {
        voice_hrtf_set_azimuth(hrtf_, azimuth_deg);
    }

    void set_elevation(float elevation_deg) {
        voice_hrtf_set_elevation(hrtf_, elevation_deg);
    }

    void set_distance(float distance) {
        voice_hrtf_set_distance(hrtf_, distance);
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();

        std::vector<int16_t> left(buf.size);
        std::vector<int16_t> right(buf.size);

        voice_error_t err = voice_hrtf_process(
            hrtf_,
            static_cast<int16_t*>(buf.ptr),
            left.data(), right.data(), buf.size
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("HRTF processing failed");
        }

        // Interleave to stereo
        py::array_t<int16_t> output(buf.size * 2);
        auto out_buf = output.request();
        int16_t* out_ptr = static_cast<int16_t*>(out_buf.ptr);

        for (size_t i = 0; i < static_cast<size_t>(buf.size); i++) {
            out_ptr[i * 2] = left[i];
            out_ptr[i * 2 + 1] = right[i];
        }

        return output;
    }

    void reset() {
        voice_hrtf_reset(hrtf_);
    }
};

/* ============================================
 * Module Initialization
 * ============================================ */

void init_audio_bindings(py::module_& m) {
    // Audio Buffer
    py::class_<PyAudioBuffer>(m, "AudioBuffer", R"pbdoc(
        Audio sample buffer for storage and management.

        Args:
            capacity_samples: Maximum number of samples to store
            channels: Number of audio channels

        Example:
            >>> buffer = AudioBuffer(16000, channels=1)
            >>> buffer.write(audio_samples)
            >>> samples = buffer.read(160)
    )pbdoc")
        .def(py::init<size_t, int>(),
             py::arg("capacity_samples"),
             py::arg("channels") = 1)
        .def("write", &PyAudioBuffer::write, py::arg("input"),
             "Write samples to buffer")
        .def("read", &PyAudioBuffer::read, py::arg("num_samples"),
             "Read and remove samples from buffer")
        .def("peek", &PyAudioBuffer::peek, py::arg("num_samples"),
             "Read samples without removing them")
        .def_property_readonly("available", &PyAudioBuffer::available,
             "Number of samples available for reading")
        .def_property_readonly("capacity", &PyAudioBuffer::capacity,
             "Total buffer capacity")
        .def_property_readonly("free_space", &PyAudioBuffer::free_space,
             "Free space remaining")
        .def("clear", &PyAudioBuffer::clear, "Clear all samples");

    // Audio Level
    py::class_<PyAudioLevel>(m, "AudioLevel", R"pbdoc(
        Audio level meter for signal monitoring.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per measurement frame

        Example:
            >>> level = AudioLevel(16000, 160)
            >>> level.process(audio)
            >>> print(f"Level: {level.get_level_db()} dB")
    )pbdoc")
        .def(py::init<int, int>(),
             py::arg("sample_rate"),
             py::arg("frame_size"))
        .def("process", &PyAudioLevel::process, py::arg("input"),
             "Process audio samples")
        .def("get_level_db", &PyAudioLevel::get_level_db,
             "Get current level in dB")
        .def("get_peak_db", &PyAudioLevel::get_peak_db,
             "Get peak level in dB")
        .def("get_rms_db", &PyAudioLevel::get_rms_db,
             "Get RMS level in dB")
        .def("is_silence", &PyAudioLevel::is_silence,
             py::arg("threshold_db") = -50.0f,
             "Check if audio is silence")
        .def("is_clipping", &PyAudioLevel::is_clipping,
             "Check if audio is clipping")
        .def("reset", &PyAudioLevel::reset, "Reset level meter");

    // Audio Mixer
    py::class_<PyAudioMixer>(m, "AudioMixer", R"pbdoc(
        Multi-channel audio mixer.

        Args:
            sample_rate: Audio sample rate in Hz
            num_inputs: Number of input channels to mix
            frame_size: Samples per frame

        Example:
            >>> mixer = AudioMixer(48000, 4, 480)
            >>> mixer.set_input(0, voice1)
            >>> mixer.set_input(1, voice2)
            >>> mixer.set_input_gain(1, 0.5)
            >>> mixed = mixer.mix(480)
    )pbdoc")
        .def(py::init<int, int, int>(),
             py::arg("sample_rate"),
             py::arg("num_inputs"),
             py::arg("frame_size"))
        .def("set_input", &PyAudioMixer::set_input,
             py::arg("index"), py::arg("input"),
             "Set input audio for a channel")
        .def("set_input_gain", &PyAudioMixer::set_input_gain,
             py::arg("index"), py::arg("gain"),
             "Set gain for an input channel")
        .def("set_master_gain", &PyAudioMixer::set_master_gain,
             py::arg("gain"), "Set master output gain")
        .def("mix", &PyAudioMixer::mix, py::arg("output_samples"),
             "Mix all inputs and return output")
        .def("reset", &PyAudioMixer::reset, "Reset mixer state")
        .def_property_readonly("num_inputs", &PyAudioMixer::num_inputs);

    // Jitter Buffer
    py::class_<PyJitterBuffer>(m, "JitterBuffer", R"pbdoc(
        Jitter buffer for network audio.

        Handles packet reordering, loss concealment, and timing.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size_ms: Frame size in milliseconds
            min_delay_ms: Minimum buffer delay
            max_delay_ms: Maximum buffer delay

        Example:
            >>> jbuf = JitterBuffer(16000, 20, 20, 200)
            >>> jbuf.put(packet_audio, timestamp, sequence)
            >>> playback = jbuf.get(160)
    )pbdoc")
        .def(py::init<int, int, int, int>(),
             py::arg("sample_rate"),
             py::arg("frame_size_ms") = 20,
             py::arg("min_delay_ms") = 20,
             py::arg("max_delay_ms") = 200)
        .def("put", &PyJitterBuffer::put,
             py::arg("input"), py::arg("timestamp"), py::arg("sequence"),
             "Put a packet into the jitter buffer")
        .def("get", &PyJitterBuffer::get, py::arg("num_samples"),
             "Get audio from the jitter buffer")
        .def("get_stats", &PyJitterBuffer::get_stats,
             "Get jitter buffer statistics")
        .def("get_delay_ms", &PyJitterBuffer::get_delay_ms,
             "Get current buffer delay in ms")
        .def("reset", &PyJitterBuffer::reset, "Reset jitter buffer");

    // Spatial Renderer
    py::class_<PySpatialRenderer>(m, "SpatialRenderer", R"pbdoc(
        3D spatial audio renderer.

        Positions audio sources in 3D space relative to the listener.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame

        Example:
            >>> spatial = SpatialRenderer(48000, 480)
            >>> spatial.set_listener_position(0, 0, 0)
            >>> spatial.set_source_position(-2, 0, 1)
            >>> stereo = spatial.process(mono_audio)
    )pbdoc")
        .def(py::init<int, int>(),
             py::arg("sample_rate"),
             py::arg("frame_size"))
        .def("set_source_position", &PySpatialRenderer::set_source_position,
             py::arg("x"), py::arg("y"), py::arg("z"),
             "Set sound source position")
        .def("set_listener_position", &PySpatialRenderer::set_listener_position,
             py::arg("x"), py::arg("y"), py::arg("z"),
             "Set listener position")
        .def("set_listener_orientation", &PySpatialRenderer::set_listener_orientation,
             py::arg("yaw"), py::arg("pitch"), py::arg("roll"),
             "Set listener orientation")
        .def("process", &PySpatialRenderer::process, py::arg("input"),
             "Process mono audio to stereo with spatial positioning")
        .def("set_attenuation", &PySpatialRenderer::set_attenuation,
             py::arg("min_distance"), py::arg("max_distance"), py::arg("rolloff"),
             "Set distance attenuation parameters")
        .def("reset", &PySpatialRenderer::reset, "Reset renderer state");

    // HRTF
    py::class_<PyHRTF>(m, "HRTF", R"pbdoc(
        Head-Related Transfer Function processor.

        Provides realistic 3D audio using HRTF filtering.

        Args:
            sample_rate: Audio sample rate in Hz

        Example:
            >>> hrtf = HRTF(48000)
            >>> hrtf.set_azimuth(-45)  # 45 degrees left
            >>> hrtf.set_elevation(0)
            >>> stereo = hrtf.process(mono_audio)
    )pbdoc")
        .def(py::init<int>(),
             py::arg("sample_rate"))
        .def("set_azimuth", &PyHRTF::set_azimuth, py::arg("azimuth_deg"),
             "Set azimuth angle in degrees (-180 to 180)")
        .def("set_elevation", &PyHRTF::set_elevation, py::arg("elevation_deg"),
             "Set elevation angle in degrees (-90 to 90)")
        .def("set_distance", &PyHRTF::set_distance, py::arg("distance"),
             "Set distance from listener")
        .def("process", &PyHRTF::process, py::arg("input"),
             "Process mono audio to stereo with HRTF")
        .def("reset", &PyHRTF::reset, "Reset HRTF state");
}
