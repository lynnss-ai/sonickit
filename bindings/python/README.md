# SonicKit Python Bindings

Python bindings for SonicKit, a high-performance audio processing library.

## Features

- **DSP Processing**: Noise reduction, echo cancellation, AGC, VAD, resampling
- **Audio Effects**: Reverb, delay, pitch shifting, chorus, flanger, time stretching
- **Codecs**: G.711 (A-law/μ-law) encoding and decoding
- **Audio Utilities**: Buffers, level metering, mixing, jitter buffers
- **Spatial Audio**: 3D positioning, HRTF processing
- **Watermarking**: Embed and detect audio watermarks

## Requirements

- Python 3.8 or later
- NumPy
- CMake 3.16 or later (for building from source)
- C/C++ compiler (GCC, Clang, or MSVC)

## Installation

### From PyPI (when available)

```bash
pip install sonickit
```

### From Source

```bash
# Clone the repository
git clone https://github.com/example/sonickit.git
cd sonickit/bindings/python

# Install build dependencies
pip install build numpy pybind11

# Build and install
pip install .
```

### Development Installation

```bash
# Install in editable mode with development dependencies
pip install -e ".[dev]"

# Run tests
pytest
```

## Quick Start

### Basic Audio Processing

```python
import numpy as np
import sonickit

# Create a denoiser
denoiser = sonickit.Denoiser(sample_rate=16000, frame_size=160)

# Process audio frames
audio = np.random.randint(-32768, 32767, 160, dtype=np.int16)
cleaned = denoiser.process(audio)
```

### Voice Activity Detection

```python
import sonickit

# Create VAD
vad = sonickit.VAD(sample_rate=16000, mode=1)

# Check for speech
is_speech = vad.is_speech(audio)
probability = vad.get_probability()
```

### Echo Cancellation

```python
import sonickit

# Create AEC
aec = sonickit.EchoCanceller(
    sample_rate=16000,
    frame_size=160,
    filter_length=2000  # Echo tail length
)

# Process with reference
captured = get_microphone_audio()  # Your capture function
playback = get_speaker_audio()     # What's being played
cleaned = aec.process(captured, playback)
```

### Audio Effects

```python
import sonickit

# Create effects
reverb = sonickit.Reverb(48000, room_size=0.7, wet_level=0.3)
delay = sonickit.Delay(48000, delay_ms=250, feedback=0.4)
pitch = sonickit.PitchShifter(48000, shift_semitones=5)

# Apply effects
audio = reverb.process(audio)
audio = delay.process(audio)
audio = pitch.process(audio)
```

### Sample Rate Conversion

```python
import sonickit

# Upsample from 16 kHz to 48 kHz
resampler = sonickit.Resampler(
    channels=1,
    in_rate=16000,
    out_rate=48000,
    quality=5  # 0-10, higher is better
)

output = resampler.process(audio)
```

### G.711 Codec

```python
import sonickit

# Create codec (A-law or μ-law)
codec = sonickit.G711Codec(use_alaw=True)

# Encode PCM to G.711
encoded = codec.encode(pcm_audio)  # Returns uint8 array

# Decode G.711 to PCM
decoded = codec.decode(encoded)
```

### Jitter Buffer

```python
import sonickit

# Create jitter buffer
jitter = sonickit.JitterBuffer(
    sample_rate=16000,
    frame_size_ms=20,
    min_delay_ms=40,
    max_delay_ms=200
)

# Put packets (from network)
jitter.put(audio, timestamp=0, sequence=0)

# Get audio for playback
playback = jitter.get(320)  # 20ms at 16kHz
```

### Spatial Audio

```python
import sonickit

# Create spatial renderer
spatial = sonickit.SpatialRenderer(48000, 480)

# Position the sound source
spatial.set_source_position(2.0, 0.0, 1.0)  # x, y, z
spatial.set_listener_position(0.0, 0.0, 0.0)

# Process mono to stereo with spatial positioning
stereo = spatial.process(mono_audio)
```

### DTMF Processing

```python
import sonickit

# Generate DTMF tones
generator = sonickit.DTMFGenerator(8000, tone_duration_ms=100)
audio = generator.generate_sequence("123456")

# Detect DTMF tones
detector = sonickit.DTMFDetector(8000, 160)
for frame in audio_frames:
    digit = detector.process(frame)
    if digit:
        print(f"Detected: {digit}")
```

### Audio Watermarking

```python
import sonickit

# Embed watermark
embedder = sonickit.WatermarkEmbedder(48000, strength=0.15)
marked = embedder.embed_string(audio, "Copyright 2024")

# Detect watermark
detector = sonickit.WatermarkDetector(48000)
result = detector.detect_string(marked)
if result['found']:
    print(f"Watermark: {result['message']}")
```

## API Reference

### DSP Classes

| Class | Description |
|-------|-------------|
| `Denoiser` | Noise reduction (SpeexDSP/RNNoise) |
| `EchoCanceller` | Acoustic echo cancellation |
| `AGC` | Automatic gain control |
| `VAD` | Voice activity detection |
| `Resampler` | Sample rate conversion |
| `DTMFDetector` | DTMF tone detection |
| `DTMFGenerator` | DTMF tone generation |
| `Equalizer` | Parametric equalizer |
| `Compressor` | Dynamic range compression |
| `ComfortNoise` | Comfort noise generation |

### Audio Classes

| Class | Description |
|-------|-------------|
| `AudioBuffer` | Ring buffer for audio samples |
| `AudioLevel` | Level metering |
| `AudioMixer` | Multi-channel mixer |
| `JitterBuffer` | Network jitter compensation |
| `SpatialRenderer` | 3D spatial audio |
| `HRTF` | Head-related transfer function |

### Codec Classes

| Class | Description |
|-------|-------------|
| `G711Codec` | G.711 A-law/μ-law codec |

### Effects Classes

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

## Audio Format

All audio processing functions expect and return 16-bit signed integer arrays (`numpy.int16`).

- **Sample Rate**: Specified per processor (typically 8000, 16000, 44100, or 48000 Hz)
- **Channels**: Most processors work with mono audio; stereo output for spatial processing
- **Frame Size**: Depends on the processor (typically 10-20ms worth of samples)

## Thread Safety

Individual processor instances are NOT thread-safe. Use separate instances for different threads, or implement your own synchronization.

## Examples

See the `examples/` directory for complete examples:

- `basic_processing.py` - Basic DSP operations
- `effects_processing.py` - Audio effects
- `voip_pipeline.py` - Complete VoIP processing pipeline

## Building the Documentation

```bash
pip install -e ".[docs]"
cd docs
sphinx-build -b html . _build/html
```

## Running Tests

```bash
pip install -e ".[dev]"
pytest -v
```

## License

MIT License - see the main SonicKit repository for details.

## Contributing

Contributions are welcome! Please read the contributing guidelines in the main repository.
