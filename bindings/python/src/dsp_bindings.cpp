/**
 * @file dsp_bindings.cpp
 * @brief DSP module Python bindings (Denoiser, AEC, AGC, VAD, Resampler, DTMF, etc.)
 * @author SonicKit Team
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <vector>
#include <cstring>

extern "C" {
#include "dsp/denoiser.h"
#include "dsp/echo_canceller.h"
#include "dsp/agc.h"
#include "dsp/vad.h"
#include "dsp/resampler.h"
#include "dsp/dtmf.h"
#include "dsp/equalizer.h"
#include "dsp/compressor.h"
#include "dsp/comfort_noise.h"
#include "dsp/delay_estimator.h"
#include "dsp/time_stretcher.h"
#include "voice/error.h"
}

namespace py = pybind11;

/* ============================================
 * Denoiser Wrapper
 * ============================================ */

class PyDenoiser {
private:
    voice_denoiser_t* denoiser_;
    int frame_size_;

public:
    PyDenoiser(int sample_rate, int frame_size, int engine_type = 0)
        : denoiser_(nullptr), frame_size_(frame_size) {

        voice_denoiser_config_t config;
        voice_denoiser_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.engine = static_cast<voice_denoise_engine_t>(engine_type);

        denoiser_ = voice_denoiser_create(&config);
        if (!denoiser_) {
            throw std::runtime_error("Failed to create denoiser");
        }
    }

    ~PyDenoiser() {
        if (denoiser_) {
            voice_denoiser_destroy(denoiser_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }
        if (buf.size != frame_size_) {
            throw std::runtime_error("Input size must match frame_size");
        }

        // Create output array (copy of input for in-place processing)
        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_denoiser_process(denoiser_,
            static_cast<int16_t*>(out_buf.ptr), buf.size);

        return output;
    }

    void reset() {
        if (denoiser_) {
            voice_denoiser_reset(denoiser_);
        }
    }

    int frame_size() const { return frame_size_; }
};

/* ============================================
 * Echo Canceller Wrapper
 * ============================================ */

class PyEchoCanceller {
private:
    voice_aec_t* aec_;
    int frame_size_;

public:
    PyEchoCanceller(int sample_rate, int frame_size, int filter_length = 2000)
        : aec_(nullptr), frame_size_(frame_size) {

        voice_aec_ext_config_t config;
        voice_aec_ext_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.filter_length = filter_length;

        aec_ = voice_aec_create(&config);
        if (!aec_) {
            throw std::runtime_error("Failed to create echo canceller");
        }
    }

