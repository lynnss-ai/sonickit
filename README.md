# SonicKit

A cross-platform real-time audio processing library written in pure C.

[中文文档](README_zh.md) | English

## Features

### Audio Processing
- 🎙️ **Audio Capture/Playback** - Based on miniaudio, supports all major platforms
- 🔇 **Noise Reduction** - Dual engine (SpeexDSP + RNNoise neural network)
- 📢 **Echo Cancellation** (AEC) - SpeexDSP implementation
- 🎚️ **Automatic Gain Control** (AGC) - Fixed/adaptive/digital gain modes
- 🔄 **High-Quality Resampling** - SpeexDSP resampler (quality 0-10)
- 🎙️ **Voice Activity Detection** (VAD) - Energy/zero-crossing/spectral entropy analysis
- 🎛️ **Audio Level Monitoring** - RMS/Peak/VU/LUFS measurements
- 📈 **Audio Quality Analysis** - POLQA/PESQ style MOS scoring

### DSP Processing
- 🔊 **Equalizer** - Multi-band parametric EQ, 7 filter types, preset configurations
- 📉 **Dynamic Compressor** - Compressor/limiter/expander/gate, soft knee
- 📞 **DTMF** - Dual-tone multi-frequency detection (Goertzel algorithm) and generation
- 🔉 **Comfort Noise Generation** (CNG) - White/pink/brown/spectral matching noise, RFC 3389 SID frames

### Codecs
- 🎵 **Opus** - High-quality voice/music encoding (6-510 kbps)
- 📞 **G.711** - A-law/μ-law traditional telephony encoding
- 📡 **G.722** - Wideband voice encoding (16kHz)

### Network Transport
- 📦 **RTP/RTCP** - Real-time Transport Protocol
- 🔐 **SRTP** - Encrypted transport (AES-CM/AES-GCM)
- 📊 **Adaptive Jitter Buffer** - Dynamic delay adjustment
- 🔧 **Packet Loss Concealment** (PLC) - Multiple algorithms
- 📶 **Bandwidth Estimation** - GCC/REMB/BBR congestion control
- 🌐 **ICE/STUN/TURN** - NAT traversal and connection establishment
- 🚀 **Transport Layer Abstraction** - UDP/TCP socket encapsulation, QoS/DSCP

### Audio Tools
- 🎧 **Audio Mixer** - Multi-stream mixing, independent gain/mute
- 📼 **Audio Recorder** - WAV/RAW file recording, memory buffering, playback control
- 📊 **Statistics Collector** - Audio/codec/network statistics, MOS/R-Factor calculation

### File I/O
- 📁 **WAV** - Read/write support
- 🎶 **MP3** - Decoding support
- 🎼 **FLAC** - Lossless audio support

### Platform Support
| Platform | Audio Backend | Status |
|----------|---------------|--------|
| Windows | WASAPI | ✅ |
| macOS | Core Audio | ✅ |
| Linux | ALSA/PulseAudio | ✅ |
| iOS | AVAudioSession + Core Audio | ✅ |
| Android | AAudio/OpenSL ES | ✅ |
| **WebAssembly** | Emscripten | ✅ |

## Project Structure

