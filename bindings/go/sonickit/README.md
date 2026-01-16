# SonicKit Go Bindings

Go bindings for SonicKit, a high-performance audio processing library.

## Features

- **DSP Processing**: Noise reduction, echo cancellation, AGC, VAD, resampling, DTMF, EQ, compression
- **Audio Utilities**: Buffers, level metering, mixing, jitter buffers, spatial audio, HRTF
- **Codecs**: G.711 (A-law/μ-law) encoding and decoding
- **Effects**: Reverb, delay, pitch shifting, chorus, flanger, time stretching, watermarking

## Requirements

- Go 1.21+
- GCC or compatible C compiler (for cgo)
- Native SonicKit library built and installed

## Installation

```bash
go get github.com/aspect-build/sonickit-go
```

## Building from Source

First, build the native SonicKit library:

```bash
cd /path/to/sonickit
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Then, set the library path:

```bash
# Linux
export LD_LIBRARY_PATH=/path/to/sonickit/build:$LD_LIBRARY_PATH

# macOS
export DYLD_LIBRARY_PATH=/path/to/sonickit/build:$DYLD_LIBRARY_PATH

# Windows (PowerShell)
$env:PATH = "C:\path\to\sonickit\build;$env:PATH"
```

## Quick Start

### Noise Reduction

```go
package main

import (
    "log"
    "github.com/aspect-build/sonickit-go"
)

func main() {
    // Create a denoiser
    denoiser, err := sonickit.NewDenoiser(16000, 160, sonickit.DenoiserSpeexDSP)
    if err != nil {
        log.Fatal(err)
    }
    defer denoiser.Close()

    // Process audio
    input := getAudioFrame() // []int16
    output := denoiser.Process(input)
    // Use output...
}
```

### Echo Cancellation

```go
aec, err := sonickit.NewEchoCanceller(16000, 160, 2000)
if err != nil {
    log.Fatal(err)
}
defer aec.Close()

captured := getMicrophoneAudio() // []int16
playback := getSpeakerAudio()    // []int16
clean := aec.Process(captured, playback)
```

### Voice Activity Detection

```go
vad, err := sonickit.NewVad(16000, sonickit.VadLowBitrate)
if err != nil {
    log.Fatal(err)
}
defer vad.Close()

if vad.IsSpeech(audioFrame) {
    log.Println("Speech detected!")
}
probability := vad.GetProbability()
```

### Sample Rate Conversion

```go
resampler, err := sonickit.NewResampler(1, 16000, 48000, 5)
if err != nil {
    log.Fatal(err)
}
defer resampler.Close()

resampled := resampler.Process(audio16k)
```

### G.711 Codec

```go
// A-law codec
codec, err := sonickit.NewG711Codec(true)
if err != nil {
    log.Fatal(err)
}
defer codec.Close()

// Encode PCM to G.711
encoded := codec.Encode(pcmSamples)

// Decode G.711 to PCM
decoded := codec.Decode(encoded)
```

### Audio Effects

```go
// Reverb
reverb, _ := sonickit.NewReverb(48000, 0.7, 0.3)
defer reverb.Close()
wet := reverb.Process(audio)

// Pitch Shifting
shifter, _ := sonickit.NewPitchShifter(48000, 5.0)
defer shifter.Close()
shifted := shifter.Process(audio)

// Delay/Echo
delay, _ := sonickit.NewDelay(48000, 250, 0.4)
defer delay.Close()
delayed := delay.Process(audio)
```

### Spatial Audio

```go
spatial, err := sonickit.NewSpatialRenderer(48000, 480)
if err != nil {
    log.Fatal(err)
}
defer spatial.Close()

spatial.SetSourcePosition(2.0, 0.0, 1.0)
spatial.SetListenerPosition(0.0, 0.0, 0.0)

stereo := spatial.Process(monoAudio)
```

### DTMF Processing

```go
// Generate DTMF tones
generator, _ := sonickit.NewDtmfGenerator(8000, 100)
defer generator.Close()
audio := generator.GenerateSequence("123456")

// Detect DTMF tones
detector, _ := sonickit.NewDtmfDetector(8000, 160)
defer detector.Close()

for _, frame := range audioFrames {
    if digit := detector.Process(frame); digit != 0 {
        log.Printf("Detected: %c", digit)
    }
}
```

## API Reference

### DSP Types

| Type | Description |
|------|-------------|
| `Denoiser` | Noise reduction (SpeexDSP/RNNoise) |
| `EchoCanceller` | Acoustic echo cancellation |
| `Agc` | Automatic gain control |
| `Vad` | Voice activity detection |
| `Resampler` | Sample rate conversion |
| `DtmfDetector` | DTMF tone detection |
| `DtmfGenerator` | DTMF tone generation |
| `Equalizer` | Parametric equalizer |
| `Compressor` | Dynamic range compression |
| `ComfortNoiseGenerator` | Comfort noise generation |

### Audio Types

| Type | Description |
|------|-------------|
| `AudioBuffer` | Ring buffer for audio samples |
| `AudioLevel` | Level metering |
| `AudioMixer` | Multi-channel mixer |
| `JitterBuffer` | Network jitter compensation |
| `SpatialRenderer` | 3D spatial audio |
| `Hrtf` | Head-related transfer function |

### Codec Types

| Type | Description |
|------|-------------|
| `G711Codec` | G.711 A-law/μ-law codec |

### Effect Types

| Type | Description |
|------|-------------|
| `Reverb` | Room reverb |
| `Delay` | Echo/delay effect |
| `PitchShifter` | Pitch shifting |
| `Chorus` | Chorus effect |
| `Flanger` | Flanger effect |
| `TimeStretcher` | Time stretching |
| `WatermarkEmbedder` | Audio watermark embedding |
| `WatermarkDetector` | Audio watermark detection |

## Resource Management

All types implement the `Close()` method. Use `defer` to ensure cleanup:

```go
denoiser, err := sonickit.NewDenoiser(16000, 160, sonickit.DenoiserSpeexDSP)
if err != nil {
    log.Fatal(err)
}
defer denoiser.Close() // Ensure cleanup

// Use denoiser...
```

Types also have finalizers that will clean up if `Close()` is not called, but explicit cleanup is recommended.

## Thread Safety

Individual processor instances are NOT thread-safe. Use separate instances for different goroutines, or implement your own synchronization:

```go
var mu sync.Mutex
var denoiser *sonickit.Denoiser

func ProcessFrame(frame []int16) []int16 {
    mu.Lock()
    defer mu.Unlock()
    return denoiser.Process(frame)
}
```

## Running Tests

```bash
cd bindings/go/sonickit
go test -v
```

## License

MIT License - see the main SonicKit repository for details.
