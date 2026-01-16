"""
Example: Audio Effects with SonicKit

This example demonstrates audio effects:
- Reverb
- Delay
- Pitch shifting
- Chorus and Flanger
- Time stretching
"""

import numpy as np

import sonickit


def demonstrate_reverb():
    """Demonstrate reverb effect with different presets."""
    print("Reverb Effect Demo")
    print("-" * 40)

    sample_rate = 48000
    duration = 1.0  # 1 second

    # Generate a short percussion-like sound
    t = np.linspace(0, duration, int(sample_rate * duration))
    # Exponential decay envelope
    envelope = np.exp(-t * 10)
    # Impulse with some harmonics
    audio = (np.sin(2 * np.pi * 200 * t) * envelope * 16000).astype(np.int16)

    # Apply reverb with different settings
    presets = [
        ("Small Room", 0.2, 0.5),
        ("Medium Room", 0.5, 0.5),
        ("Large Hall", 0.8, 0.3),
        ("Cathedral", 0.95, 0.2),
    ]

    for name, room_size, damping in presets:
        reverb = sonickit.Reverb(
            sample_rate,
            room_size=room_size,
            damping=damping,
            wet_level=0.4,
            dry_level=0.6
        )

        output = reverb.process(audio)

        # Calculate tail length (where signal drops below threshold)
        threshold = 100
        tail_samples = len(output)
        for i in range(len(output) - 1, 0, -1):
            if abs(output[i]) > threshold:
                tail_samples = i
                break

        tail_ms = tail_samples / sample_rate * 1000
        print(f"  {name:15}: Room={room_size:.1f}, Damp={damping:.1f}, "
              f"Tail≈{tail_ms:.0f}ms")

    print()


def demonstrate_delay():
    """Demonstrate delay/echo effect."""
    print("Delay Effect Demo")
    print("-" * 40)

    sample_rate = 48000
    frame_size = 480

    # Create impulse to hear the echoes clearly
    audio = np.zeros(sample_rate, dtype=np.int16)
    audio[0] = 16000  # Single impulse

    # Different delay settings
    delay_settings = [
        (100, 0.5, "Fast echo"),
        (250, 0.4, "Slapback"),
        (500, 0.3, "Long echo"),
    ]

    for delay_ms, feedback, name in delay_settings:
        delay = sonickit.Delay(sample_rate, delay_ms, feedback, 0.5)

        output = np.zeros_like(audio)
        for i in range(0, len(audio), frame_size):
            end = min(i + frame_size, len(audio))
            frame = audio[i:end]
            processed = delay.process(frame)
            output[i:i+len(processed)] = processed

        # Count echoes (peaks above threshold)
        threshold = 500
        peaks = np.sum(np.abs(output) > threshold)

        print(f"  {name:12}: {delay_ms}ms delay, {feedback} feedback, "
              f"~{peaks} peaks detected")

    print()


def demonstrate_pitch_shift():
    """Demonstrate pitch shifting."""
    print("Pitch Shift Demo")
    print("-" * 40)

    sample_rate = 48000
    duration = 0.5

    # Generate a clear tone
    t = np.linspace(0, duration, int(sample_rate * duration))
    freq = 440  # A4 note
    audio = (np.sin(2 * np.pi * freq * t) * 16000).astype(np.int16)

    shifts = [
        (-12, "One octave down"),
        (-7, "Perfect fifth down"),
        (-5, "Perfect fourth down"),
        (0, "No shift"),
        (5, "Perfect fourth up"),
        (7, "Perfect fifth up"),
        (12, "One octave up"),
    ]

    for semitones, name in shifts:
        shifter = sonickit.PitchShifter(sample_rate, semitones)
        output = shifter.process(audio)

        # Calculate expected frequency
        expected_freq = freq * (2 ** (semitones / 12))

        print(f"  {semitones:+3d} semitones: {name:20} "
              f"({freq}Hz → {expected_freq:.1f}Hz)")

    print()


def demonstrate_modulation():
    """Demonstrate chorus and flanger effects."""
    print("Modulation Effects Demo")
    print("-" * 40)

    sample_rate = 48000
    duration = 1.0

    # Generate a sustained tone
    t = np.linspace(0, duration, int(sample_rate * duration))
    audio = (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)

    # Chorus effect
    print("  Chorus Effect:")
    chorus = sonickit.Chorus(sample_rate, rate=1.5, depth=0.5, mix=0.5)
    chorus_out = chorus.process(audio)
    print(f"    Input: {len(audio)} samples, Output: {len(chorus_out)} samples")

    # Flanger effect
    print("  Flanger Effect:")
    flanger = sonickit.Flanger(sample_rate, rate=0.5, depth=0.7, feedback=0.7, mix=0.5)
    flanger_out = flanger.process(audio)
    print(f"    Input: {len(audio)} samples, Output: {len(flanger_out)} samples")

    print()


