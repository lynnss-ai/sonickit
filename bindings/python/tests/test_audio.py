"""Tests for SonicKit audio utility bindings."""

import pytest
import numpy as np

try:
    import sonickit
    HAS_SONICKIT = True
except ImportError:
    HAS_SONICKIT = False

pytestmark = pytest.mark.skipif(not HAS_SONICKIT, reason="sonickit not installed")


class TestAudioBuffer:
    """Tests for AudioBuffer class."""

    def test_create(self):
        """Test creating audio buffer."""
        buffer = sonickit.AudioBuffer(4800)
        assert buffer is not None
        assert buffer.capacity == 4800
        assert buffer.available == 0

    def test_create_multichannel(self):
        """Test creating multi-channel buffer."""
        buffer = sonickit.AudioBuffer(4800, channels=2)
        assert buffer.capacity == 4800

    def test_write_read(self):
        """Test writing and reading data."""
        buffer = sonickit.AudioBuffer(1000)
        data = np.array([1, 2, 3, 4, 5], dtype=np.int16)
        buffer.write(data)
        assert buffer.available == 5

        output = buffer.read(3)
        assert len(output) == 3
        assert buffer.available == 2

    def test_peek(self):
        """Test peeking without consuming."""
        buffer = sonickit.AudioBuffer(1000)
        data = np.array([10, 20, 30], dtype=np.int16)
        buffer.write(data)

        peeked = buffer.peek(2)
        assert len(peeked) == 2
        assert buffer.available == 3  # Not consumed

    def test_clear(self):
        """Test clearing buffer."""
        buffer = sonickit.AudioBuffer(1000)
        data = np.array([1, 2, 3], dtype=np.int16)
        buffer.write(data)
        assert buffer.available == 3

        buffer.clear()
        assert buffer.available == 0

    def test_free_space(self):
        """Test free space calculation."""
        buffer = sonickit.AudioBuffer(100)
        assert buffer.free_space == 100

        data = np.zeros(30, dtype=np.int16)
        buffer.write(data)
        assert buffer.free_space == 70


class TestAudioLevel:
    """Tests for AudioLevel class."""

    def test_create(self):
        """Test creating level meter."""
        meter = sonickit.AudioLevel(16000, 160)
        assert meter is not None

    def test_process_silence(self):
        """Test processing silence."""
        meter = sonickit.AudioLevel(16000, 160)
        silence = np.zeros(160, dtype=np.int16)
        meter.process(silence)

        level = meter.get_level_db()
        assert level < -50.0  # Silence should be very low

    def test_process_loud(self):
        """Test processing loud signal."""
        meter = sonickit.AudioLevel(16000, 160)
        loud = np.full(160, 20000, dtype=np.int16)
        meter.process(loud)

        level = meter.get_level_db()
        assert level > -20.0  # Should be fairly loud

    def test_get_peak_rms(self):
        """Test getting peak and RMS levels."""
        meter = sonickit.AudioLevel(16000, 160)
        audio = np.random.randint(-10000, 10000, 160, dtype=np.int16)
        meter.process(audio)

        peak = meter.get_peak_db()
        rms = meter.get_rms_db()
        assert isinstance(peak, float)
        assert isinstance(rms, float)
        assert peak >= rms  # Peak should always be >= RMS

    def test_is_silence(self):
        """Test silence detection."""
        meter = sonickit.AudioLevel(16000, 160)
        silence = np.zeros(160, dtype=np.int16)
        meter.process(silence)
        assert meter.is_silence()

    def test_is_clipping(self):
        """Test clipping detection."""
        meter = sonickit.AudioLevel(16000, 160)
        clipped = np.full(160, 32767, dtype=np.int16)
        meter.process(clipped)
        assert meter.is_clipping()


class TestAudioMixer:
    """Tests for AudioMixer class."""

    def test_create(self):
        """Test creating mixer."""
        mixer = sonickit.AudioMixer(16000, 4, 160)
        assert mixer is not None
        assert mixer.num_inputs == 4

    def test_mix_single_input(self):
        """Test mixing with single input."""
        mixer = sonickit.AudioMixer(16000, 1, 160)
        audio = np.random.randint(-16000, 16000, 160, dtype=np.int16)
        mixer.set_input(0, audio)

        output = mixer.mix(160)
        assert len(output) == 160

    def test_mix_multiple_inputs(self):
        """Test mixing multiple inputs."""
        mixer = sonickit.AudioMixer(16000, 2, 160)

        audio1 = np.random.randint(-8000, 8000, 160, dtype=np.int16)
        audio2 = np.random.randint(-8000, 8000, 160, dtype=np.int16)

        mixer.set_input(0, audio1)
        mixer.set_input(1, audio2)

        output = mixer.mix(160)
        assert len(output) == 160

    def test_set_input_gain(self):
        """Test setting input gain."""
        mixer = sonickit.AudioMixer(16000, 2, 160)
        mixer.set_input_gain(0, 0.5)
        mixer.set_input_gain(1, 1.5)

    def test_set_master_gain(self):
        """Test setting master gain."""
        mixer = sonickit.AudioMixer(16000, 2, 160)
        mixer.set_master_gain(0.8)


