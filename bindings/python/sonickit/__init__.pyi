"""Type stubs for SonicKit Python bindings."""

from typing import Dict, List, Optional, Tuple, Union
import numpy as np
from numpy.typing import NDArray

__version__: str

# Type aliases
AudioArray = NDArray[np.int16]
ByteArray = NDArray[np.uint8]

# ============================================
# DSP Classes
# ============================================

class Denoiser:
    """Noise reduction using spectral subtraction or RNNoise."""

    def __init__(
        self,
        sample_rate: int,
        frame_size: int,
        engine_type: int = 0
    ) -> None:
        """
        Create a denoiser.

        Args:
            sample_rate: Audio sample rate in Hz (8000, 16000, 48000)
            frame_size: Number of samples per frame
            engine_type: 0=auto, 1=SpeexDSP, 2=RNNoise
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio frame and return denoised output."""
        ...

    def reset(self) -> None:
        """Reset denoiser state."""
        ...

    @property
    def frame_size(self) -> int:
        """Get frame size."""
        ...


class EchoCanceller:
    """Acoustic Echo Canceller for full-duplex communication."""

    def __init__(
        self,
        sample_rate: int,
        frame_size: int,
        filter_length: int = 2000
    ) -> None:
        """
        Create an echo canceller.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
            filter_length: Echo tail length in samples
        """
        ...

    def process(self, captured: AudioArray, playback: AudioArray) -> AudioArray:
        """Process captured audio with playback reference."""
        ...

    def reset(self) -> None:
        """Reset AEC state."""
        ...

    @property
    def frame_size(self) -> int:
        """Get frame size."""
        ...


class AGC:
    """Automatic Gain Control for level normalization."""

    def __init__(
        self,
        sample_rate: int,
        frame_size: int,
        mode: int = 1,
        target_level: float = -3.0
    ) -> None:
        """
        Create an AGC.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
            mode: AGC mode (0=fixed, 1=adaptive, 2=digital)
            target_level: Target level in dBFS
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio frame with AGC."""
        ...

    def get_gain(self) -> float:
        """Get current gain in dB."""
        ...

    def reset(self) -> None:
        """Reset AGC state."""
        ...


class VAD:
    """Voice Activity Detection."""

    def __init__(self, sample_rate: int, mode: int = 1) -> None:
        """
        Create a VAD.

        Args:
            sample_rate: Audio sample rate in Hz
            mode: VAD mode (0=quality, 1=low bitrate, 2=aggressive, 3=very aggressive)
        """
        ...

    def is_speech(self, input: AudioArray) -> bool:
        """Check if audio frame contains speech."""
        ...

    def get_probability(self) -> float:
        """Get speech probability (0.0 - 1.0)."""
        ...

    def reset(self) -> None:
        """Reset VAD state."""
        ...


class Resampler:
    """High-quality sample rate converter."""

    def __init__(
        self,
        channels: int,
        in_rate: int,
        out_rate: int,
        quality: int = 5
    ) -> None:
        """
        Create a resampler.

        Args:
            channels: Number of audio channels
            in_rate: Input sample rate in Hz
            out_rate: Output sample rate in Hz
            quality: Quality level (0-10, higher is better)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Resample audio data."""
        ...

    def reset(self) -> None:
        """Reset resampler state."""
        ...


class DTMFDetector:
    """DTMF tone detector."""

    def __init__(self, sample_rate: int, frame_size: int) -> None:
        """
        Create a DTMF detector.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
        """
        ...

    def process(self, input: AudioArray) -> str:
        """Process audio and return detected digit (empty if none)."""
        ...

    def get_digits(self) -> str:
        """Get all accumulated digits."""
        ...

    def clear_digits(self) -> None:
        """Clear accumulated digits."""
        ...

    def reset(self) -> None:
        """Reset detector state."""
        ...


