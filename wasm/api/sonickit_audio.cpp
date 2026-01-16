/**
 * @file sonickit_audio.cpp
 * @brief Audio utilities and spatial audio bindings for SonicKit WebAssembly
 * @author SonicKit Team
 *
 * This file adds WASM bindings for:
 * - Ring Buffer (audio buffering)
 * - Level Meter (audio level metering)
 * - Mixer (multi-source mixing)
 * - Spatial Renderer (3D audio positioning)
 * - HRTF Processor (binaural 3D audio)
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstring>

extern "C" {
#include "audio/audio_buffer.h"
#include "audio/audio_level.h"
#include "audio/audio_mixer.h"
#include "dsp/spatial.h"
#include "dsp/hrtf.h"
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
 * Ring Buffer Wrapper
 * ============================================ */

class WasmRingBuffer {
private:
    voice_ring_buffer_t* buffer;

public:
    /**
     * @brief Create a ring buffer for storing audio samples
     * @param capacity_bytes Maximum capacity in bytes
     * @param frame_size Size of each frame in bytes (typically sample_size * channels)
     */
    WasmRingBuffer(size_t capacity_bytes, size_t frame_size = 2)
        : buffer(nullptr) {
        buffer = voice_ring_buffer_create(capacity_bytes, frame_size);
        if (!buffer) {
            throw std::runtime_error("Failed to create ring buffer");
        }
    }

    ~WasmRingBuffer() {
        if (buffer) {
            voice_ring_buffer_destroy(buffer);
        }
    }

    size_t write(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        return voice_ring_buffer_write(buffer, vec.data(), vec.size() * sizeof(int16_t));
    }

    val read(size_t num_samples) {
        std::vector<int16_t> output(num_samples);
        size_t bytes_read = voice_ring_buffer_read(buffer, output.data(), num_samples * sizeof(int16_t));
        size_t samples_read = bytes_read / sizeof(int16_t);
        output.resize(samples_read);
        return int16VectorToJsArray(output);
    }

    val peek(size_t num_samples) {
        std::vector<int16_t> output(num_samples);
        size_t bytes_peeked = voice_ring_buffer_peek(buffer, output.data(), num_samples * sizeof(int16_t));
        size_t samples_peeked = bytes_peeked / sizeof(int16_t);
        output.resize(samples_peeked);
        return int16VectorToJsArray(output);
    }

    size_t getAvailable() {
        return voice_ring_buffer_available(buffer) / sizeof(int16_t);
    }

    size_t getFreeSpace() {
        return voice_ring_buffer_free_space(buffer) / sizeof(int16_t);
    }

    void clear() {
        voice_ring_buffer_clear(buffer);
    }

    size_t skip(size_t num_samples) {
        return voice_ring_buffer_skip(buffer, num_samples * sizeof(int16_t)) / sizeof(int16_t);
    }
};

/* ============================================
 * Level Meter Wrapper
 * ============================================ */

class WasmLevelMeter {
private:
    voice_level_meter_t* meter;
    voice_level_result_t last_result;

public:
    /**
     * @brief Create a level meter for measuring audio signal levels
     * @param sample_rate Audio sample rate in Hz
     * @param channels Number of audio channels
     */
    WasmLevelMeter(int sample_rate, int channels = 1)
        : meter(nullptr) {
        voice_level_meter_config_t config;
        voice_level_meter_config_init(&config);
        config.sample_rate = sample_rate;
        config.channels = channels;

        meter = voice_level_meter_create(&config);
        if (!meter) {
            throw std::runtime_error("Failed to create level meter");
        }
        memset(&last_result, 0, sizeof(last_result));
    }

    ~WasmLevelMeter() {
        if (meter) {
            voice_level_meter_destroy(meter);
        }
    }

    void process(const val& input) {
        auto vec = jsArrayToInt16Vector(input);
        voice_level_meter_process(meter, vec.data(), vec.size(), &last_result);
    }

    float getLevelDb() {
        return voice_level_meter_get_level_db(meter);
    }

    float getPeakDb() {
        return last_result.peak_db;
    }

    float getRmsDb() {
        return last_result.rms_db;
    }

    bool isClipping() {
        return last_result.clipping;
    }

    void reset() {
        if (meter) {
            voice_level_meter_reset(meter);
        }
    }
};

/* ============================================
 * Audio Mixer Wrapper
 * ============================================ */

class WasmMixer {
private:
    voice_mixer_t* mixer;
    std::vector<mixer_source_id_t> sources;

public:
    /**
     * @brief Create an audio mixer for combining multiple audio streams
     * @param sample_rate Audio sample rate in Hz
     * @param max_sources Maximum number of sources to mix
     * @param frame_size Samples per frame
     */
    WasmMixer(int sample_rate, int max_sources, int frame_size)
        : mixer(nullptr) {
        voice_mixer_config_t config;
        voice_mixer_config_init(&config);
        config.sample_rate = sample_rate;
        config.max_sources = max_sources;
        config.frame_size = frame_size;
        config.channels = 1;

        mixer = voice_mixer_create(&config);
        if (!mixer) {
            throw std::runtime_error("Failed to create mixer");
        }
    }