def demonstrate_time_stretch():
    """Demonstrate time stretching (tempo change without pitch change)."""
    print("Time Stretch Demo")
    print("-" * 40)

    sample_rate = 48000
    duration = 1.0

    # Generate audio
    t = np.linspace(0, duration, int(sample_rate * duration))
    audio = (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)

    rates = [
        (0.5, "Half speed (2x duration)"),
        (0.75, "75% speed"),
        (1.0, "Normal speed"),
        (1.5, "150% speed"),
        (2.0, "Double speed (0.5x duration)"),
    ]

    for rate, name in rates:
        stretcher = sonickit.TimeStretcher(sample_rate, 1, rate)
        output = stretcher.process(audio)

        expected_len = int(len(audio) / rate)
        actual_len = len(output)

        print(f"  Rate {rate:.2f}: {name}")
        print(f"    Input: {len(audio)}, Output: {actual_len} "
              f"(expected ~{expected_len})")

    print()


def demonstrate_equalizer():
    """Demonstrate parametric equalizer."""
    print("Equalizer Demo")
    print("-" * 40)

    sample_rate = 48000

    # Create EQ with 5 bands
    eq = sonickit.Equalizer(sample_rate, 5)

    # Set up a voice enhancement preset
    # Bass rolloff, presence boost, air boost
    bands = [
        (100, -3.0, 0.7, "Sub cut"),
        (250, -2.0, 1.0, "Mud cut"),
        (1000, 0.0, 1.0, "Neutral"),
        (3000, 3.0, 1.5, "Presence"),
        (10000, 2.0, 0.7, "Air"),
    ]

    print("  Voice Enhancement Preset:")
    for i, (freq, gain, q, name) in enumerate(bands):
        eq.set_band(i, freq, gain, q)
        print(f"    Band {i}: {freq:5}Hz {gain:+5.1f}dB Q={q:.1f} ({name})")

    # Generate test signal
    duration = 0.1
    t = np.linspace(0, duration, int(sample_rate * duration))
    audio = (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)

    # Process
    output = eq.process(audio)
    print(f"\n  Processed {len(audio)} samples")

    print()


def demonstrate_dynamics():
    """Demonstrate compressor."""
    print("Dynamics Processing Demo")
    print("-" * 40)

    sample_rate = 48000

    # Create compressor
    compressor = sonickit.Compressor(
        sample_rate,
        threshold_db=-20.0,
        ratio=4.0,
        attack_ms=10.0,
        release_ms=100.0
    )

    print("  Compressor Settings:")
    print("    Threshold: -20 dB")
    print("    Ratio: 4:1")
    print("    Attack: 10 ms")
    print("    Release: 100 ms")

    # Generate audio with varying levels
    duration = 0.1
    t = np.linspace(0, duration, int(sample_rate * duration))

    # Quiet signal
    quiet = (np.sin(2 * np.pi * 440 * t) * 3000).astype(np.int16)
    quiet_out = compressor.process(quiet)
    quiet_reduction = np.max(np.abs(quiet)) - np.max(np.abs(quiet_out))

    # Loud signal
    compressor.reset()
    loud = (np.sin(2 * np.pi * 440 * t) * 25000).astype(np.int16)
    loud_out = compressor.process(loud)
    loud_reduction = np.max(np.abs(loud)) - np.max(np.abs(loud_out))

    print(f"\n  Results:")
    print(f"    Quiet signal: peak {np.max(np.abs(quiet)):5} → {np.max(np.abs(quiet_out)):5} "
          f"(reduction: {quiet_reduction})")
    print(f"    Loud signal:  peak {np.max(np.abs(loud)):5} → {np.max(np.abs(loud_out)):5} "
          f"(reduction: {loud_reduction})")

    print()


if __name__ == "__main__":
    print("=" * 50)
    print("SonicKit Audio Effects Examples")
    print("=" * 50)
    print()

    demonstrate_reverb()
    demonstrate_delay()
    demonstrate_pitch_shift()
    demonstrate_modulation()
    demonstrate_time_stretch()
    demonstrate_equalizer()
    demonstrate_dynamics()

    print("=" * 50)
    print("All examples complete!")
