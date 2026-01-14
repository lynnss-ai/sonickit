# SonicKit User Guide

A comprehensive guide for using the SonicKit real-time audio processing library.

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Quick Start](#quick-start)
4. [Core Modules](#core-modules)
5. [Audio Device Management](#audio-device-management)
6. [DSP Processing](#dsp-processing)
7. [Codec Usage](#codec-usage)
8. [Network Transmission](#network-transmission)
9. [Pipeline Integration](#pipeline-integration)
10. [Platform Adaptation](#platform-adaptation)
11. [Performance Optimization](#performance-optimization)
12. [FAQ](#faq)
13. [Glossary](#glossary)

---

## Introduction

SonicKit is a cross-platform real-time audio processing library written in pure C. It provides complete solutions for voice communication applications, including:

- **Audio I/O**: Capture and playback on all major platforms
- **Audio Processing**: Noise reduction, echo cancellation, AGC, resampling
- **Codec Support**: Opus, G.711, G.722
- **Network Transport**: RTP/RTCP, SRTP, jitter buffer, ICE/STUN/TURN
- **Utility Tools**: Mixer, recorder, quality analysis

### Design Philosophy

- **Pure C Implementation**: Maximum compatibility, easy integration with other languages
- **Modular Architecture**: Use only what you need
- **Cross-Platform**: Windows, macOS, Linux, iOS, Android, WebAssembly
- **Low Latency**: Optimized for real-time voice communication
- **Thread-Safe**: Safe for multi-threaded usage

---

## Installation

### Prerequisites

- CMake 3.14 or higher
- C11 compatible compiler (GCC 4.9+, Clang 3.4+, MSVC 2015+)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/your-repo/sonickit.git
cd sonickit

# Create build directory
mkdir build && cd build

# Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Install (optional)
cmake --install . --prefix /usr/local
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `VOICE_ENABLE_OPUS` | ON | Enable Opus codec |
| `VOICE_ENABLE_G722` | ON | Enable G.722 codec |
| `VOICE_ENABLE_RNNOISE` | OFF | Enable RNNoise neural network denoiser |
| `VOICE_ENABLE_SRTP` | OFF | Enable SRTP encryption |
| `VOICE_BUILD_EXAMPLES` | ON | Build example programs |
| `VOICE_BUILD_TESTS` | ON | Build unit tests |

### Linking in Your Project

```cmake
find_package(SonicKit REQUIRED)
target_link_libraries(your_app PRIVATE SonicKit::SonicKit)
```

---

## Quick Start

### Minimal Example: Audio Capture

```c
#include "voice/voice.h"
#include "audio/device.h"
#include <stdio.h>

// Callback function for captured audio
void on_capture(voice_device_t *dev, const int16_t *samples, 
                size_t count, void *user_data) {
    printf("Captured %zu samples\n", count);
}

int main() {
    // Initialize library
    voice_init(NULL);
    
    // Configure device
    voice_device_config_t config;
    voice_device_config_init(&config);
    config.mode = VOICE_DEVICE_MODE_CAPTURE;
    config.sample_rate = 48000;
    config.channels = 1;
    config.frame_size = 480;  // 10ms at 48kHz
    config.capture_callback = on_capture;
    
    // Create and start device
    voice_device_t *device = voice_device_create(&config);
    if (!device) {
        fprintf(stderr, "Failed to create device\n");
        return 1;
    }
    
    voice_device_start(device);
    
    // Capture for 5 seconds
    voice_sleep_ms(5000);
    
    // Cleanup
    voice_device_stop(device);
    voice_device_destroy(device);
    voice_deinit();
    
    return 0;
}
```

### Minimal Example: Noise Reduction

```c
#include "dsp/denoiser.h"

// Create denoiser
voice_denoiser_config_t config;
voice_denoiser_config_init(&config);
config.sample_rate = 16000;
config.frame_size = 160;  // 10ms frame
config.engine = VOICE_DENOISE_RNNOISE;

voice_denoiser_t *denoiser = voice_denoiser_create(&config);

// Process audio frames
int16_t frame[160];
// ... fill frame with audio data ...
voice_denoiser_process(denoiser, frame, 160);

// Cleanup
voice_denoiser_destroy(denoiser);
```

---

## Core Modules

### Module Dependencies

```
voice/voice.h          <- Main API entry point
    |
    +-- audio/device.h       <- Audio I/O
    +-- dsp/denoiser.h       <- Noise reduction
    +-- dsp/echo_canceller.h <- Echo cancellation
    +-- dsp/agc.h            <- Automatic gain control
    +-- dsp/resampler.h      <- Sample rate conversion
    +-- codec/codec.h        <- Audio encoding/decoding
    +-- network/rtp.h        <- RTP packetization
    +-- network/srtp.h       <- SRTP encryption
    +-- voice/pipeline.h     <- Full processing pipeline
```

### Initialization

Always initialize the library before using any modules:

```c
#include "voice/voice.h"

int main() {
    // Optional: Configure logging
    voice_global_config_t global_config;
    voice_global_config_init(&global_config);
    global_config.log_level = VOICE_LOG_DEBUG;
    
    // Initialize
    voice_error_t err = voice_init(&global_config);
    if (err != VOICE_OK) {
        fprintf(stderr, "Init failed: %s\n", voice_error_string(err));
        return 1;
    }
    
    // ... use library ...
    
    // Cleanup
    voice_deinit();
    return 0;
}
```

---

## Audio Device Management

### Enumerating Devices

```c
#include "audio/device.h"

void list_devices() {
    voice_device_info_t devices[16];
    size_t count = 16;
    
    // List capture devices
    voice_device_enumerate(VOICE_DEVICE_MODE_CAPTURE, devices, &count);
    printf("Capture devices:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  [%zu] %s (ID: %s)\n", i, devices[i].name, devices[i].id);
    }
    
    // List playback devices
    count = 16;
    voice_device_enumerate(VOICE_DEVICE_MODE_PLAYBACK, devices, &count);
    printf("Playback devices:\n");
    for (size_t i = 0; i < count; i++) {
        printf("  [%zu] %s (ID: %s)\n", i, devices[i].name, devices[i].id);
    }
}
```

### Full-Duplex Audio

```c
// Capture callback
void on_capture(voice_device_t *dev, const int16_t *input, 
                size_t samples, void *user_data) {
    // Process captured audio
}

// Playback callback
void on_playback(voice_device_t *dev, int16_t *output, 
                 size_t samples, void *user_data) {
    // Fill output buffer
}

// Configure duplex device
voice_device_config_t config;
voice_device_config_init(&config);
config.mode = VOICE_DEVICE_MODE_DUPLEX;
config.sample_rate = 48000;
config.channels = 1;
config.frame_size = 480;
config.capture_callback = on_capture;
config.playback_callback = on_playback;
config.user_data = my_context;

voice_device_t *device = voice_device_create(&config);
voice_device_start(device);
```

---

## DSP Processing

### Noise Reduction

SonicKit supports two noise reduction engines:

| Engine | Description | Best For |
|--------|-------------|----------|
| `VOICE_DENOISE_SPEEX` | SpeexDSP spectral subtraction | Low CPU, basic noise |
| `VOICE_DENOISE_RNNOISE` | RNN-based neural network | High quality, complex noise |

```c
voice_denoiser_config_t config;
voice_denoiser_config_init(&config);
config.sample_rate = 48000;
config.frame_size = 480;
config.engine = VOICE_DENOISE_RNNOISE;
config.denoise_level = 80;  // 0-100

voice_denoiser_t *dn = voice_denoiser_create(&config);

// Process in-place
voice_denoiser_process(dn, pcm_frame, frame_size);

// Get noise reduction amount
float reduction_db = voice_denoiser_get_reduction(dn);
```

### Echo Cancellation (AEC)

```c
voice_aec_config_t config;
voice_aec_config_init(&config);
config.sample_rate = 16000;
config.frame_size = 160;
config.filter_length_ms = 200;  // Echo tail length
config.nlp_mode = VOICE_AEC_NLP_MODERATE;

voice_aec_t *aec = voice_aec_create(&config);

// Process: capture with playback reference
int16_t output[160];
voice_aec_process(aec, capture_frame, playback_frame, output, 160);
```

### Automatic Gain Control (AGC)

```c
voice_agc_config_t config;
voice_agc_config_init(&config);
config.sample_rate = 16000;
config.frame_size = 160;
config.mode = VOICE_AGC_ADAPTIVE_DIGITAL;
config.target_level_dbfs = -3.0f;

voice_agc_t *agc = voice_agc_create(&config);

// Process in-place
voice_agc_process(agc, pcm_frame, frame_size);

// Get current gain
float gain = voice_agc_get_gain(agc);
```

### Resampling

```c
voice_resampler_config_t config;
voice_resampler_config_init(&config);
config.input_rate = 48000;
config.output_rate = 16000;
config.channels = 1;
config.quality = 5;  // 0-10, higher = better quality

voice_resampler_t *resampler = voice_resampler_create(&config);

// Calculate output size
int output_frames = (input_frames * 16000) / 48000;
int16_t output[output_frames];
int actual_output;

voice_resampler_process(resampler, input, input_frames, 
                        output, output_frames, &actual_output);
```

---

## Codec Usage

### Opus Encoding/Decoding

```c
// Encoder
voice_encoder_config_t enc_config;
voice_encoder_config_init(&enc_config);
enc_config.codec = VOICE_CODEC_OPUS;
enc_config.sample_rate = 48000;
enc_config.channels = 1;
enc_config.bitrate = 32000;
enc_config.application = VOICE_OPUS_APP_VOIP;

voice_encoder_t *encoder = voice_encoder_create(&enc_config);

// Encode
uint8_t encoded[256];
int encoded_size = voice_encoder_encode(encoder, pcm_frame, 960, 
                                        encoded, sizeof(encoded));

// Decoder
voice_decoder_config_t dec_config;
voice_decoder_config_init(&dec_config);
dec_config.codec = VOICE_CODEC_OPUS;
dec_config.sample_rate = 48000;
dec_config.channels = 1;

voice_decoder_t *decoder = voice_decoder_create(&dec_config);

// Decode
int16_t decoded[960];
int decoded_samples = voice_decoder_decode(decoder, encoded, encoded_size,
                                           decoded, 960);
```

---

## Network Transmission

### RTP Session

```c
// Create RTP session
rtp_session_config_t config;
rtp_session_config_init(&config);
config.payload_type = 111;  // Opus
config.clock_rate = 48000;

rtp_session_t *rtp = rtp_session_create(&config);

// Create RTP packet
uint8_t packet[1500];
size_t packet_size;
rtp_session_create_packet(rtp, encoded_data, encoded_size, 
                          timestamp, false, packet, &packet_size);

// Parse received packet
rtp_packet_t parsed;
rtp_session_parse_packet(rtp, received_data, received_size, &parsed);
```

### Jitter Buffer

```c
jitter_buffer_config_t config;
jitter_buffer_config_init(&config);
config.sample_rate = 48000;
config.frame_size = 960;
config.min_delay_ms = 20;
config.max_delay_ms = 200;

jitter_buffer_t *jb = jitter_buffer_create(&config);

// Push received packets
jitter_buffer_push(jb, timestamp, encoded_data, encoded_size);

// Pull frames for playback
uint8_t frame_data[256];
size_t frame_size;
jitter_buffer_status_t status;
jitter_buffer_pull(jb, frame_data, &frame_size, &status);
```

---

## Pipeline Integration

The pipeline module provides a complete, integrated solution:

```c
voice_pipeline_config_t config;
voice_pipeline_config_init(&config);
config.mode = PIPELINE_MODE_DUPLEX;
config.sample_rate = 48000;
config.channels = 1;
config.frame_duration_ms = 20;

// Enable processing
config.enable_aec = true;
config.enable_denoise = true;
config.enable_agc = true;
config.denoise_engine = VOICE_DENOISE_RNNOISE;

// Configure codec
config.codec = VOICE_CODEC_OPUS;
config.bitrate = 32000;
config.enable_fec = true;

// Network
config.enable_srtp = true;

voice_pipeline_t *pipeline = voice_pipeline_create(&config);

// Set callbacks
voice_pipeline_set_encoded_callback(pipeline, on_encoded_packet, user_data);
voice_pipeline_set_state_callback(pipeline, on_state_change, user_data);

// Start
voice_pipeline_start(pipeline);

// Feed received network data
voice_pipeline_receive_packet(pipeline, packet_data, packet_size);

// Stop and cleanup
voice_pipeline_stop(pipeline);
voice_pipeline_destroy(pipeline);
```

---

## Platform Adaptation

### iOS

```objc
// Configure audio session
voice_session_config_t config;
voice_session_config_init(&config);
config.category = VOICE_SESSION_CATEGORY_PLAY_AND_RECORD;
config.mode = VOICE_SESSION_MODE_VOICE_CHAT;
config.options = VOICE_SESSION_OPTION_DEFAULT_TO_SPEAKER;

voice_session_configure(&config);
voice_session_activate();

// Handle interruptions
void on_interrupt(voice_session_interruption_t type, void *user_data) {
    if (type == VOICE_SESSION_INTERRUPTION_BEGAN) {
        voice_pipeline_stop(pipeline);
    } else {
        voice_pipeline_start(pipeline);
    }
}
voice_session_set_interrupt_callback(on_interrupt, user_data);
```

### Android

```java
// Java initialization
public class SonicKitLib {
    static {
        System.loadLibrary("sonickit");
    }
    
    public static native void nativeInit(Context context);
    public static native void nativeRelease();
}

// Call during app startup
SonicKitLib.nativeInit(getApplicationContext());
```

### WebAssembly

```javascript
// Load module
const SonicKit = await loadSonicKit();

// Create denoiser
const denoiser = new SonicKit.Denoiser(48000, 480, 0);

// Process audio
const input = new Int16Array(480);
// ... fill input ...
const output = denoiser.process(input);

// Cleanup
denoiser.delete();
```

---

## Performance Optimization

### CPU Optimization

1. **Frame Size**: Larger frames reduce overhead but increase latency
2. **Quality Settings**: Lower resampler quality reduces CPU usage
3. **Engine Selection**: SpeexDSP uses less CPU than RNNoise
4. **SIMD**: Enable with `-DVOICE_ENABLE_SIMD=ON`

### Memory Optimization

1. **Buffer Sizing**: Match buffer sizes to actual needs
2. **Object Pooling**: Reuse encoder/decoder instances
3. **Static Allocation**: Use `_preallocated` variants where available

### Latency Optimization

1. **Frame Duration**: Use 10-20ms frames for low latency
2. **Jitter Buffer**: Configure minimum delay for your network
3. **Processing Order**: AEC -> Denoise -> AGC is optimal

---

## FAQ

### Q: Why is echo cancellation not working?

A: Ensure you're providing the correct playback reference signal to AEC. The playback signal must match exactly what's being output through the speakers.

### Q: How do I reduce CPU usage?

A: Try using SpeexDSP instead of RNNoise, reduce resampler quality, and use larger frame sizes.

### Q: What's the recommended sample rate?

A: 48000 Hz for Opus codec, 16000 Hz for G.711/G.722. Use resampler to convert between rates.

### Q: How do I handle network packet loss?

A: Enable FEC in Opus encoder and use the jitter buffer's PLC feature.

---

## Glossary

| Term | English | Description |
|------|---------|-------------|
| AEC | Acoustic Echo Cancellation | Removes speaker echo from microphone input |
| AGC | Automatic Gain Control | Automatically adjusts audio volume |
| CNG | Comfort Noise Generation | Generates background noise during silence |
| DTMF | Dual-Tone Multi-Frequency | Phone dial tones |
| FEC | Forward Error Correction | Error recovery for lost packets |
| ICE | Interactive Connectivity Establishment | NAT traversal protocol |
| MOS | Mean Opinion Score | Audio quality metric (1-5) |
| PLC | Packet Loss Concealment | Hides effect of lost packets |
| RTP | Real-time Transport Protocol | Media streaming protocol |
| SRTP | Secure RTP | Encrypted RTP |
| STUN | Session Traversal Utilities for NAT | NAT traversal |
| TURN | Traversal Using Relays around NAT | Relay server for NAT traversal |
| VAD | Voice Activity Detection | Detects speech presence |

---

For more information, please refer to:
- [API Reference](API_REFERENCE.md)
- [Examples](../examples/)
- [Source Code](../src/)
