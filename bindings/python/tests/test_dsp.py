"""Tests for SonicKit DSP bindings."""

import pytest
import numpy as np
from numpy.testing import assert_array_equal

try:
    import sonickit
    HAS_SONICKIT = True
except ImportError:
    HAS_SONICKIT = False

pytestmark = pytest.mark.skipif(not HAS_SONICKIT, reason="sonickit not installed")


class TestDenoiser:
    """Tests for Denoiser class."""

    def test_create_default(self):
        """Test creating denoiser with default settings."""
        denoiser = sonickit.Denoiser(16000, 480)
        assert denoiser is not None
        assert denoiser.frame_size == 480

    def test_create_different_sample_rates(self):
        """Test creating denoiser with different sample rates."""
        for rate in [8000, 16000, 48000]:
            frame_size = rate // 100  # 10ms frame
            denoiser = sonickit.Denoiser(rate, frame_size)
            assert denoiser.frame_size == frame_size

    def test_process(self):
        """Test processing audio."""
        denoiser = sonickit.Denoiser(16000, 160)
        input_audio = np.random.randint(-32768, 32767, 160, dtype=np.int16)
        output = denoiser.process(input_audio)
        assert output.shape == input_audio.shape
        assert output.dtype == np.int16

    def test_reset(self):
        """Test resetting denoiser state."""
        denoiser = sonickit.Denoiser(16000, 160)
        input_audio = np.random.randint(-32768, 32767, 160, dtype=np.int16)
        denoiser.process(input_audio)
        denoiser.reset()  # Should not raise


class TestEchoCanceller:
    """Tests for EchoCanceller class."""

    def test_create(self):
        """Test creating echo canceller."""
        aec = sonickit.EchoCanceller(16000, 160, 1600)
        assert aec is not None
        assert aec.frame_size == 160

    def test_process(self):
        """Test processing with AEC."""
        aec = sonickit.EchoCanceller(16000, 160)
        captured = np.random.randint(-32768, 32767, 160, dtype=np.int16)
        playback = np.random.randint(-32768, 32767, 160, dtype=np.int16)
        output = aec.process(captured, playback)
        assert output.shape == captured.shape
        assert output.dtype == np.int16

    def test_reset(self):
        """Test resetting AEC."""
        aec = sonickit.EchoCanceller(16000, 160)
        aec.reset()


class TestAGC:
    """Tests for AGC class."""

    def test_create_default(self):
        """Test creating AGC with defaults."""
        agc = sonickit.AGC(16000, 160)
        assert agc is not None

    def test_create_with_params(self):
        """Test creating AGC with parameters."""
        agc = sonickit.AGC(16000, 160, mode=2, target_level=-6.0)
        assert agc is not None

    def test_process(self):
        """Test processing with AGC."""
        agc = sonickit.AGC(16000, 160)
        input_audio = np.random.randint(-1000, 1000, 160, dtype=np.int16)
        output = agc.process(input_audio)
        assert output.shape == input_audio.shape
        assert output.dtype == np.int16

    def test_get_gain(self):
        """Test getting current gain."""
        agc = sonickit.AGC(16000, 160)
        input_audio = np.random.randint(-1000, 1000, 160, dtype=np.int16)
        agc.process(input_audio)
        gain = agc.get_gain()
        assert isinstance(gain, float)


class TestVAD:
    """Tests for VAD class."""

    def test_create(self):
        """Test creating VAD."""
        vad = sonickit.VAD(16000)
        assert vad is not None

    def test_create_modes(self):
        """Test creating VAD with different modes."""
        for mode in range(4):
            vad = sonickit.VAD(16000, mode=mode)
            assert vad is not None

    def test_is_speech_silence(self):
        """Test VAD with silence."""
        vad = sonickit.VAD(16000, mode=0)
        silence = np.zeros(160, dtype=np.int16)
        result = vad.is_speech(silence)
        assert isinstance(result, bool)
        # Silence should typically not be speech
        assert not result

    def test_is_speech_noise(self):
        """Test VAD with audio."""
        vad = sonickit.VAD(16000, mode=0)
        # Create a sine wave that simulates voice
        t = np.linspace(0, 0.01, 160)
        audio = (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)
        result = vad.is_speech(audio)
        assert isinstance(result, bool)

    def test_get_probability(self):
        """Test getting speech probability."""
        vad = sonickit.VAD(16000)
        audio = np.random.randint(-1000, 1000, 160, dtype=np.int16)
        vad.is_speech(audio)
        prob = vad.get_probability()
        assert 0.0 <= prob <= 1.0