class DTMFGenerator:
    """DTMF tone generator."""

    def __init__(
        self,
        sample_rate: int,
        tone_duration_ms: int = 100,
        pause_duration_ms: int = 50
    ) -> None:
        """
        Create a DTMF generator.

        Args:
            sample_rate: Audio sample rate in Hz
            tone_duration_ms: Duration of each tone in milliseconds
            pause_duration_ms: Duration of pause between tones
        """
        ...

    def generate_digit(self, digit: str) -> AudioArray:
        """Generate audio for a single DTMF digit."""
        ...

    def generate_sequence(self, digits: str) -> AudioArray:
        """Generate audio for a sequence of digits."""
        ...

    def reset(self) -> None:
        """Reset generator state."""
        ...


class Equalizer:
    """Multi-band parametric equalizer."""

    def __init__(self, sample_rate: int, num_bands: int = 5) -> None:
        """
        Create an equalizer.

        Args:
            sample_rate: Audio sample rate in Hz
            num_bands: Number of EQ bands (default 5)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through equalizer."""
        ...

    def set_band(
        self,
        band_index: int,
        frequency: float,
        gain_db: float,
        q: float
    ) -> None:
        """Set EQ band parameters (index, freq Hz, gain dB, Q)."""
        ...

    def set_master_gain(self, gain_db: float) -> None:
        """Set master output gain in dB."""
        ...

    def apply_preset(self, preset: int) -> None:
        """Apply EQ preset."""
        ...

    def reset(self) -> None:
        """Reset EQ state."""
        ...


class Compressor:
    """Dynamic range compressor."""

    def __init__(
        self,
        sample_rate: int,
        threshold_db: float = -20.0,
        ratio: float = 4.0,
        attack_ms: float = 10.0,
        release_ms: float = 100.0
    ) -> None:
        """
        Create a compressor.

        Args:
            sample_rate: Audio sample rate in Hz
            threshold_db: Compression threshold in dB
            ratio: Compression ratio (e.g., 4.0 for 4:1)
            attack_ms: Attack time in milliseconds
            release_ms: Release time in milliseconds
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through compressor."""
        ...

    def set_threshold(self, threshold_db: float) -> None:
        """Set compression threshold."""
        ...

    def set_ratio(self, ratio: float) -> None:
        """Set compression ratio."""
        ...

    def set_times(self, attack_ms: float, release_ms: float) -> None:
        """Set attack and release times."""
        ...


class ComfortNoise:
    """Comfort Noise Generator for silence substitution."""

    def __init__(
        self,
        sample_rate: int,
        frame_size: int,
        noise_level_db: float = -50.0
    ) -> None:
        """
        Create a CNG.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
            noise_level_db: Initial noise level in dB
        """
        ...

    def analyze(self, input: AudioArray) -> None:
        """Analyze background noise characteristics."""
        ...

    def generate(self, num_samples: int) -> AudioArray:
        """Generate comfort noise samples."""
        ...

    def set_level(self, level_db: float) -> None:
        """Set noise level in dB."""
        ...

    def get_level(self) -> float:
        """Get current noise level in dB."""
        ...

    def reset(self) -> None:
        """Reset CNG state."""
        ...


# ============================================
# Audio Classes
# ============================================

class AudioBuffer:
    """Audio sample buffer for storage and management."""

    def __init__(self, capacity_samples: int, channels: int = 1) -> None:
        """
        Create an audio buffer.

        Args:
            capacity_samples: Maximum number of samples to store
            channels: Number of audio channels
        """
        ...

    def write(self, input: AudioArray) -> None:
        """Write samples to buffer."""
        ...

    def read(self, num_samples: int) -> AudioArray:
        """Read and remove samples from buffer."""
        ...

    def peek(self, num_samples: int) -> AudioArray:
        """Read samples without removing them."""
        ...

    @property
    def available(self) -> int:
        """Number of samples available for reading."""
        ...

    @property
    def capacity(self) -> int:
        """Total buffer capacity."""
        ...

    @property
    def free_space(self) -> int:
        """Free space remaining."""
        ...

    def clear(self) -> None:
        """Clear all samples."""
        ...