```
sonickit/
├── include/
│   ├── voice/           # Core headers
│   │   ├── types.h      # Basic type definitions
│   │   ├── error.h      # Error codes and logging
│   │   ├── config.h     # Configuration structures
│   │   ├── voice.h      # Main API
│   │   ├── pipeline.h   # Processing pipeline
│   │   ├── platform.h   # Platform abstraction
│   │   └── statistics.h # Statistics collection
│   ├── audio/           # Audio modules
│   │   ├── audio_buffer.h    # Ring buffer
│   │   ├── device.h          # Device management
│   │   ├── file_io.h         # File read/write
│   │   ├── audio_mixer.h     # Audio mixing
│   │   ├── audio_level.h     # Level monitoring
│   │   ├── audio_quality.h   # Quality analysis
│   │   └── audio_recorder.h  # Recording/playback
│   ├── dsp/             # DSP modules
│   │   ├── resampler.h       # Resampling
│   │   ├── denoiser.h        # Noise reduction
│   │   ├── echo_canceller.h  # Echo cancellation
│   │   ├── vad.h             # Voice activity detection
│   │   ├── agc.h             # Automatic gain control
│   │   ├── comfort_noise.h   # Comfort noise
│   │   ├── dtmf.h            # DTMF detection/generation
│   │   ├── equalizer.h       # Parametric equalizer
│   │   └── compressor.h      # Dynamic compressor
│   ├── codec/           # Codecs
│   │   ├── codec.h           # Codec interface
│   │   ├── g711.h            # G.711 codec
│   │   ├── g722.h            # G.722 codec
│   │   └── opus.h            # Opus codec
│   ├── network/         # Network modules
│   │   ├── rtp.h             # RTP protocol
│   │   ├── srtp.h            # SRTP encryption
│   │   ├── jitter_buffer.h   # Jitter buffer
│   │   ├── bandwidth_estimator.h  # Bandwidth estimation
│   │   ├── ice.h             # ICE/STUN/TURN
│   │   └── transport.h       # Transport layer abstraction
│   ├── sip/             # SIP protocol modules
│   │   ├── sip_core.h        # SIP core protocol
│   │   └── sip_ua.h          # SIP user agent
│   └── utils/           # Utility modules
│       ├── diagnostics.h     # Diagnostic tools
│       └── simd_utils.h      # SIMD optimizations
├── src/                 # Implementation files
├── examples/            # Example programs
├── wasm/                # WebAssembly support
│   ├── api/                  # JS binding API
│   ├── platform/             # WASM platform layer
│   ├── stubs/                # Stub functions
│   └── examples/             # Browser demos
├── docs/                # Documentation
│   └── NEW_FEATURES.md  # New features documentation
├── third_party/         # Third-party libraries
└── CMakeLists.txt
```

## Building

### Dependencies

**Required:**
- CMake 3.16+
- C11 compiler (GCC, Clang, MSVC, or MinGW)

**Optional (auto-detected or manually enabled):**
- SpeexDSP - Resampling, noise reduction, AEC
- RNNoise - Neural network noise reduction
- Opus - Opus codec support
- libsrtp2 - SRTP encryption
- OpenSSL - DTLS-SRTP key exchange

### Quick Build (Minimal, No External Dependencies)

```bash
# Clone repository
git clone <repo>
cd sonickit

# Create build directory
mkdir build && cd build

# Configure with minimal features (no external dependencies required)
cmake .. -DSONICKIT_ENABLE_OPUS=OFF \
         -DSONICKIT_ENABLE_RNNOISE=OFF \
         -DSONICKIT_ENABLE_SRTP=OFF \
         -DSONICKIT_ENABLE_DTLS=OFF

# Build
cmake --build .
```

### Build with All Features

```bash
# Configure with all features (requires external dependencies)
cmake .. -DSONICKIT_ENABLE_OPUS=ON \
         -DSONICKIT_ENABLE_RNNOISE=ON \
         -DSONICKIT_ENABLE_SRTP=ON \
         -DSONICKIT_ENABLE_DTLS=ON \
         -DSONICKIT_ENABLE_G722=ON

# Build
cmake --build . --config Release

# Run tests
ctest -C Release
```

### Platform-Specific Instructions

#### Windows (MinGW)

```powershell
# Using MinGW Makefiles
mkdir build; cd build
cmake .. -G "MinGW Makefiles" -DSONICKIT_BUILD_TESTS=ON -DSONICKIT_BUILD_EXAMPLES=ON
cmake --build .
```

#### Windows (Visual Studio)