    ~PyEchoCanceller() {
        if (aec_) {
            voice_aec_destroy(aec_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> captured,
                                  py::array_t<int16_t> playback) {
        py::buffer_info cap_buf = captured.request();
        py::buffer_info play_buf = playback.request();

        if (cap_buf.ndim != 1 || play_buf.ndim != 1) {
            throw std::runtime_error("Inputs must be 1-dimensional");
        }
        if (cap_buf.size != frame_size_ || play_buf.size != frame_size_) {
            throw std::runtime_error("Input sizes must match frame_size");
        }

        py::array_t<int16_t> output(frame_size_);
        auto out_buf = output.request();

        voice_error_t err = voice_aec_process(
            aec_,
            static_cast<int16_t*>(cap_buf.ptr),
            static_cast<int16_t*>(play_buf.ptr),
            static_cast<int16_t*>(out_buf.ptr),
            frame_size_
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("AEC processing failed");
        }

        return output;
    }

    void reset() {
        if (aec_) {
            voice_aec_reset(aec_);
        }
    }

    int frame_size() const { return frame_size_; }
};

/* ============================================
 * AGC Wrapper
 * ============================================ */

class PyAGC {
private:
    voice_agc_t* agc_;
    int frame_size_;

public:
    PyAGC(int sample_rate, int frame_size, int mode = 1, float target_level = -3.0f)
        : agc_(nullptr), frame_size_(frame_size) {

        voice_agc_config_t config;
        voice_agc_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.mode = static_cast<voice_agc_mode_t>(mode);
        config.target_level_dbfs = target_level;

        agc_ = voice_agc_create(&config);
        if (!agc_) {
            throw std::runtime_error("Failed to create AGC");
        }
    }

    ~PyAGC() {
        if (agc_) {
            voice_agc_destroy(agc_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1 || buf.size != frame_size_) {
            throw std::runtime_error("Invalid input size");
        }

        py::array_t<int16_t> output(frame_size_);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_agc_process(
            agc_, static_cast<int16_t*>(out_buf.ptr), frame_size_);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("AGC processing failed");
        }

        return output;
    }

    float get_gain() const {
        if (agc_) {
            voice_agc_state_t state;
            if (voice_agc_get_state(agc_, &state) == VOICE_SUCCESS) {
                return state.current_gain_db;
            }
        }
        return 0.0f;
    }

    void reset() {
        if (agc_) {
            voice_agc_reset(agc_);
        }
    }
};

/* ============================================
 * VAD Wrapper
 * ============================================ */

class PyVAD {
private:
    voice_vad_t* vad_;
    voice_vad_result_t last_result_;

public:
    PyVAD(int sample_rate, int mode = 1)
        : vad_(nullptr) {

        std::memset(&last_result_, 0, sizeof(last_result_));

        voice_vad_config_t config;
        voice_vad_config_init(&config);
        config.sample_rate = sample_rate;
        config.mode = static_cast<voice_vad_mode_t>(mode);

        vad_ = voice_vad_create(&config);
        if (!vad_) {
            throw std::runtime_error("Failed to create VAD");
        }
    }

    ~PyVAD() {
        if (vad_) {
            voice_vad_destroy(vad_);
        }
    }

    bool is_speech(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        voice_vad_process(vad_,
            static_cast<int16_t*>(buf.ptr), buf.size, &last_result_);
        return last_result_.is_speech;
    }

    float get_probability() const {
        return last_result_.speech_probability;
    }

    void reset() {
        if (vad_) {
            voice_vad_reset(vad_);
        }
        std::memset(&last_result_, 0, sizeof(last_result_));
    }
};

/* ============================================
 * Resampler Wrapper
 * ============================================ */

class PyResampler {
private:
    voice_resampler_t* resampler_;
    int in_rate_;
    int out_rate_;
    int channels_;

public:
    PyResampler(int channels, int in_rate, int out_rate, int quality = 5)
        : resampler_(nullptr), in_rate_(in_rate), out_rate_(out_rate), channels_(channels) {

        resampler_ = voice_resampler_create(channels, in_rate, out_rate, quality);
        if (!resampler_) {
            throw std::runtime_error("Failed to create resampler");
        }
    }

    ~PyResampler() {
        if (resampler_) {
            voice_resampler_destroy(resampler_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        // Calculate output size
        size_t output_frames = (buf.size * out_rate_ + in_rate_ - 1) / in_rate_;
        py::array_t<int16_t> output(output_frames);
        auto out_buf = output.request();

        int result = voice_resampler_process_int16(
            resampler_,
            static_cast<int16_t*>(buf.ptr), buf.size,
            static_cast<int16_t*>(out_buf.ptr), output_frames
        );

        if (result < 0) {
            throw std::runtime_error("Resampler processing failed");
        }

        // Resize to actual output
        output.resize({static_cast<py::ssize_t>(result)});
        return output;
    }

    void reset() {
        if (resampler_) {
            voice_resampler_reset(resampler_);
        }
    }
};

/* ============================================
 * DTMF Detector Wrapper
 * ============================================ */

class PyDTMFDetector {
private:
    voice_dtmf_detector_t* detector_;
    int frame_size_;

public:
    PyDTMFDetector(int sample_rate, int frame_size)
        : detector_(nullptr), frame_size_(frame_size) {

        voice_dtmf_detector_config_t config;
        voice_dtmf_detector_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;

        detector_ = voice_dtmf_detector_create(&config);
        if (!detector_) {
            throw std::runtime_error("Failed to create DTMF detector");
        }
    }

    ~PyDTMFDetector() {
        if (detector_) {
            voice_dtmf_detector_destroy(detector_);
        }
    }

    std::string process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        voice_dtmf_result_t result;
        voice_dtmf_digit_t digit = voice_dtmf_detector_process(
            detector_, static_cast<int16_t*>(buf.ptr), buf.size, &result);

        if (digit != VOICE_DTMF_NONE && result.valid) {
            return std::string(1, static_cast<char>(digit));
        }
        return "";
    }

    std::string get_digits() {
        char buffer[256];
        size_t count = voice_dtmf_detector_get_digits(detector_, buffer, sizeof(buffer));
        return std::string(buffer, count);
    }

    void clear_digits() {
        voice_dtmf_detector_clear_digits(detector_);
    }

    void reset() {
        if (detector_) {
            voice_dtmf_detector_reset(detector_);
        }
    }
};

/* ============================================
 * DTMF Generator Wrapper
 * ============================================ */

class PyDTMFGenerator {
private:
    voice_dtmf_generator_t* generator_;
    int sample_rate_;

public:
    PyDTMFGenerator(int sample_rate, int tone_duration_ms = 100, int pause_duration_ms = 50)
        : generator_(nullptr), sample_rate_(sample_rate) {

        voice_dtmf_generator_config_t config;
        voice_dtmf_generator_config_init(&config);
        config.sample_rate = sample_rate;
        config.tone_duration_ms = tone_duration_ms;
        config.pause_duration_ms = pause_duration_ms;

        generator_ = voice_dtmf_generator_create(&config);
        if (!generator_) {
            throw std::runtime_error("Failed to create DTMF generator");
        }
    }

    ~PyDTMFGenerator() {
        if (generator_) {
            voice_dtmf_generator_destroy(generator_);
        }
    }

    py::array_t<int16_t> generate_digit(const std::string& digit) {
        if (digit.empty()) {
            throw std::runtime_error("Empty digit");
        }

        size_t max_samples = sample_rate_; // 1 second max
        std::vector<int16_t> buffer(max_samples);

        size_t generated = voice_dtmf_generator_generate(
            generator_,
            static_cast<voice_dtmf_digit_t>(digit[0]),
            buffer.data(),
            max_samples
        );

        py::array_t<int16_t> output(generated);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buffer.data(), generated * sizeof(int16_t));
        return output;
    }

    py::array_t<int16_t> generate_sequence(const std::string& digits) {
        size_t max_samples = sample_rate_ * digits.length();
        std::vector<int16_t> buffer(max_samples);

        size_t generated = voice_dtmf_generator_generate_sequence(
            generator_, digits.c_str(), buffer.data(), max_samples);

        py::array_t<int16_t> output(generated);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buffer.data(), generated * sizeof(int16_t));
        return output;
    }

    void reset() {
        if (generator_) {
            voice_dtmf_generator_reset(generator_);
        }
    }
};

/* ============================================
 * Equalizer Wrapper
 * ============================================ */

class PyEqualizer {
private:
    voice_eq_t* eq_;
    int num_bands_;

public:
    PyEqualizer(int sample_rate, int num_bands = 5)
        : eq_(nullptr), num_bands_(num_bands) {

        voice_eq_config_t config;
        voice_eq_config_init(&config);
        config.sample_rate = sample_rate;
        config.num_bands = num_bands;

        std::vector<voice_eq_band_t> bands(num_bands);
        for (int i = 0; i < num_bands; i++) {
            bands[i].enabled = true;
            bands[i].type = VOICE_EQ_PEAK;
            bands[i].gain_db = 0.0f;
            bands[i].q = 1.0f;
        }

        if (num_bands >= 5) {
            bands[0].frequency = 60.0f;
            bands[1].frequency = 250.0f;
            bands[2].frequency = 1000.0f;
            bands[3].frequency = 4000.0f;
            bands[4].frequency = 12000.0f;
        }

        config.bands = bands.data();

        eq_ = voice_eq_create(&config);
        if (!eq_) {
            throw std::runtime_error("Failed to create equalizer");
        }
    }

