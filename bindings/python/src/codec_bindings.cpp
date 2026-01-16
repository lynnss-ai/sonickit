/**
 * @file codec_bindings.cpp
 * @brief Codec module Python bindings (G.711, Opus)
 * @author SonicKit Team
 */

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include <vector>
#include <cstring>

extern "C" {
#include "codec/codec.h"
#include "voice/error.h"
}

namespace py = pybind11;

/* ============================================
 * G.711 Codec Wrapper
 * ============================================ */

class PyG711Codec {
private:
    voice_encoder_t* encoder_;
    voice_decoder_t* decoder_;
    bool use_alaw_;

public:
    PyG711Codec(bool use_alaw = true)
        : encoder_(nullptr), decoder_(nullptr), use_alaw_(use_alaw) {

        voice_codec_detail_config_t config;
        std::memset(&config, 0, sizeof(config));
        config.codec_id = use_alaw ? VOICE_CODEC_G711_ALAW : VOICE_CODEC_G711_ULAW;
        config.u.g711.sample_rate = 8000;
        config.u.g711.use_alaw = use_alaw;

        encoder_ = voice_encoder_create(&config);
        decoder_ = voice_decoder_create(&config);

        if (!encoder_ || !decoder_) {
            if (encoder_) voice_encoder_destroy(encoder_);
            if (decoder_) voice_decoder_destroy(decoder_);
            throw std::runtime_error("Failed to create G.711 codec");
        }
    }

    ~PyG711Codec() {
        if (encoder_) voice_encoder_destroy(encoder_);
        if (decoder_) voice_decoder_destroy(decoder_);
    }

    py::array_t<uint8_t> encode(py::array_t<int16_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        std::vector<uint8_t> encoded(buf.size);
        size_t encoded_size = encoded.size();

        voice_error_t err = voice_encoder_encode(
            encoder_,
            static_cast<int16_t*>(buf.ptr), buf.size,
            encoded.data(), &encoded_size
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("G.711 encode failed");
        }

        py::array_t<uint8_t> output(encoded_size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, encoded.data(), encoded_size);

        return output;
    }

    py::array_t<int16_t> decode(py::array_t<uint8_t> input) {
        py::buffer_info buf = input.request();
        if (buf.ndim != 1) {
            throw std::runtime_error("Input must be 1-dimensional");
        }

        std::vector<int16_t> decoded(buf.size);
        size_t decoded_size = decoded.size();

        voice_error_t err = voice_decoder_decode(
            decoder_,
            static_cast<uint8_t*>(buf.ptr), buf.size,
            decoded.data(), &decoded_size
        );

        if (err != VOICE_SUCCESS) {
            throw std::runtime_error("G.711 decode failed");
        }

        py::array_t<int16_t> output(decoded_size);
        auto out_buf = output.request();
        std::memcpy(out_buf.ptr, decoded.data(), decoded_size * sizeof(int16_t));

        return output;
    }

    bool is_alaw() const { return use_alaw_; }
};

/* ============================================
 * Module Initialization
 * ============================================ */

void init_codec_bindings(py::module_& m) {
    // G.711 Codec
    py::class_<PyG711Codec>(m, "G711Codec", R"pbdoc(
        G.711 audio codec (A-law and μ-law).

        G.711 is a standard codec used in telephony, providing 64 kbps
        encoding at 8 kHz sample rate.

        Args:
            use_alaw: True for A-law, False for μ-law (default True)

        Example:
            >>> codec = G711Codec(use_alaw=True)
            >>> encoded = codec.encode(pcm_audio)
            >>> decoded = codec.decode(encoded)
    )pbdoc")
        .def(py::init<bool>(),
             py::arg("use_alaw") = true)
        .def("encode", &PyG711Codec::encode, py::arg("input"),
             "Encode PCM audio to G.711")
        .def("decode", &PyG711Codec::decode, py::arg("input"),
             "Decode G.711 to PCM audio")
        .def_property_readonly("is_alaw", &PyG711Codec::is_alaw,
             "True if using A-law, False for μ-law");

    // Codec IDs enum
    py::enum_<voice_codec_id_t>(m, "CodecID")
        .value("G711_ALAW", VOICE_CODEC_G711_ALAW)
        .value("G711_ULAW", VOICE_CODEC_G711_ULAW)
        .value("OPUS", VOICE_CODEC_OPUS)
        .value("G722", VOICE_CODEC_G722)
        .export_values();
}