```powershell
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

#### Linux / macOS

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `SONICKIT_BUILD_EXAMPLES` | ON | Build example programs |
| `SONICKIT_BUILD_TESTS` | ON | Build test programs |
| `SONICKIT_ENABLE_OPUS` | ON | Enable Opus codec (requires libopus) |
| `SONICKIT_ENABLE_G722` | OFF | Enable G.722 codec |
| `SONICKIT_ENABLE_SPEEX` | OFF | Enable Speex codec |
| `SONICKIT_ENABLE_RNNOISE` | ON | Enable RNNoise neural network noise reduction |
| `SONICKIT_ENABLE_SRTP` | ON | Enable SRTP encryption (requires libsrtp2) |
| `SONICKIT_ENABLE_DTLS` | ON | Enable DTLS-SRTP key exchange (requires OpenSSL) |

### Build Outputs

After a successful build, you will find:

```
build/
├── libvoice.a              # Static library
├── test_buffer.exe         # Ring buffer tests
├── test_resampler.exe      # Resampler tests
├── test_codec.exe          # Codec tests
├── test_dsp.exe            # DSP module tests
├── test_network.exe        # Network module tests
├── test_effects.exe        # Audio effects tests
├── test_watermark.exe      # Watermark tests
├── test_diagnostics.exe    # Diagnostics tests
├── test_datachannel.exe    # DataChannel tests
├── test_sip.exe            # SIP protocol tests
├── benchmark_simd.exe      # SIMD performance benchmark
├── benchmark_dsp.exe       # DSP performance benchmark
└── examples/
    ├── example_capture.exe     # Audio capture demo
    ├── example_playback.exe    # Audio playback demo
    ├── example_file_convert.exe # File conversion demo
    └── example_voicechat.exe   # Voice chat demo
```

### Running Tests

```bash
# Run all tests
ctest --test-dir build -C Debug --output-on-failure

# Run specific test
./build/test_codec.exe
```

### Performance Benchmarks

SonicKit includes performance benchmarks for SIMD optimizations and DSP modules.

```bash
# SIMD benchmark (format conversion, gain, mixing, etc.)
./build/benchmark_simd.exe

# DSP benchmark (AEC, time stretcher, delay estimator)
./build/benchmark_dsp.exe
```

**Command-line options:**

| Option | Description |
|--------|-------------|
| `-n, --iterations N` | Set number of iterations (default: 10000 for SIMD, 1000 for DSP) |
| `-h, --help` | Show help message |

**Examples:**

```bash
# Run SIMD benchmark with 50000 iterations
./build/benchmark_simd.exe -n 50000

# Run DSP benchmark with 5000 iterations
./build/benchmark_dsp.exe --iterations 5000
```

**Sample output:**

```
+================================================================+
|          SonicKit SIMD Performance Benchmark                   |
+================================================================+

SIMD Capabilities: AVX2 SSE4.1 SSE2
Test buffer size: 4096 samples
Iterations: 10000

===================================================================
                     Format Conversion Tests
===================================================================

Benchmark: int16_to_float
  Iterations: 10000
  Mean:     245.32 ns
  Median:   241.00 ns
  Std Dev:   32.45 ns
  Min/Max:  235.00 / 412.00 ns
  P95/P99:  298.00 / 345.00 ns
  Throughput: 16.70 Gsamples/sec
```

### WebAssembly (Browser) Build

SonicKit can be compiled to WebAssembly for real-time audio processing in browsers.

**Prerequisites:**
- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)

```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh  # Linux/macOS
# or emsdk_env.bat      # Windows

# Build WASM
cd sonickit/wasm
mkdir build && cd build
emcmake cmake .. -DWASM_ENABLE_OPUS=OFF -DWASM_ENABLE_RNNOISE=OFF
emmake make -j8

# Output files
# build/sonickit.js
# build/sonickit.wasm
```

**Browser Usage Example:**

```javascript
// Load SonicKit WASM
const sonicKit = await Module();