class TestJitterBuffer:
    """Tests for JitterBuffer class."""

    def test_create(self):
        """Test creating jitter buffer."""
        jitter = sonickit.JitterBuffer(16000, 20, 20, 200)
        assert jitter is not None

    def test_put_get(self):
        """Test putting and getting packets."""
        jitter = sonickit.JitterBuffer(16000, 20, 40, 200)

        # Put some packets
        audio1 = np.random.randint(-16000, 16000, 320, dtype=np.int16)
        audio2 = np.random.randint(-16000, 16000, 320, dtype=np.int16)

        jitter.put(audio1, timestamp=0, sequence=0)
        jitter.put(audio2, timestamp=320, sequence=1)

        # Get audio
        output = jitter.get(320)
        assert len(output) == 320

    def test_get_stats(self):
        """Test getting statistics."""
        jitter = sonickit.JitterBuffer(16000, 20)
        stats = jitter.get_stats()
        assert isinstance(stats, dict)

    def test_get_delay(self):
        """Test getting delay."""
        jitter = sonickit.JitterBuffer(16000, 20)
        delay = jitter.get_delay_ms()
        assert isinstance(delay, float)
        assert delay >= 0


class TestSpatialRenderer:
    """Tests for SpatialRenderer class."""

    def test_create(self):
        """Test creating spatial renderer."""
        spatial = sonickit.SpatialRenderer(48000, 480)
        assert spatial is not None

    def test_set_source_position(self):
        """Test setting source position."""
        spatial = sonickit.SpatialRenderer(48000, 480)
        spatial.set_source_position(1.0, 0.0, 0.0)  # Right
        spatial.set_source_position(-1.0, 0.0, 0.0)  # Left
        spatial.set_source_position(0.0, 1.0, 0.0)  # Up
        spatial.set_source_position(0.0, 0.0, 1.0)  # Front

    def test_set_listener(self):
        """Test setting listener position and orientation."""
        spatial = sonickit.SpatialRenderer(48000, 480)
        spatial.set_listener_position(0.0, 0.0, 0.0)
        spatial.set_listener_orientation(0.0, 0.0, 0.0)

    def test_process(self):
        """Test processing audio."""
        spatial = sonickit.SpatialRenderer(48000, 480)
        spatial.set_source_position(2.0, 0.0, 0.0)

        mono = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        stereo = spatial.process(mono)

        # Output should be stereo (2x samples)
        assert len(stereo) == 960

    def test_set_attenuation(self):
        """Test setting attenuation parameters."""
        spatial = sonickit.SpatialRenderer(48000, 480)
        spatial.set_attenuation(1.0, 100.0, 1.0)


class TestHRTF:
    """Tests for HRTF class."""

    def test_create(self):
        """Test creating HRTF processor."""
        hrtf = sonickit.HRTF(48000)
        assert hrtf is not None

    def test_set_position(self):
        """Test setting position."""
        hrtf = sonickit.HRTF(48000)
        hrtf.set_azimuth(45.0)
        hrtf.set_elevation(30.0)
        hrtf.set_distance(2.0)

    def test_process(self):
        """Test processing audio."""
        hrtf = sonickit.HRTF(48000)
        hrtf.set_azimuth(90.0)  # Right side

        mono = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        stereo = hrtf.process(mono)

        # Output should be stereo
        assert len(stereo) == 960


class TestG711Codec:
    """Tests for G711Codec class."""

    def test_create_alaw(self):
        """Test creating A-law codec."""
        codec = sonickit.G711Codec(use_alaw=True)
        assert codec is not None
        assert codec.is_alaw

    def test_create_ulaw(self):
        """Test creating μ-law codec."""
        codec = sonickit.G711Codec(use_alaw=False)
        assert codec is not None
        assert not codec.is_alaw

    def test_encode_decode_alaw(self):
        """Test A-law encode/decode."""
        codec = sonickit.G711Codec(use_alaw=True)
        original = np.random.randint(-16000, 16000, 160, dtype=np.int16)

        encoded = codec.encode(original)
        assert len(encoded) == 160  # 1 byte per sample
        assert encoded.dtype == np.uint8

        decoded = codec.decode(encoded)
        assert len(decoded) == 160
        assert decoded.dtype == np.int16

    def test_encode_decode_ulaw(self):
        """Test μ-law encode/decode."""
        codec = sonickit.G711Codec(use_alaw=False)
        original = np.random.randint(-16000, 16000, 160, dtype=np.int16)

        encoded = codec.encode(original)
        decoded = codec.decode(encoded)

        assert len(decoded) == 160
        # G.711 has some quantization error, but should be close
        # Allow reasonable error margin
        max_error = np.max(np.abs(original.astype(np.int32) - decoded.astype(np.int32)))
        assert max_error < 5000  # Reasonable error for G.711


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