class AudioLevel:
    """Audio level meter for signal monitoring."""

    def __init__(self, sample_rate: int, frame_size: int) -> None:
        """
        Create an audio level meter.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per measurement frame
        """
        ...

    def process(self, input: AudioArray) -> None:
        """Process audio samples."""
        ...

    def get_level_db(self) -> float:
        """Get current level in dB."""
        ...

    def get_peak_db(self) -> float:
        """Get peak level in dB."""
        ...

    def get_rms_db(self) -> float:
        """Get RMS level in dB."""
        ...

    def is_silence(self, threshold_db: float = -50.0) -> bool:
        """Check if audio is silence."""
        ...

    def is_clipping(self) -> bool:
        """Check if audio is clipping."""
        ...

    def reset(self) -> None:
        """Reset level meter."""
        ...


class AudioMixer:
    """Multi-channel audio mixer."""

    def __init__(self, sample_rate: int, num_inputs: int, frame_size: int) -> None:
        """
        Create an audio mixer.

        Args:
            sample_rate: Audio sample rate in Hz
            num_inputs: Number of input channels to mix
            frame_size: Samples per frame
        """
        ...

    def set_input(self, index: int, input: AudioArray) -> None:
        """Set input audio for a channel."""
        ...

    def set_input_gain(self, index: int, gain: float) -> None:
        """Set gain for an input channel."""
        ...

    def set_master_gain(self, gain: float) -> None:
        """Set master output gain."""
        ...

    def mix(self, output_samples: int) -> AudioArray:
        """Mix all inputs and return output."""
        ...

    def reset(self) -> None:
        """Reset mixer state."""
        ...

    @property
    def num_inputs(self) -> int:
        """Number of input channels."""
        ...


class JitterBuffer:
    """Jitter buffer for network audio."""

    def __init__(
        self,
        sample_rate: int,
        frame_size_ms: int = 20,
        min_delay_ms: int = 20,
        max_delay_ms: int = 200
    ) -> None:
        """
        Create a jitter buffer.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size_ms: Frame size in milliseconds
            min_delay_ms: Minimum buffer delay
            max_delay_ms: Maximum buffer delay
        """
        ...

    def put(self, input: AudioArray, timestamp: int, sequence: int) -> None:
        """Put a packet into the jitter buffer."""
        ...

    def get(self, num_samples: int) -> AudioArray:
        """Get audio from the jitter buffer."""
        ...

    def get_stats(self) -> Dict[str, Union[int, float]]:
        """Get jitter buffer statistics."""
        ...

    def get_delay_ms(self) -> float:
        """Get current buffer delay in ms."""
        ...

    def reset(self) -> None:
        """Reset jitter buffer."""
        ...


class SpatialRenderer:
    """3D spatial audio renderer."""

    def __init__(self, sample_rate: int, frame_size: int) -> None:
        """
        Create a spatial renderer.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_size: Number of samples per frame
        """
        ...

    def set_source_position(self, x: float, y: float, z: float) -> None:
        """Set sound source position."""
        ...

    def set_listener_position(self, x: float, y: float, z: float) -> None:
        """Set listener position."""
        ...

    def set_listener_orientation(
        self, yaw: float, pitch: float, roll: float
    ) -> None:
        """Set listener orientation."""
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process mono audio to stereo with spatial positioning."""
        ...

    def set_attenuation(
        self, min_distance: float, max_distance: float, rolloff: float
    ) -> None:
        """Set distance attenuation parameters."""
        ...

    def reset(self) -> None:
        """Reset renderer state."""
        ...


class HRTF:
    """Head-Related Transfer Function processor."""

    def __init__(self, sample_rate: int) -> None:
        """
        Create an HRTF processor.

        Args:
            sample_rate: Audio sample rate in Hz
        """
        ...

    def set_azimuth(self, azimuth_deg: float) -> None:
        """Set azimuth angle in degrees (-180 to 180)."""
        ...

    def set_elevation(self, elevation_deg: float) -> None:
        """Set elevation angle in degrees (-90 to 90)."""
        ...

    def set_distance(self, distance: float) -> None:
        """Set distance from listener."""
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process mono audio to stereo with HRTF."""
        ...

    def reset(self) -> None:
        """Reset HRTF state."""
        ...