    ~WasmMixer() {
        if (mixer) {
            voice_mixer_destroy(mixer);
        }
    }

    int addSource(float gain = 1.0f) {
        voice_mixer_source_config_t src_config;
        src_config.gain = gain;
        src_config.pan = 0.0f;
        src_config.muted = false;
        src_config.priority = 0;
        src_config.user_data = nullptr;

        mixer_source_id_t id = voice_mixer_add_source(mixer, &src_config);
        if (id == MIXER_INVALID_SOURCE_ID) {
            throw std::runtime_error("Failed to add mixer source");
        }
        sources.push_back(id);
        return static_cast<int>(sources.size() - 1);
    }

    void removeSource(int index) {
        if (index < 0 || index >= static_cast<int>(sources.size())) {
            throw std::runtime_error("Invalid source index");
        }
        voice_mixer_remove_source(mixer, sources[index]);
        sources.erase(sources.begin() + index);
    }

    void pushAudio(int index, const val& input) {
        if (index < 0 || index >= static_cast<int>(sources.size())) {
            throw std::runtime_error("Invalid source index");
        }
        auto vec = jsArrayToInt16Vector(input);
        voice_error_t err = voice_mixer_push_audio(mixer, sources[index], vec.data(), vec.size());
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Failed to push audio to mixer");
        }
    }

    void setSourceGain(int index, float gain) {
        if (index < 0 || index >= static_cast<int>(sources.size())) {
            throw std::runtime_error("Invalid source index");
        }
        voice_mixer_set_source_gain(mixer, sources[index], gain);
    }

    void setSourceMuted(int index, bool muted) {
        if (index < 0 || index >= static_cast<int>(sources.size())) {
            throw std::runtime_error("Invalid source index");
        }
        voice_mixer_set_source_muted(mixer, sources[index], muted);
    }

    void setMasterGain(float gain) {
        voice_mixer_set_master_gain(mixer, gain);
    }

    val getOutput(int num_samples) {
        std::vector<int16_t> output(num_samples);
        size_t samples_out = 0;

        voice_error_t err = voice_mixer_get_output(mixer, output.data(), num_samples, &samples_out);
        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Failed to get mixer output");
        }

        output.resize(samples_out);
        return int16VectorToJsArray(output);
    }

    int getActiveSourceCount() {
        // Get stats to get active source count
        voice_mixer_stats_t stats;
        if (voice_mixer_get_stats(mixer, &stats) == VOICE_SUCCESS) {
            return static_cast<int>(stats.active_sources);
        }
        return static_cast<int>(sources.size());
    }
};

/* ============================================
 * Spatial Renderer Wrapper
 * ============================================ */

class WasmSpatialRenderer {
private:
    voice_spatial_renderer_t* renderer;
    voice_spatial_listener_t listener;
    voice_spatial_source_t source;

public:
    /**
     * @brief Create a spatial audio renderer for 3D positioning
     * @param sample_rate Audio sample rate in Hz
     * @param frame_size Samples per frame
     */
    WasmSpatialRenderer(int sample_rate, int frame_size)
        : renderer(nullptr) {
        voice_spatial_config_t config;
        voice_spatial_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;

        renderer = voice_spatial_renderer_create(&config);
        if (!renderer) {
            throw std::runtime_error("Failed to create spatial renderer");
        }

        voice_spatial_listener_init(&listener);
        voice_spatial_source_init(&source);
    }

    ~WasmSpatialRenderer() {
        if (renderer) {
            voice_spatial_renderer_destroy(renderer);
        }
    }

    void setSourcePosition(float x, float y, float z) {
        source.position.x = x;
        source.position.y = y;
        source.position.z = z;
    }

    void setSourceGain(float gain) {
        source.gain = gain;
    }

    void setSourceAttenuation(float min_dist, float max_dist, float rolloff) {
        source.min_distance = min_dist;
        source.max_distance = max_dist;
        source.rolloff_factor = rolloff;
    }

    void setListenerPosition(float x, float y, float z) {
        listener.position.x = x;
        listener.position.y = y;
        listener.position.z = z;
        voice_spatial_set_listener(renderer, &listener);
    }

    void setListenerOrientation(float fx, float fy, float fz, float ux, float uy, float uz) {
        listener.forward.x = fx;
        listener.forward.y = fy;
        listener.forward.z = fz;
        listener.up.x = ux;
        listener.up.y = uy;
        listener.up.z = uz;
        voice_spatial_set_listener(renderer, &listener);
    }

    val process(const val& input) {
        auto in_vec = jsArrayToInt16Vector(input);

        // Output is stereo (2x mono input)
        std::vector<int16_t> output(in_vec.size() * 2);

        voice_error_t err = voice_spatial_render_source_int16(
            renderer, &source, in_vec.data(), output.data(), in_vec.size());

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Spatial rendering failed");
        }

        return int16VectorToJsArray(output);
    }

    void reset() {
        if (renderer) {
            voice_spatial_renderer_reset(renderer);
        }
    }
};

/* ============================================
 * HRTF Processor Wrapper
 * ============================================ */

