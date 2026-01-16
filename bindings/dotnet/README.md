# SonicKit .NET Bindings

.NET bindings for SonicKit, a high-performance audio processing library.

## Features

- **DSP Processing**: Noise reduction, echo cancellation, AGC, VAD, resampling, DTMF, EQ, compression
- **Audio Utilities**: Buffers, level metering, mixing, jitter buffers, spatial audio, HRTF
- **Codecs**: G.711 (A-law/μ-law) encoding and decoding
- **Effects**: Reverb, delay, pitch shifting, chorus, flanger, time stretching, watermarking

## Requirements

- .NET 6.0, 7.0, or 8.0
- Native SonicKit library for your platform

## Installation

### From NuGet (when available)

```bash
dotnet add package SonicKit
```

### From Source

```bash
cd bindings/dotnet
dotnet build
```

## Quick Start

### Noise Reduction

```csharp
using SonicKit.Dsp;

using var denoiser = new Denoiser(sampleRate: 16000, frameSize: 160);

short[] input = GetAudioFrame();
short[] output = denoiser.Process(input);
```

### Echo Cancellation

```csharp
using SonicKit.Dsp;

using var aec = new EchoCanceller(
    sampleRate: 16000,
    frameSize: 160,
    filterLength: 2000);

short[] captured = GetMicrophoneAudio();
short[] playback = GetSpeakerAudio();
short[] clean = aec.Process(captured, playback);
```

### Voice Activity Detection

```csharp
using SonicKit.Dsp;

using var vad = new Vad(sampleRate: 16000, mode: Vad.Mode.LowBitrate);

bool isSpeech = vad.IsSpeech(audioFrame);
float probability = vad.GetProbability();
```

### Sample Rate Conversion

```csharp
using SonicKit.Dsp;

using var resampler = new Resampler(
    channels: 1,
    inRate: 16000,
    outRate: 48000,
    quality: 5);

short[] resampled = resampler.Process(inputAudio);
```

### G.711 Codec

```csharp
using SonicKit.Codecs;

var codec = new G711Codec(useAlaw: true);

// Encode PCM to G.711
byte[] encoded = codec.Encode(pcmSamples);

// Decode G.711 to PCM
short[] decoded = codec.Decode(encoded);
```

### Audio Effects

```csharp
using SonicKit.Effects;

// Reverb
using var reverb = new Reverb(48000, roomSize: 0.7f, wetLevel: 0.3f);
short[] wet = reverb.Process(audio);

// Pitch Shifting
using var shifter = new PitchShifter(48000, semitones: 5f);
short[] shifted = shifter.Process(audio);

// Delay/Echo
using var delay = new Delay(48000, delayMs: 250f, feedback: 0.4f);
short[] delayed = delay.Process(audio);
```

### Spatial Audio

```csharp
using SonicKit.Audio;

using var spatial = new SpatialRenderer(48000, 480);
spatial.SetSourcePosition(x: 2f, y: 0f, z: 1f);
spatial.SetListenerPosition(x: 0f, y: 0f, z: 0f);

short[] stereo = spatial.Process(monoAudio);
```

### Jitter Buffer

```csharp
using SonicKit.Audio;

using var jitter = new JitterBuffer(
    sampleRate: 16000,
    frameSizeMs: 20,
    minDelayMs: 40,
    maxDelayMs: 200);

// Receive packets
jitter.Put(audioData, timestamp: rtpTimestamp, sequence: seqNum);

// Get playback audio
short[] playback = jitter.Get(numSamples: 320);
```

### DTMF Processing

```csharp
using SonicKit.Dsp;

// Generate DTMF tones
using var generator = new DtmfGenerator(8000, toneDurationMs: 100);
short[] audio = generator.GenerateSequence("123456");

// Detect DTMF tones
using var detector = new DtmfDetector(8000, 160);
foreach (var frame in audioFrames)
{
    char digit = detector.Process(frame);
    if (digit != '\0')
        Console.WriteLine($"Detected: {digit}");
}
```

## API Reference

### DSP Namespace (SonicKit.Dsp)

| Class | Description |
|-------|-------------|
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

### Audio Namespace (SonicKit.Audio)

| Class | Description |
|-------|-------------|
| `AudioBuffer` | Ring buffer for audio samples |
| `AudioLevel` | Level metering |
| `AudioMixer` | Multi-channel mixer |
| `JitterBuffer` | Network jitter compensation |
| `SpatialRenderer` | 3D spatial audio |
| `Hrtf` | Head-related transfer function |

### Codecs Namespace (SonicKit.Codecs)

| Class | Description |
|-------|-------------|
| `G711Codec` | G.711 A-law/μ-law codec |

### Effects Namespace (SonicKit.Effects)

| Class | Description |
|-------|-------------|
| `Reverb` | Room reverb |
| `Delay` | Echo/delay effect |
| `PitchShifter` | Pitch shifting |
| `Chorus` | Chorus effect |
| `Flanger` | Flanger effect |
| `TimeStretcher` | Time stretching |
| `WatermarkEmbedder` | Audio watermark embedding |
| `WatermarkDetector` | Audio watermark detection |

## Memory Management

All classes that wrap native handles implement `IDisposable`. Use the `using` statement or call `Dispose()` when done:

```csharp
// Recommended: using statement
using var denoiser = new Denoiser(16000, 160);

// Alternative: manual disposal
var denoiser = new Denoiser(16000, 160);
try
{
    // Use denoiser
}
finally
{
    denoiser.Dispose();
}
```

## Thread Safety

Individual processor instances are NOT thread-safe. Use separate instances for different threads, or implement your own synchronization.

## Native Library Loading

The library uses `LibraryImport` (source-generated P/Invoke) for native interop. The native library should be:

- On Windows: `sonickit.dll` in the application directory or PATH
- On Linux: `libsonickit.so` in the library search path
- On macOS: `libsonickit.dylib` in the library search path

For NuGet packages, native libraries are included in the `runtimes` folder.

## Building from Source

```bash
# Build the library
cd bindings/dotnet
dotnet build

# Run tests
dotnet test

# Create NuGet package
dotnet pack -c Release
```

## License

MIT License - see the main SonicKit repository for details.
