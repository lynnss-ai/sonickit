"""Tests for SonicKit effects bindings."""

import pytest
import numpy as np

try:
    import sonickit
    HAS_SONICKIT = True
except ImportError:
    HAS_SONICKIT = False

pytestmark = pytest.mark.skipif(not HAS_SONICKIT, reason="sonickit not installed")


class TestReverb:
    """Tests for Reverb class."""

    def test_create_default(self):
        """Test creating reverb with defaults."""
        reverb = sonickit.Reverb(48000)
        assert reverb is not None

    def test_create_with_params(self):
        """Test creating reverb with parameters."""
        reverb = sonickit.Reverb(
            48000,
            room_size=0.8,
            damping=0.6,
            wet_level=0.4,
            dry_level=0.6
        )
        assert reverb is not None

    def test_process(self):
        """Test processing audio."""
        reverb = sonickit.Reverb(48000)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = reverb.process(input_audio)
        assert output.shape == input_audio.shape

    def test_set_parameters(self):
        """Test setting reverb parameters."""
        reverb = sonickit.Reverb(48000)
        reverb.set_room_size(0.7)
        reverb.set_damping(0.5)
        reverb.set_wet_level(0.5)
        reverb.set_dry_level(0.5)

    def test_reset(self):
        """Test resetting reverb."""
        reverb = sonickit.Reverb(48000)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        reverb.process(input_audio)
        reverb.reset()


class TestDelay:
    """Tests for Delay class."""

    def test_create_default(self):
        """Test creating delay with defaults."""
        delay = sonickit.Delay(48000)
        assert delay is not None

    def test_create_with_params(self):
        """Test creating delay with parameters."""
        delay = sonickit.Delay(48000, delay_ms=500.0, feedback=0.5, mix=0.5)
        assert delay is not None

    def test_process(self):
        """Test processing audio."""
        delay = sonickit.Delay(48000, delay_ms=100.0)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = delay.process(input_audio)
        assert output.shape == input_audio.shape

    def test_set_parameters(self):
        """Test setting delay parameters."""
        delay = sonickit.Delay(48000)
        delay.set_delay_time(300.0)
        delay.set_feedback(0.6)
        delay.set_mix(0.4)


class TestPitchShifter:
    """Tests for PitchShifter class."""

    def test_create(self):
        """Test creating pitch shifter."""
        shifter = sonickit.PitchShifter(48000)
        assert shifter is not None

    def test_create_with_shift(self):
        """Test creating with initial shift."""
        shifter = sonickit.PitchShifter(48000, shift_semitones=5.0)
        assert shifter.get_shift() == pytest.approx(5.0, abs=0.1)

    def test_process(self):
        """Test processing audio."""
        shifter = sonickit.PitchShifter(48000, 2.0)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = shifter.process(input_audio)
        # Output size may vary due to pitch shifting
        assert len(output) > 0

    def test_set_get_shift(self):
        """Test setting and getting shift."""
        shifter = sonickit.PitchShifter(48000)
        shifter.set_shift(7.0)
        assert shifter.get_shift() == pytest.approx(7.0, abs=0.1)

        shifter.set_shift(-5.0)
        assert shifter.get_shift() == pytest.approx(-5.0, abs=0.1)


class TestChorus:
    """Tests for Chorus class."""

    def test_create_default(self):
        """Test creating chorus with defaults."""
        chorus = sonickit.Chorus(48000)
        assert chorus is not None

    def test_create_with_params(self):
        """Test creating chorus with parameters."""
        chorus = sonickit.Chorus(48000, rate=2.0, depth=0.7, mix=0.6)
        assert chorus is not None

    def test_process(self):
        """Test processing audio."""
        chorus = sonickit.Chorus(48000)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = chorus.process(input_audio)
        assert output.shape == input_audio.shape

    def test_set_parameters(self):
        """Test setting chorus parameters."""
        chorus = sonickit.Chorus(48000)
        chorus.set_rate(1.0)
        chorus.set_depth(0.8)
        chorus.set_mix(0.5)


class TestFlanger:
    """Tests for Flanger class."""

    def test_create_default(self):
        """Test creating flanger with defaults."""
        flanger = sonickit.Flanger(48000)
        assert flanger is not None

    def test_create_with_params(self):
        """Test creating flanger with parameters."""
        flanger = sonickit.Flanger(
            48000, rate=0.3, depth=0.6, feedback=0.7, mix=0.5
        )
        assert flanger is not None

    def test_process(self):
        """Test processing audio."""
        flanger = sonickit.Flanger(48000)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = flanger.process(input_audio)
        assert output.shape == input_audio.shape

    def test_set_parameters(self):
        """Test setting flanger parameters."""
        flanger = sonickit.Flanger(48000)
        flanger.set_rate(0.4)
        flanger.set_depth(0.5)
        flanger.set_feedback(-0.5)
        flanger.set_mix(0.6)