class WasmHRTFProcessor {
private:
    voice_hrtf_t* hrtf_data;
    voice_hrtf_processor_t* processor;
    float current_azimuth;
    float current_elevation;

public:
    /**
     * @brief Create an HRTF processor for binaural 3D audio
     * @param sample_rate Audio sample rate in Hz
     * @param block_size Processing block size
     */
    WasmHRTFProcessor(int sample_rate, int block_size = 256)
        : hrtf_data(nullptr), processor(nullptr), current_azimuth(0), current_elevation(0) {

        // Load built-in HRTF dataset
        hrtf_data = voice_hrtf_load_builtin();
        if (!hrtf_data) {
            throw std::runtime_error("Failed to load HRTF dataset");
        }

        voice_hrtf_config_t config;
        voice_hrtf_config_init(&config);
        config.sample_rate = sample_rate;
        config.block_size = block_size;

        processor = voice_hrtf_processor_create(hrtf_data, &config);
        if (!processor) {
            voice_hrtf_destroy(hrtf_data);
            throw std::runtime_error("Failed to create HRTF processor");
        }
    }

    ~WasmHRTFProcessor() {
        if (processor) {
            voice_hrtf_processor_destroy(processor);
        }
        if (hrtf_data) {
            voice_hrtf_destroy(hrtf_data);
        }
    }

    void setAzimuth(float azimuth_deg) {
        current_azimuth = azimuth_deg;
    }

    void setElevation(float elevation_deg) {
        current_elevation = elevation_deg;
    }

    void setPosition(float azimuth_deg, float elevation_deg) {
        current_azimuth = azimuth_deg;
        current_elevation = elevation_deg;
    }

    val process(const val& input) {
        auto in_vec = jsArrayToInt16Vector(input);

        // Output is stereo interleaved
        std::vector<int16_t> output(in_vec.size() * 2);

        voice_error_t err = voice_hrtf_process_int16(
            processor, in_vec.data(), output.data(), in_vec.size(),
            current_azimuth, current_elevation);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("HRTF processing failed");
        }

        return int16VectorToJsArray(output);
    }

    void reset() {
        if (processor) {
            voice_hrtf_processor_reset(processor);
        }
    }
};

/* ============================================
 * Embind Bindings
 * ============================================ */

EMSCRIPTEN_BINDINGS(sonickit_audio) {
    // Ring Buffer
    class_<WasmRingBuffer>("RingBuffer")
        .constructor<size_t, size_t>()
        .function("write", &WasmRingBuffer::write)
        .function("read", &WasmRingBuffer::read)
        .function("peek", &WasmRingBuffer::peek)
        .function("getAvailable", &WasmRingBuffer::getAvailable)
        .function("getFreeSpace", &WasmRingBuffer::getFreeSpace)
        .function("clear", &WasmRingBuffer::clear)
        .function("skip", &WasmRingBuffer::skip);

    // Level Meter
    class_<WasmLevelMeter>("LevelMeter")
        .constructor<int, int>()
        .function("process", &WasmLevelMeter::process)
        .function("getLevelDb", &WasmLevelMeter::getLevelDb)
        .function("getPeakDb", &WasmLevelMeter::getPeakDb)
        .function("getRmsDb", &WasmLevelMeter::getRmsDb)
        .function("isClipping", &WasmLevelMeter::isClipping)
        .function("reset", &WasmLevelMeter::reset);

    // Mixer
    class_<WasmMixer>("Mixer")
        .constructor<int, int, int>()
        .function("addSource", &WasmMixer::addSource)
        .function("removeSource", &WasmMixer::removeSource)
        .function("pushAudio", &WasmMixer::pushAudio)
        .function("setSourceGain", &WasmMixer::setSourceGain)
        .function("setSourceMuted", &WasmMixer::setSourceMuted)
        .function("setMasterGain", &WasmMixer::setMasterGain)
        .function("getOutput", &WasmMixer::getOutput)
        .function("getActiveSourceCount", &WasmMixer::getActiveSourceCount);

    // Spatial Renderer
    class_<WasmSpatialRenderer>("SpatialRenderer")
        .constructor<int, int>()
        .function("setSourcePosition", &WasmSpatialRenderer::setSourcePosition)
        .function("setSourceGain", &WasmSpatialRenderer::setSourceGain)
        .function("setSourceAttenuation", &WasmSpatialRenderer::setSourceAttenuation)
        .function("setListenerPosition", &WasmSpatialRenderer::setListenerPosition)
        .function("setListenerOrientation", &WasmSpatialRenderer::setListenerOrientation)
        .function("process", &WasmSpatialRenderer::process)
        .function("reset", &WasmSpatialRenderer::reset);

    // HRTF Processor
    class_<WasmHRTFProcessor>("HRTFProcessor")
        .constructor<int, int>()
        .function("setAzimuth", &WasmHRTFProcessor::setAzimuth)
        .function("setElevation", &WasmHRTFProcessor::setElevation)
        .function("setPosition", &WasmHRTFProcessor::setPosition)
        .function("process", &WasmHRTFProcessor::process)
        .function("reset", &WasmHRTFProcessor::reset);
}