// Create denoiser (48kHz, 480 samples/frame)
const denoiser = new sonicKit.Denoiser(48000, 480, 0);

// Process audio (Int16Array)
const processedSamples = denoiser.process(inputSamples);

// Cleanup
denoiser.delete();
```

**JavaScript API Classes:**

| Class | Constructor | Methods |
|-------|-------------|--------|
| `Denoiser` | `(sampleRate, frameSize, engine)` | `process()`, `reset()` |
| `EchoCanceller` | `(sampleRate, frameSize, filterLen)` | `process()`, `reset()` |
| `AGC` | `(sampleRate, frameSize, mode, target)` | `process()`, `getGain()`, `reset()` |
| `VAD` | `(sampleRate, frameSize, mode)` | `isSpeech()`, `getProbability()`, `reset()` |
| `Resampler` | `(channels, inRate, outRate, quality)` | `process()`, `reset()` |
| `G711Codec` | `(useAlaw)` | `encode()`, `decode()` |

For more WASM details, see [wasm/README.md](wasm/README.md).

## Module Overview

| Module | Function | Key Features |
|--------|----------|--------------|
| **audio_device** | Audio device | Capture/playback, cross-platform |
| **audio_buffer** | Ring buffer | Thread-safe, zero-copy |
| **audio_mixer** | Audio mixing | Multi-stream mix, independent gain |
| **audio_level** | Level monitoring | RMS/Peak/VU/LUFS |
| **audio_quality** | Quality analysis | MOS scoring, SNR estimation |
| **audio_recorder** | Recording/playback | WAV files, memory buffer |
| **resampler** | Resampling | Quality 0-10, arbitrary ratio |
| **denoiser** | Noise reduction | SpeexDSP + RNNoise |
| **echo_canceller** | Echo cancellation | Adaptive filtering |
| **vad** | Voice detection | Energy/zero-crossing/spectral entropy |
| **agc** | Gain control | Fixed/adaptive/digital |
| **comfort_noise** | Comfort noise | White/pink/brown/spectral matching |
| **dtmf** | DTMF | Goertzel detection/generation |
| **equalizer** | Equalizer | Multi-band, 7 filter types |
| **compressor** | Compressor | Soft knee, sidechain |
| **codec** | Encoding/decoding | Opus/G.711/G.722 |
| **rtp** | RTP protocol | Packetization/parsing, RTCP |
| **srtp** | SRTP encryption | AES-CM/AES-GCM |
| **jitter_buffer** | Jitter buffer | Adaptive, PLC |
| **bandwidth_estimator** | Bandwidth estimation | GCC/REMB/BBR |
| **ice** | NAT traversal | ICE/STUN/TURN |
| **transport** | Transport layer | UDP/TCP, QoS |
| **sip_core** | SIP Protocol | RFC 3261, Register/Call |
| **sip_ua** | SIP User Agent | Call management, Session control |
| **diagnostics** | Diagnostics | Performance analysis, Debug logging |
| **simd_utils** | SIMD Optimization | SSE/AVX/NEON acceleration |
| **statistics** | Statistics collection | MOS/R-Factor, JSON |

## Quick Start

### Simple Capture

```c
#include "voice/voice.h"
#include "audio/device.h"

void capture_callback(voice_device_t *dev, const int16_t *input,
                     size_t samples, void *user_data) {
    // Process captured audio data
}

int main() {
    voice_device_config_t config;
    voice_device_config_init(&config);
    config.mode = VOICE_DEVICE_MODE_CAPTURE;
    config.sample_rate = 48000;
    config.channels = 1;
    config.capture_callback = capture_callback;

    voice_device_t *device = voice_device_create(&config);
    voice_device_start(device);

    // ... run ...

    voice_device_stop(device);
    voice_device_destroy(device);
    return 0;
}
```

### Full Duplex Voice Chat

```c
#include "voice/pipeline.h"

