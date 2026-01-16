"""
Example: Basic Audio Processing with SonicKit

This example demonstrates basic DSP operations:
- Noise reduction
- Voice Activity Detection
- Level metering
"""

import numpy as np

# Import SonicKit
import sonickit


def process_audio_file():
    """Process audio with basic DSP operations."""

    # Configuration
    sample_rate = 16000
    frame_size = 160  # 10ms at 16kHz

    # Create DSP processors
    denoiser = sonickit.Denoiser(sample_rate, frame_size)
    vad = sonickit.VAD(sample_rate, mode=1)
    level_meter = sonickit.AudioLevel(sample_rate, frame_size)
    agc = sonickit.AGC(sample_rate, frame_size, mode=1, target_level=-3.0)

    print("SonicKit Audio Processing Example")
    print("=" * 40)
    print(f"Sample Rate: {sample_rate} Hz")
    print(f"Frame Size: {frame_size} samples ({frame_size/sample_rate*1000:.1f}ms)")
    print()

    # Simulate processing audio frames
    # In a real application, you would read from a file or audio device

    total_frames = 100
    speech_frames = 0

    for i in range(total_frames):
        # Generate simulated audio (alternating speech/silence)
        if i % 20 < 15:
            # Simulate speech - sine wave with noise
            t = np.linspace(0, frame_size / sample_rate, frame_size)
            audio = (np.sin(2 * np.pi * 300 * t) * 5000).astype(np.int16)
            audio += np.random.randint(-1000, 1000, frame_size, dtype=np.int16)
        else:
            # Simulate silence with background noise
            audio = np.random.randint(-100, 100, frame_size, dtype=np.int16)

        # Apply noise reduction
        denoised = denoiser.process(audio)

        # Check for speech
        is_speech = vad.is_speech(denoised)
        if is_speech:
            speech_frames += 1

        # Apply AGC only to speech
        if is_speech:
            processed = agc.process(denoised)
        else:
            processed = denoised

        # Measure level
        level_meter.process(processed)

        # Print status every 20 frames
        if (i + 1) % 20 == 0:
            level = level_meter.get_level_db()
            prob = vad.get_probability()
            print(f"Frame {i+1:3d}: Level={level:6.1f}dB, Speech Prob={prob:.2f}")

    print()
    print(f"Processing complete!")
    print(f"Speech detected in {speech_frames}/{total_frames} frames "
          f"({speech_frames/total_frames*100:.1f}%)")


def demonstrate_resampling():
    """Demonstrate sample rate conversion."""
    print("\n" + "=" * 40)
    print("Sample Rate Conversion Example")
    print("=" * 40)

    # Create test signal at 16 kHz
    in_rate = 16000
    out_rate = 48000
    duration = 0.1  # 100ms

    t = np.linspace(0, duration, int(in_rate * duration))
    original = (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)

    print(f"Original: {len(original)} samples at {in_rate} Hz")

    # Upsample to 48 kHz
    resampler_up = sonickit.Resampler(1, in_rate, out_rate, quality=5)
    upsampled = resampler_up.process(original)

    print(f"Upsampled: {len(upsampled)} samples at {out_rate} Hz")
    print(f"Ratio: {len(upsampled)/len(original):.2f}x (expected {out_rate/in_rate:.2f}x)")

    # Downsample back to 16 kHz
    resampler_down = sonickit.Resampler(1, out_rate, in_rate, quality=5)
    downsampled = resampler_down.process(upsampled)

    print(f"Downsampled: {len(downsampled)} samples at {in_rate} Hz")


def demonstrate_dtmf():
    """Demonstrate DTMF generation and detection."""
    print("\n" + "=" * 40)
    print("DTMF Generation and Detection Example")
    print("=" * 40)

    sample_rate = 8000

    # Generate DTMF tones
    generator = sonickit.DTMFGenerator(sample_rate, 100, 50)
    detector = sonickit.DTMFDetector(sample_rate, 160)

    # Generate a phone number
    phone_number = "5551234"
    print(f"Generating DTMF for: {phone_number}")

    audio = generator.generate_sequence(phone_number)
    print(f"Generated {len(audio)} samples ({len(audio)/sample_rate*1000:.0f}ms)")

    # Detect the digits
    print("Detecting digits...")

    frame_size = 160
    for i in range(0, len(audio) - frame_size, frame_size):
        frame = audio[i:i+frame_size]
        digit = detector.process(frame)
        if digit:
            print(f"  Detected: {digit}")

    detected = detector.get_digits()
    print(f"All detected digits: {detected}")
    print(f"Match: {'Yes' if detected == phone_number else 'No'}")


if __name__ == "__main__":
    process_audio_file()
    demonstrate_resampling()
    demonstrate_dtmf()

    print("\n" + "=" * 40)
    print("Examples complete!")
