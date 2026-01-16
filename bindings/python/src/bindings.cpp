/**
 * @file bindings.cpp
 * @brief Main pybind11 module definition for SonicKit Python bindings
 * @author SonicKit Team
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for submodule init functions
void init_dsp_bindings(py::module_& m);
void init_codec_bindings(py::module_& m);
void init_audio_bindings(py::module_& m);
void init_effects_bindings(py::module_& m);

PYBIND11_MODULE(_sonickit, m) {
    m.doc() = R"pbdoc(
        SonicKit - High-Performance Audio Processing Library
        =====================================================

        SonicKit provides professional-grade audio processing capabilities
        for Python applications, including:

        - Noise reduction (RNNoise / SpeexDSP)
        - Acoustic echo cancellation (AEC)
        - Automatic gain control (AGC)
        - Voice activity detection (VAD)
        - Sample rate conversion
        - Audio effects (EQ, compressor, reverb, etc.)
        - Spatial audio (3D positioning, HRTF)
        - Audio codecs (G.711, Opus)
        - Audio watermarking

        Example:
            >>> import sonickit
            >>> denoiser = sonickit.Denoiser(sample_rate=16000, frame_size=160)
            >>> clean_audio = denoiser.process(noisy_audio)
    )pbdoc";

    // Version info
    m.attr("__version__") = "1.0.0";
    m.attr("__author__") = "SonicKit Team";

    // Initialize submodules
    init_dsp_bindings(m);
    init_codec_bindings(m);
    init_audio_bindings(m);
    init_effects_bindings(m);
}