int main() {
    voice_pipeline_config_t config;
    voice_pipeline_config_init(&config);
    config.mode = PIPELINE_MODE_DUPLEX;
    config.enable_aec = true;
    config.enable_denoise = true;
    config.codec = VOICE_CODEC_OPUS;

    voice_pipeline_t *pipeline = voice_pipeline_create(&config);

    voice_pipeline_set_encoded_callback(pipeline, on_send_packet, NULL);
    voice_pipeline_start(pipeline);

    // Receive remote data
    voice_pipeline_receive_packet(pipeline, data, size);

    // ... run ...

    voice_pipeline_stop(pipeline);
    voice_pipeline_destroy(pipeline);
    return 0;
}
```

## Example Programs

- **example_capture** - Microphone capture recording
- **example_playback** - Audio file playback
- **example_file_convert** - Audio format conversion
- **example_voicechat** - Full duplex voice chat

```bash
# Record for 10 seconds
./example_capture -o recording.wav -d 10

# Play audio
./example_playback music.mp3

# Format conversion + noise reduction
./example_file_convert input.mp3 output.wav -r 16000 -n

# Voice chat (two machines)
# Machine A
./example_voicechat -p 5004

# Machine B
./example_voicechat -p 5005 -c <Machine_A_IP> -r 5004
```

## API Documentation

### Audio Device

```c
// Enumerate devices
voice_device_info_t devices[10];
size_t count = 10;
voice_device_enumerate(VOICE_DEVICE_MODE_CAPTURE, devices, &count);

// Create device
voice_device_t *device = voice_device_create(&config);
voice_device_start(device);
voice_device_stop(device);
voice_device_destroy(device);
```

### Noise Reduction

```c
voice_denoiser_config_t config;
voice_denoiser_config_init(&config);
config.sample_rate = 48000;
config.engine = VOICE_DENOISE_RNNOISE;  // Or VOICE_DENOISE_SPEEX

voice_denoiser_t *dn = voice_denoiser_create(&config);
voice_denoiser_process(dn, pcm_buffer, sample_count);
voice_denoiser_destroy(dn);
```

### Encoding/Decoding

```c
// Encode
voice_encoder_t *enc = voice_encoder_create(&codec_config);
voice_encoder_encode(enc, pcm, samples, output, &output_size);

// Decode
voice_decoder_t *dec = voice_decoder_create(&codec_config);
voice_decoder_decode(dec, input, input_size, pcm, &samples);
```

### Network

```c
// RTP
rtp_session_t *rtp = rtp_session_create(&config);
rtp_session_create_packet(rtp, payload, len, timestamp, marker, packet, &size);
rtp_session_parse_packet(rtp, data, len, &packet);

// SRTP
srtp_session_t *srtp = srtp_session_create(&config);
srtp_protect(srtp, packet, &size, max_size);
srtp_unprotect(srtp, packet, &size);
```

## Mobile Platform Notes

### iOS

```objc
// Configure session before using audio
voice_session_config_t config;
voice_session_config_init(&config);
config.category = VOICE_SESSION_CATEGORY_PLAY_AND_RECORD;
config.mode = VOICE_SESSION_MODE_VOICE_CHAT;

voice_session_configure(&config);
voice_session_activate();

// Handle interruptions
voice_session_set_interrupt_callback(on_interrupt, user_data);
```

### Android

Initialization required in Java layer:

```java
public class VoiceLib {
    static {
        System.loadLibrary("voice");
    }

    public static native void nativeInit(Context context);
    public static native void nativeRelease();
}

// Initialize
VoiceLib.nativeInit(getApplicationContext());
```

## License

MIT License

## Contributing

Issues and Pull Requests are welcome!

## Detailed Documentation

For more feature details, please refer to [docs/NEW_FEATURES.md](docs/NEW_FEATURES.md), including:
- Detailed API documentation for each module
- Complete usage examples
- Performance optimization recommendations
- Mobile platform adaptation guide