# ============================================
# Codec Classes
# ============================================

class G711Codec:
    """G.711 audio codec (A-law and μ-law)."""

    def __init__(self, use_alaw: bool = True) -> None:
        """
        Create a G.711 codec.

        Args:
            use_alaw: True for A-law, False for μ-law
        """
        ...

    def encode(self, input: AudioArray) -> ByteArray:
        """Encode PCM audio to G.711."""
        ...

    def decode(self, input: ByteArray) -> AudioArray:
        """Decode G.711 to PCM audio."""
        ...

    @property
    def is_alaw(self) -> bool:
        """True if using A-law, False for μ-law."""
        ...


# ============================================
# Effects Classes
# ============================================

class Reverb:
    """Room reverb effect."""

    def __init__(
        self,
        sample_rate: int,
        room_size: float = 0.5,
        damping: float = 0.5,
        wet_level: float = 0.3,
        dry_level: float = 0.7
    ) -> None:
        """
        Create a reverb effect.

        Args:
            sample_rate: Audio sample rate in Hz
            room_size: Room size (0.0 = small, 1.0 = large)
            damping: High frequency damping (0.0 = bright, 1.0 = dark)
            wet_level: Wet signal level (0.0 - 1.0)
            dry_level: Dry signal level (0.0 - 1.0)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through reverb."""
        ...

    def set_room_size(self, room_size: float) -> None: ...
    def set_damping(self, damping: float) -> None: ...
    def set_wet_level(self, level: float) -> None: ...
    def set_dry_level(self, level: float) -> None: ...
    def set_preset(self, preset: int) -> None: ...
    def reset(self) -> None: ...


class Delay:
    """Echo/delay effect."""

    def __init__(
        self,
        sample_rate: int,
        delay_ms: float = 250.0,
        feedback: float = 0.4,
        mix: float = 0.5
    ) -> None:
        """
        Create a delay effect.

        Args:
            sample_rate: Audio sample rate in Hz
            delay_ms: Delay time in milliseconds
            feedback: Feedback amount (0.0 - 1.0)
            mix: Wet/dry mix (0.0 = dry only, 1.0 = wet only)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through delay."""
        ...

    def set_delay_time(self, delay_ms: float) -> None: ...
    def set_feedback(self, feedback: float) -> None: ...
    def set_mix(self, mix: float) -> None: ...
    def reset(self) -> None: ...


class PitchShifter:
    """Pitch shifter effect."""

    def __init__(self, sample_rate: int, shift_semitones: float = 0.0) -> None:
        """
        Create a pitch shifter.

        Args:
            sample_rate: Audio sample rate in Hz
            shift_semitones: Pitch shift in semitones (-12 to +12)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through pitch shifter."""
        ...

    def set_shift(self, semitones: float) -> None:
        """Set pitch shift in semitones."""
        ...

    def get_shift(self) -> float:
        """Get current pitch shift."""
        ...

    def reset(self) -> None: ...


class Chorus:
    """Chorus effect for thickening sound."""

    def __init__(
        self,
        sample_rate: int,
        rate: float = 1.5,
        depth: float = 0.5,
        mix: float = 0.5
    ) -> None:
        """
        Create a chorus effect.

        Args:
            sample_rate: Audio sample rate in Hz
            rate: Modulation rate in Hz
            depth: Modulation depth (0.0 - 1.0)
            mix: Wet/dry mix (0.0 - 1.0)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through chorus."""
        ...

    def set_rate(self, rate: float) -> None: ...
    def set_depth(self, depth: float) -> None: ...
    def set_mix(self, mix: float) -> None: ...
    def reset(self) -> None: ...