class TestTimeStretcher:
    """Tests for TimeStretcher class."""

    def test_create(self):
        """Test creating time stretcher."""
        stretcher = sonickit.TimeStretcher(48000)
        assert stretcher is not None

    def test_create_with_rate(self):
        """Test creating with initial rate."""
        stretcher = sonickit.TimeStretcher(48000, channels=1, initial_rate=1.5)
        assert stretcher.get_rate() == pytest.approx(1.5, abs=0.01)

    def test_process_normal(self):
        """Test processing at normal speed."""
        stretcher = sonickit.TimeStretcher(48000, 1, 1.0)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = stretcher.process(input_audio)
        assert len(output) > 0

    def test_process_slow(self):
        """Test processing at slower speed."""
        stretcher = sonickit.TimeStretcher(48000, 1, 0.5)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = stretcher.process(input_audio)
        # At 0.5x speed, output should be roughly 2x input length
        assert len(output) > 0

    def test_process_fast(self):
        """Test processing at faster speed."""
        stretcher = sonickit.TimeStretcher(48000, 1, 2.0)
        input_audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
        output = stretcher.process(input_audio)
        # At 2x speed, output should be roughly half input length
        assert len(output) > 0

    def test_set_get_rate(self):
        """Test setting and getting rate."""
        stretcher = sonickit.TimeStretcher(48000)
        stretcher.set_rate(0.75)
        assert stretcher.get_rate() == pytest.approx(0.75, abs=0.01)


class TestWatermark:
    """Tests for Watermark classes."""

    def test_embedder_create(self):
        """Test creating watermark embedder."""
        embedder = sonickit.WatermarkEmbedder(48000)
        assert embedder is not None

    def test_embedder_create_with_strength(self):
        """Test creating embedder with strength."""
        embedder = sonickit.WatermarkEmbedder(48000, strength=0.2)
        assert embedder is not None

    def test_embed_string(self):
        """Test embedding a string."""
        embedder = sonickit.WatermarkEmbedder(48000, 0.15)
        audio = np.random.randint(-16000, 16000, 48000, dtype=np.int16)  # 1 second

        marked = embedder.embed_string(audio, "test")
        assert marked.shape == audio.shape

    def test_embed_bytes(self):
        """Test embedding binary data."""
        embedder = sonickit.WatermarkEmbedder(48000, 0.15)
        audio = np.random.randint(-16000, 16000, 48000, dtype=np.int16)
        data = np.array([0x12, 0x34, 0x56, 0x78], dtype=np.uint8)

        marked = embedder.embed_bytes(audio, data)
        assert marked.shape == audio.shape

    def test_detector_create(self):
        """Test creating watermark detector."""
        detector = sonickit.WatermarkDetector(48000)
        assert detector is not None

    def test_detect_no_watermark(self):
        """Test detecting in unmarked audio."""
        detector = sonickit.WatermarkDetector(48000)
        audio = np.random.randint(-16000, 16000, 48000, dtype=np.int16)

        result = detector.has_watermark(audio)
        assert isinstance(result, bool)

    def test_roundtrip(self):
        """Test embedding and detecting watermark."""
        embedder = sonickit.WatermarkEmbedder(48000, 0.2)
        detector = sonickit.WatermarkDetector(48000)

        # Create clean audio (sine wave for more reliable detection)
        t = np.linspace(0, 1.0, 48000)
        audio = (np.sin(2 * np.pi * 440 * t) * 10000).astype(np.int16)

        # Embed watermark
        marked = embedder.embed_string(audio, "hello")

        # Detect watermark
        result = detector.detect_string(marked)
        assert isinstance(result, dict)
        assert "found" in result


class TestReverbPresets:
    """Tests for reverb presets."""

    def test_apply_preset(self):
        """Test applying reverb presets."""
        reverb = sonickit.Reverb(48000)

        # Test that preset application doesn't raise
        for preset_value in [0, 1, 2, 3, 4, 5]:
            reverb.set_preset(preset_value)

            # Process some audio to verify it still works
            audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)
            output = reverb.process(audio)
            assert len(output) == 480


class TestEffectsChain:
    """Integration tests for effects chains."""

    def test_chain_effects(self):
        """Test chaining multiple effects."""
        # Create effects
        eq = sonickit.Equalizer(48000, 3)
        comp = sonickit.Compressor(48000, -15.0, 4.0)
        reverb = sonickit.Reverb(48000, 0.6, 0.5, 0.3, 0.7)

        # Process through chain
        audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)

        audio = eq.process(audio)
        audio = comp.process(audio)
        audio = reverb.process(audio)

        assert len(audio) == 480
        assert audio.dtype == np.int16

    def test_parallel_effects(self):
        """Test processing same audio with parallel effects."""
        audio = np.random.randint(-16000, 16000, 480, dtype=np.int16)

        # Create effects
        chorus = sonickit.Chorus(48000)
        flanger = sonickit.Flanger(48000)
        delay = sonickit.Delay(48000)

        # Process in parallel
        chorus_out = chorus.process(audio.copy())
        flanger_out = flanger.process(audio.copy())
        delay_out = delay.process(audio.copy())

        # All should produce valid output
        assert len(chorus_out) == 480
        assert len(flanger_out) == 480
        assert len(delay_out) == 480


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