    ~PyEqualizer() {
        if (eq_) {
            voice_eq_destroy(eq_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_eq_process(
            eq_, static_cast<int16_t*>(out_buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("EQ processing failed");
        }

        return output;
    }

    void set_band(int band_index, float frequency, float gain_db, float q) {
        if (band_index < 0 || band_index >= num_bands_) {
            throw std::runtime_error("Invalid band index");
        }

        voice_eq_band_t band;
        band.enabled = true;
        band.type = VOICE_EQ_PEAK;
        band.frequency = frequency;
        band.gain_db = gain_db;
        band.q = q;

        voice_eq_set_band(eq_, band_index, &band);
    }

    void set_master_gain(float gain_db) {
        voice_eq_set_master_gain(eq_, gain_db);
    }

    void apply_preset(int preset) {
        voice_eq_apply_preset(eq_, static_cast<voice_eq_preset_t>(preset));
    }

    void reset() {
        if (eq_) {
            voice_eq_reset(eq_);
        }
    }
};

/* ============================================
 * Compressor Wrapper
 * ============================================ */

class PyCompressor {
private:
    voice_compressor_t* comp_;

public:
    PyCompressor(int sample_rate, float threshold_db = -20.0f,
                 float ratio = 4.0f, float attack_ms = 10.0f,
                 float release_ms = 100.0f)
        : comp_(nullptr) {

        voice_compressor_config_t config;
        voice_compressor_config_init(&config);
        config.sample_rate = sample_rate;
        config.threshold_db = threshold_db;
        config.ratio = ratio;
        config.attack_ms = attack_ms;
        config.release_ms = release_ms;
        config.type = VOICE_DRC_COMPRESSOR;

        comp_ = voice_compressor_create(&config);
        if (!comp_) {
            throw std::runtime_error("Failed to create compressor");
        }
    }

    ~PyCompressor() {
        if (comp_) {
            voice_compressor_destroy(comp_);
        }
    }

    py::array_t<int16_t> process(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        py::array_t<int16_t> output(buf.size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, buf.ptr, buf.size * sizeof(int16_t));

        voice_error_t err = voice_compressor_process(
            comp_, static_cast<int16_t*>(out_buf.ptr), buf.size);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("Compressor processing failed");
        }

        return output;
    }

    void set_threshold(float threshold_db) {
        voice_compressor_set_threshold(comp_, threshold_db);
    }

    void set_ratio(float ratio) {
        voice_compressor_set_ratio(comp_, ratio);
    }

    void set_times(float attack_ms, float release_ms) {
        voice_compressor_set_times(comp_, attack_ms, release_ms);
    }
};

/* ============================================
 * Comfort Noise Generator Wrapper
 * ============================================ */

class PyComfortNoise {
private:
    voice_cng_t* cng_;
    int frame_size_;

public:
    PyComfortNoise(int sample_rate, int frame_size, float noise_level_db = -50.0f)
        : cng_(nullptr), frame_size_(frame_size) {

        voice_cng_config_t config;
        voice_cng_config_init(&config);
        config.sample_rate = sample_rate;
        config.frame_size = frame_size;
        config.noise_level_db = noise_level_db;

        cng_ = voice_cng_create(&config);
        if (!cng_) {
            throw std::runtime_error("Failed to create CNG");
        }
    }

    ~PyComfortNoise() {
        if (cng_) {
            voice_cng_destroy(cng_);
        }
    }

    void analyze(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        voice_cng_analyze(cng_, static_cast<int16_t*>(buf.ptr), buf.size);
    }

    py::array_t<int16_t> generate(int num_samples) {
        py::array_t<int16_t> output(num_samples);
        auto out_buf = output.request();

        voice_error_t err = voice_cng_generate(
            cng_, static_cast<int16_t*>(out_buf.ptr), num_samples);

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("CNG generation failed");
        }

        return output;
    }

    void set_level(float level_db) {
        voice_cng_set_level(cng_, level_db);
    }

    float get_level() const {
        return voice_cng_get_level(cng_);
    }

    void reset() {
        if (cng_) {
            voice_cng_reset(cng_);
        }
    }
};

/* ============================================
 * Module Initialization
 * ============================================ */

void init_dsp_bindings(py::module_& m) {
    // Denoiser
    py::class_<PyDenoiser>(m, "Denoiser", R"pbdoc(
        Noise reduction using spectral subtraction or RNNoise deep learning.

        Args:
            sample_rate: Audio sample rate in Hz (8000, 16000, 48000)
            frame_size: Number of samples per frame
            engine_type: 0=auto, 1=SpeexDSP, 2=RNNoise

        Example:
            >>> denoiser = Denoiser(16000, 160)
            >>> clean_audio = denoiser.process(noisy_audio)
    )pbdoc")
        .def(py::init<int, int, int>(),
             py::arg("sample_rate"),
             py::arg("frame_size"),
             py::arg("engine_type") = 0)
        .def("process", &PyDenoiser::process,
             py::arg("input"),
             "Process audio frame and return denoised output")
        .def("reset", &PyDenoiser::reset, "Reset denoiser state")
        .def_property_readonly("frame_size", &PyDenoiser::frame_size);

    // Echo Canceller
    py::class_<PyEchoCanceller>(m, "EchoCanceller", R"pbdoc(
        Acoustic Echo Canceller for full-duplex communication.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
            filter_length: Echo tail length in samples

        Example:
            >>> aec = EchoCanceller(16000, 160, 1600)
            >>> cleaned = aec.process(mic_audio, speaker_audio)
    )pbdoc")
        .def(py::init<int, int, int>(),
             py::arg("sample_rate"),
             py::arg("frame_size"),
             py::arg("filter_length") = 2000)
        .def("process", &PyEchoCanceller::process,
             py::arg("captured"), py::arg("playback"),
             "Process captured audio with playback reference")
        .def("reset", &PyEchoCanceller::reset, "Reset AEC state")
        .def_property_readonly("frame_size", &PyEchoCanceller::frame_size);

    // AGC
    py::class_<PyAGC>(m, "AGC", R"pbdoc(
        Automatic Gain Control for level normalization.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
            mode: AGC mode (0=fixed, 1=adaptive, 2=digital)
            target_level: Target level in dBFS

        Example:
            >>> agc = AGC(16000, 160, mode=1, target_level=-3.0)
            >>> normalized = agc.process(audio)
    )pbdoc")
        .def(py::init<int, int, int, float>(),
             py::arg("sample_rate"),
             py::arg("frame_size"),
             py::arg("mode") = 1,
             py::arg("target_level") = -3.0f)
        .def("process", &PyAGC::process, py::arg("input"),
             "Process audio frame with AGC")
        .def("get_gain", &PyAGC::get_gain, "Get current gain in dB")
        .def("reset", &PyAGC::reset, "Reset AGC state");

    // VAD
    py::class_<PyVAD>(m, "VAD", R"pbdoc(
        Voice Activity Detection.

        Args:
            sample_rate: Audio sample rate in Hz
            mode: VAD mode (0=quality, 1=low bitrate, 2=aggressive, 3=very aggressive)

        Example:
            >>> vad = VAD(16000, mode=1)
            >>> if vad.is_speech(audio_frame):
            ...     print("Voice detected!")
    )pbdoc")
        .def(py::init<int, int>(),
             py::arg("sample_rate"),
             py::arg("mode") = 1)
        .def("is_speech", &PyVAD::is_speech, py::arg("input"),
             "Check if audio frame contains speech")
        .def("get_probability", &PyVAD::get_probability,
             "Get speech probability (0.0 - 1.0)")
        .def("reset", &PyVAD::reset, "Reset VAD state");

    // Resampler
    py::class_<PyResampler>(m, "Resampler", R"pbdoc(
        High-quality sample rate converter.

        Args:
            channels: Number of audio channels
            in_rate: Input sample rate in Hz
            out_rate: Output sample rate in Hz
            quality: Quality level (0-10, higher is better)

        Example:
            >>> resampler = Resampler(1, 48000, 16000, quality=5)
            >>> resampled = resampler.process(audio)
    )pbdoc")
        .def(py::init<int, int, int, int>(),
             py::arg("channels"),
             py::arg("in_rate"),
             py::arg("out_rate"),
             py::arg("quality") = 5)
        .def("process", &PyResampler::process, py::arg("input"),
             "Resample audio data")
        .def("reset", &PyResampler::reset, "Reset resampler state");

    // DTMF Detector
    py::class_<PyDTMFDetector>(m, "DTMFDetector", R"pbdoc(
        DTMF (Dual-Tone Multi-Frequency) tone detector.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame

        Example:
            >>> detector = DTMFDetector(8000, 160)
            >>> digit = detector.process(audio_frame)
            >>> if digit:
            ...     print(f"Detected: {digit}")
    )pbdoc")
        .def(py::init<int, int>(),
             py::arg("sample_rate"),
             py::arg("frame_size"))
        .def("process", &PyDTMFDetector::process, py::arg("input"),
             "Process audio and return detected digit (empty if none)")
        .def("get_digits", &PyDTMFDetector::get_digits,
             "Get all accumulated digits")
        .def("clear_digits", &PyDTMFDetector::clear_digits,
             "Clear accumulated digits")
        .def("reset", &PyDTMFDetector::reset, "Reset detector state");

    // DTMF Generator
    py::class_<PyDTMFGenerator>(m, "DTMFGenerator", R"pbdoc(
        DTMF (Dual-Tone Multi-Frequency) tone generator.

        Args:
            sample_rate: Audio sample rate in Hz
            tone_duration_ms: Duration of each tone in milliseconds
            pause_duration_ms: Duration of pause between tones

        Example:
            >>> generator = DTMFGenerator(8000, 100, 50)
            >>> audio = generator.generate_sequence("123#")
    )pbdoc")
        .def(py::init<int, int, int>(),
             py::arg("sample_rate"),
             py::arg("tone_duration_ms") = 100,
             py::arg("pause_duration_ms") = 50)
        .def("generate_digit", &PyDTMFGenerator::generate_digit,
             py::arg("digit"), "Generate audio for a single DTMF digit")
        .def("generate_sequence", &PyDTMFGenerator::generate_sequence,
             py::arg("digits"), "Generate audio for a sequence of digits")
        .def("reset", &PyDTMFGenerator::reset, "Reset generator state");

    // Equalizer
    py::class_<PyEqualizer>(m, "Equalizer", R"pbdoc(
        Multi-band parametric equalizer.

        Args:
            sample_rate: Audio sample rate in Hz
            num_bands: Number of EQ bands (default 5)

        Example:
            >>> eq = Equalizer(48000, 5)
            >>> eq.set_band(0, 60, 3.0, 1.0)  # Boost bass
            >>> eq.set_band(4, 12000, 2.0, 1.0)  # Boost treble
            >>> processed = eq.process(audio)
    )pbdoc")
        .def(py::init<int, int>(),
             py::arg("sample_rate"),
             py::arg("num_bands") = 5)
        .def("process", &PyEqualizer::process, py::arg("input"),
             "Process audio through equalizer")
        .def("set_band", &PyEqualizer::set_band,
             py::arg("band_index"), py::arg("frequency"),
             py::arg("gain_db"), py::arg("q"),
             "Set EQ band parameters (index, freq Hz, gain dB, Q)")
        .def("set_master_gain", &PyEqualizer::set_master_gain,
             py::arg("gain_db"), "Set master output gain in dB")
        .def("apply_preset", &PyEqualizer::apply_preset,
             py::arg("preset"), "Apply EQ preset")
        .def("reset", &PyEqualizer::reset, "Reset EQ state");

    // Compressor
    py::class_<PyCompressor>(m, "Compressor", R"pbdoc(
        Dynamic range compressor.

        Args:
            sample_rate: Audio sample rate in Hz
            threshold_db: Compression threshold in dB
            ratio: Compression ratio (e.g., 4.0 for 4:1)
            attack_ms: Attack time in milliseconds
            release_ms: Release time in milliseconds

        Example:
            >>> comp = Compressor(48000, threshold_db=-20, ratio=4.0)
            >>> compressed = comp.process(audio)
    )pbdoc")
        .def(py::init<int, float, float, float, float>(),
             py::arg("sample_rate"),
             py::arg("threshold_db") = -20.0f,
             py::arg("ratio") = 4.0f,
             py::arg("attack_ms") = 10.0f,
             py::arg("release_ms") = 100.0f)
        .def("process", &PyCompressor::process, py::arg("input"),
             "Process audio through compressor")
        .def("set_threshold", &PyCompressor::set_threshold,
             py::arg("threshold_db"), "Set compression threshold")
        .def("set_ratio", &PyCompressor::set_ratio,
             py::arg("ratio"), "Set compression ratio")
        .def("set_times", &PyCompressor::set_times,
             py::arg("attack_ms"), py::arg("release_ms"),
             "Set attack and release times");

    // Comfort Noise Generator
    py::class_<PyComfortNoise>(m, "ComfortNoise", R"pbdoc(
        Comfort Noise Generator for silence substitution.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
            noise_level_db: Initial noise level in dB

        Example:
            >>> cng = ComfortNoise(16000, 160, -50)
            >>> cng.analyze(background_noise)
            >>> comfort_noise = cng.generate(160)
    )pbdoc")
        .def(py::init<int, int, float>(),
             py::arg("sample_rate"),
             py::arg("frame_size"),
             py::arg("noise_level_db") = -50.0f)
        .def("analyze", &PyComfortNoise::analyze, py::arg("input"),
             "Analyze background noise characteristics")
        .def("generate", &PyComfortNoise::generate, py::arg("num_samples"),
             "Generate comfort noise samples")
        .def("set_level", &PyComfortNoise::set_level, py::arg("level_db"),
             "Set noise level in dB")
        .def("get_level", &PyComfortNoise::get_level,
             "Get current noise level in dB")
        .def("reset", &PyComfortNoise::reset, "Reset CNG state");

    // EQ Presets enum
    py::enum_<voice_eq_preset_t>(m, "EQPreset")
        .value("FLAT", VOICE_EQ_PRESET_FLAT)
        .value("VOICE_ENHANCE", VOICE_EQ_PRESET_VOICE_ENHANCE)
        .value("TELEPHONE", VOICE_EQ_PRESET_TELEPHONE)
        .value("BASS_BOOST", VOICE_EQ_PRESET_BASS_BOOST)
        .value("TREBLE_BOOST", VOICE_EQ_PRESET_TREBLE_BOOST)
        .value("REDUCE_NOISE", VOICE_EQ_PRESET_REDUCE_NOISE)
        .value("CLARITY", VOICE_EQ_PRESET_CLARITY)
        .export_values();
}