class Flanger:
    """Flanger effect for modulation."""

    def __init__(
        self,
        sample_rate: int,
        rate: float = 0.5,
        depth: float = 0.5,
        feedback: float = 0.5,
        mix: float = 0.5
    ) -> None:
        """
        Create a flanger effect.

        Args:
            sample_rate: Audio sample rate in Hz
            rate: Modulation rate in Hz
            depth: Modulation depth (0.0 - 1.0)
            feedback: Feedback amount (-1.0 to 1.0)
            mix: Wet/dry mix (0.0 - 1.0)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through flanger."""
        ...

    def set_rate(self, rate: float) -> None: ...
    def set_depth(self, depth: float) -> None: ...
    def set_feedback(self, feedback: float) -> None: ...
    def set_mix(self, mix: float) -> None: ...
    def reset(self) -> None: ...


class TimeStretcher:
    """Time stretcher for changing tempo without pitch change."""

    def __init__(
        self,
        sample_rate: int,
        channels: int = 1,
        initial_rate: float = 1.0
    ) -> None:
        """
        Create a time stretcher.

        Args:
            sample_rate: Audio sample rate in Hz
            channels: Number of audio channels
            initial_rate: Initial time stretch rate (1.0 = normal)
        """
        ...

    def process(self, input: AudioArray) -> AudioArray:
        """Process audio through time stretcher."""
        ...

    def set_rate(self, rate: float) -> None:
        """Set time stretch rate (0.5 - 2.0)."""
        ...

    def get_rate(self) -> float:
        """Get current time stretch rate."""
        ...

    def reset(self) -> None: ...


class WatermarkEmbedder:
    """Audio watermark embedder for hiding data in audio."""

    def __init__(self, sample_rate: int, strength: float = 0.1) -> None:
        """
        Create a watermark embedder.

        Args:
            sample_rate: Audio sample rate in Hz
            strength: Watermark strength (0.0 = inaudible, 1.0 = strong)
        """
        ...

    def embed_string(self, audio: AudioArray, message: str) -> AudioArray:
        """Embed a string message as watermark."""
        ...

    def embed_bytes(self, audio: AudioArray, data: ByteArray) -> AudioArray:
        """Embed binary data as watermark."""
        ...

    def set_strength(self, strength: float) -> None:
        """Set watermark strength."""
        ...

    def reset(self) -> None: ...


class WatermarkDetector:
    """Audio watermark detector for extracting hidden data."""

    def __init__(self, sample_rate: int) -> None:
        """
        Create a watermark detector.

        Args:
            sample_rate: Audio sample rate in Hz
        """
        ...

    def detect_string(self, audio: AudioArray) -> Dict[str, Union[bool, float, str]]:
        """Detect and extract string watermark."""
        ...

    def has_watermark(self, audio: AudioArray) -> bool:
        """Check if audio contains a watermark."""
        ...

    def reset(self) -> None: ...


# ============================================
# Enums
# ============================================

class EQPreset:
    FLAT: int
    VOICE_ENHANCE: int
    TELEPHONE: int
    BASS_BOOST: int
    TREBLE_BOOST: int
    REDUCE_NOISE: int
    CLARITY: int


class ReverbPreset:
    SMALL_ROOM: int
    MEDIUM_ROOM: int
    LARGE_ROOM: int
    HALL: int
    CATHEDRAL: int
    PLATE: int


class CodecID:
    G711_ALAW: int
    G711_ULAW: int
    OPUS: int
    G722: int


# ============================================
# Utility Functions
# ============================================

def get_version() -> str:
    """Get the SonicKit version string."""
    ...


def get_info() -> Dict[str, Union[str, Dict[str, List[str]]]]:
    """Get information about the SonicKit library."""
    ...
