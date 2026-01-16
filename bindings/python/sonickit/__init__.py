"""
SonicKit - High-Performance Audio Processing Library
=====================================================

SonicKit provides professional-grade audio processing capabilities
for Python applications.

Features:
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
    >>>
    >>> # Create a denoiser
    >>> denoiser = sonickit.Denoiser(sample_rate=16000, frame_size=160)
    >>>
    >>> # Process audio
    >>> clean_audio = denoiser.process(noisy_audio)
    >>>
    >>> # Create echo canceller
    >>> aec = sonickit.EchoCanceller(16000, 160, filter_length=1600)
    >>> cleaned = aec.process(mic_audio, speaker_audio)

Copyright (c) 2024 SonicKit Team
License: MIT
"""

from sonickit._sonickit import (
    # Version
    __version__,

    # DSP Classes
    Denoiser,
    EchoCanceller,
    AGC,
    VAD,
    Resampler,
    DTMFDetector,
    DTMFGenerator,
    Equalizer,
    Compressor,
    ComfortNoise,

    # Audio Classes
    AudioBuffer,
    AudioLevel,
    AudioMixer,
    JitterBuffer,
    SpatialRenderer,
    HRTF,

    # Codec Classes
    G711Codec,

    # Effects Classes
    Reverb,
    Delay,
    PitchShifter,
    Chorus,
    Flanger,
    TimeStretcher,
    WatermarkEmbedder,
    WatermarkDetector,

    # Enums
    EQPreset,
    ReverbPreset,
    CodecID,
)

__all__ = [
    # Version
    "__version__",

    # DSP
    "Denoiser",
    "EchoCanceller",
    "AGC",
    "VAD",
    "Resampler",
    "DTMFDetector",
    "DTMFGenerator",
    "Equalizer",
    "Compressor",
    "ComfortNoise",

    # Audio
    "AudioBuffer",
    "AudioLevel",
    "AudioMixer",
    "JitterBuffer",
    "SpatialRenderer",
    "HRTF",

    # Codec
    "G711Codec",

    # Effects
    "Reverb",
    "Delay",
    "PitchShifter",
    "Chorus",
    "Flanger",
    "TimeStretcher",
    "WatermarkEmbedder",
    "WatermarkDetector",

    # Enums
    "EQPreset",
    "ReverbPreset",
    "CodecID",
]


def get_version() -> str:
    """Get the SonicKit version string."""
    return __version__


def get_info() -> dict:
    """Get information about the SonicKit library.

    Returns:
        Dictionary with version and feature information.
    """
    return {
        "version": __version__,
        "features": {
            "dsp": [
                "Denoiser", "EchoCanceller", "AGC", "VAD",
                "Resampler", "DTMF", "Equalizer", "Compressor"
            ],
            "audio": [
                "AudioBuffer", "AudioLevel", "AudioMixer",
                "JitterBuffer", "SpatialRenderer", "HRTF"
            ],
            "codec": ["G711"],
            "effects": [
                "Reverb", "Delay", "PitchShifter",
                "Chorus", "Flanger", "TimeStretcher"
            ],
            "watermark": ["WatermarkEmbedder", "WatermarkDetector"],
        }
    }