class TestResampler:
    """Tests for Resampler class."""

    def test_create(self):
        """Test creating resampler."""
        resampler = sonickit.Resampler(1, 16000, 48000)
        assert resampler is not None

    def test_upsample(self):
        """Test upsampling audio."""
        resampler = sonickit.Resampler(1, 16000, 48000, quality=5)
        input_audio = np.random.randint(-32768, 32767, 160, dtype=np.int16)
        output = resampler.process(input_audio)
        # 48000/16000 = 3x, so output should be ~480 samples
        assert len(output) >= 480

    def test_downsample(self):
        """Test downsampling audio."""
        resampler = sonickit.Resampler(1, 48000, 16000, quality=5)
        input_audio = np.random.randint(-32768, 32767, 480, dtype=np.int16)
        output = resampler.process(input_audio)
        # 16000/48000 = 1/3, so output should be ~160 samples
        assert len(output) >= 160


class TestDTMFDetector:
    """Tests for DTMFDetector class."""

    def test_create(self):
        """Test creating DTMF detector."""
        detector = sonickit.DTMFDetector(8000, 160)
        assert detector is not None

    def test_process_silence(self):
        """Test processing silence."""
        detector = sonickit.DTMFDetector(8000, 160)
        silence = np.zeros(160, dtype=np.int16)
        digit = detector.process(silence)
        assert digit == ""

    def test_get_digits(self):
        """Test getting accumulated digits."""
        detector = sonickit.DTMFDetector(8000, 160)
        digits = detector.get_digits()
        assert isinstance(digits, str)

    def test_clear_digits(self):
        """Test clearing digits."""
        detector = sonickit.DTMFDetector(8000, 160)
        detector.clear_digits()
        assert detector.get_digits() == ""


class TestDTMFGenerator:
    """Tests for DTMFGenerator class."""

    def test_create(self):
        """Test creating DTMF generator."""
        generator = sonickit.DTMFGenerator(8000)
        assert generator is not None

    def test_generate_digit(self):
        """Test generating a single digit."""
        generator = sonickit.DTMFGenerator(8000, 100, 50)
        audio = generator.generate_digit("5")
        assert len(audio) > 0
        assert audio.dtype == np.int16

    def test_generate_sequence(self):
        """Test generating a digit sequence."""
        generator = sonickit.DTMFGenerator(8000, 100, 50)
        audio = generator.generate_sequence("123")
        # 3 digits * (100ms tone + 50ms pause) = 450ms = 3600 samples at 8kHz
        assert len(audio) >= 3600


class TestEqualizer:
    """Tests for Equalizer class."""

    def test_create(self):
        """Test creating equalizer."""
        eq = sonickit.Equalizer(48000, 5)
        assert eq is not None

    def test_process(self):
        """Test processing audio."""
        eq = sonickit.Equalizer(48000, 5)
        input_audio = np.random.randint(-32768, 32767, 480, dtype=np.int16)
        output = eq.process(input_audio)
        assert output.shape == input_audio.shape

    def test_set_band(self):
        """Test setting EQ band."""
        eq = sonickit.Equalizer(48000, 5)
        eq.set_band(0, 100.0, 6.0, 1.0)  # Bass boost
        eq.set_band(4, 10000.0, -3.0, 0.7)  # Treble cut

    def test_set_master_gain(self):
        """Test setting master gain."""
        eq = sonickit.Equalizer(48000, 5)
        eq.set_master_gain(-6.0)


class TestCompressor:
    """Tests for Compressor class."""

    def test_create(self):
        """Test creating compressor."""
        comp = sonickit.Compressor(48000)
        assert comp is not None

    def test_create_with_params(self):
        """Test creating compressor with parameters."""
        comp = sonickit.Compressor(48000, -20.0, 4.0, 10.0, 100.0)
        assert comp is not None

    def test_process(self):
        """Test processing audio."""
        comp = sonickit.Compressor(48000)
        input_audio = np.random.randint(-32768, 32767, 480, dtype=np.int16)
        output = comp.process(input_audio)
        assert output.shape == input_audio.shape

    def test_set_parameters(self):
        """Test setting compressor parameters."""
        comp = sonickit.Compressor(48000)
        comp.set_threshold(-15.0)
        comp.set_ratio(8.0)
        comp.set_times(5.0, 50.0)


class TestComfortNoise:
    """Tests for ComfortNoise class."""

    def test_create(self):
        """Test creating CNG."""
        cng = sonickit.ComfortNoise(16000, 160)
        assert cng is not None

    def test_generate(self):
        """Test generating comfort noise."""
        cng = sonickit.ComfortNoise(16000, 160, -50.0)
        noise = cng.generate(160)
        assert len(noise) == 160
        assert noise.dtype == np.int16

    def test_analyze(self):
        """Test analyzing noise."""
        cng = sonickit.ComfortNoise(16000, 160)
        audio = np.random.randint(-100, 100, 160, dtype=np.int16)
        cng.analyze(audio)

    def test_set_get_level(self):
        """Test setting and getting noise level."""
        cng = sonickit.ComfortNoise(16000, 160)
        cng.set_level(-40.0)
        level = cng.get_level()
        assert isinstance(level, float)


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
