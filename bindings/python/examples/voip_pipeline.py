"""
Example: VoIP Audio Processing Pipeline

This example demonstrates a complete VoIP audio processing pipeline:
- Echo cancellation
- Noise reduction
- Voice activity detection
- Automatic gain control
- G.711 codec encoding/decoding
- Jitter buffer management
"""

import numpy as np
from typing import List, Optional
import time

import sonickit


class VoIPAudioPipeline:
    """Complete VoIP audio processing pipeline."""

    def __init__(
        self,
        sample_rate: int = 16000,
        frame_duration_ms: int = 20,
        use_alaw: bool = True
    ):
        """
        Initialize VoIP pipeline.

        Args:
            sample_rate: Audio sample rate in Hz
            frame_duration_ms: Frame duration in milliseconds
            use_alaw: Use A-law (True) or μ-law (False) for G.711
        """
        self.sample_rate = sample_rate
        self.frame_size = sample_rate * frame_duration_ms // 1000

        # Create processing chain
        self.aec = sonickit.EchoCanceller(
            sample_rate, self.frame_size,
            filter_length=sample_rate // 2  # 500ms echo tail
        )

        self.denoiser = sonickit.Denoiser(sample_rate, self.frame_size)

        self.vad = sonickit.VAD(sample_rate, mode=1)

        self.agc = sonickit.AGC(
            sample_rate, self.frame_size,
            mode=1,  # Adaptive
            target_level=-3.0
        )

        self.comfort_noise = sonickit.ComfortNoise(
            sample_rate, self.frame_size, -50.0
        )

        self.level_meter = sonickit.AudioLevel(sample_rate, self.frame_size)

        self.codec = sonickit.G711Codec(use_alaw=use_alaw)

        self.jitter_buffer = sonickit.JitterBuffer(
            sample_rate,
            frame_size_ms=frame_duration_ms,
            min_delay_ms=40,
            max_delay_ms=200
        )

        # Statistics
        self.stats = {
            "frames_processed": 0,
            "speech_frames": 0,
            "silence_frames": 0,
            "bytes_encoded": 0,
            "packets_sent": 0,
            "packets_received": 0,
        }

    def process_capture(
        self,
        captured: np.ndarray,
        playback: np.ndarray
    ) -> Optional[bytes]:
        """
        Process captured audio for transmission.

        Args:
            captured: Microphone input samples
            playback: Speaker output samples (for echo reference)

        Returns:
            Encoded packet or None if silence
        """
        # Step 1: Echo cancellation
        echo_cancelled = self.aec.process(captured, playback)

        # Step 2: Noise reduction
        denoised = self.denoiser.process(echo_cancelled)

        # Step 3: Voice activity detection
        is_speech = self.vad.is_speech(denoised)

        self.stats["frames_processed"] += 1

        if is_speech:
            # Step 4: AGC for speech
            normalized = self.agc.process(denoised)

            # Step 5: Level metering
            self.level_meter.process(normalized)

            # Step 6: Encode with G.711
            encoded = self.codec.encode(normalized)

            self.stats["speech_frames"] += 1
            self.stats["bytes_encoded"] += len(encoded)

            return bytes(encoded)
        else:
            # Analyze silence for comfort noise parameters
            self.comfort_noise.analyze(denoised)
            self.stats["silence_frames"] += 1

            # Return special "silence" indicator
            return None

    def send_packet(self, encoded: bytes, timestamp: int, sequence: int):
        """Simulate sending a packet."""
        self.stats["packets_sent"] += 1
        # In real app: socket.send(...)

    def receive_packet(
        self,
        encoded: bytes,
        timestamp: int,
        sequence: int
    ):
        """
        Receive and buffer an incoming packet.

        Args:
            encoded: G.711 encoded audio
            timestamp: RTP timestamp
            sequence: Sequence number
        """
        # Decode
        decoded = self.codec.decode(np.frombuffer(encoded, dtype=np.uint8))

        # Add to jitter buffer
        self.jitter_buffer.put(decoded, timestamp, sequence)
        self.stats["packets_received"] += 1

    def get_playback_audio(self) -> np.ndarray:
        """
        Get audio for playback from jitter buffer.

        Returns:
            Audio samples for playback
        """
        return self.jitter_buffer.get(self.frame_size)

    def get_level_db(self) -> float:
        """Get current input level in dB."""
        return self.level_meter.get_level_db()

    def get_speech_probability(self) -> float:
        """Get speech probability (0-1)."""
        return self.vad.get_probability()

    def get_jitter_stats(self) -> dict:
        """Get jitter buffer statistics."""
        return self.jitter_buffer.get_stats()

    def reset(self):
        """Reset all processors."""
        self.aec.reset()
        self.denoiser.reset()
        self.vad.reset()
        self.agc.reset()
        self.comfort_noise.reset()
        self.level_meter.reset()
        self.jitter_buffer.reset()


def simulate_voip_call():
    """Simulate a VoIP call with the processing pipeline."""

    print("=" * 60)
    print("VoIP Audio Processing Pipeline Simulation")
    print("=" * 60)
    print()

    # Create pipeline
    pipeline = VoIPAudioPipeline(
        sample_rate=16000,
        frame_duration_ms=20,
        use_alaw=True
    )

    print(f"Configuration:")
    print(f"  Sample Rate: {pipeline.sample_rate} Hz")
    print(f"  Frame Size: {pipeline.frame_size} samples ({20}ms)")
    print(f"  Codec: G.711 A-law")
    print()

    # Simulate 5 seconds of audio
    duration_sec = 5
    frame_duration_ms = 20
    total_frames = duration_sec * 1000 // frame_duration_ms

    print(f"Simulating {duration_sec} seconds ({total_frames} frames)...")
    print()

    sequence = 0
    timestamp = 0

    for i in range(total_frames):
        # Generate simulated audio
        # Alternate between speech and silence
        frame_in_cycle = i % 50  # 50 frames = 1 second

        if frame_in_cycle < 35:  # 70% speech
            # Simulate speech
            t = np.linspace(0, pipeline.frame_size / pipeline.sample_rate,
                           pipeline.frame_size)
            freq = 200 + (i % 10) * 20  # Varying pitch
            captured = (np.sin(2 * np.pi * freq * t) * 5000).astype(np.int16)
            captured += np.random.randint(-500, 500, pipeline.frame_size,
                                         dtype=np.int16)
        else:
            # Silence with noise
            captured = np.random.randint(-100, 100, pipeline.frame_size,
                                        dtype=np.int16)

        # Simulate echo from speaker
        playback = np.zeros(pipeline.frame_size, dtype=np.int16)
        if i > 5:  # Add some echo after initial frames
            playback = (captured * 0.1).astype(np.int16)

        # Process capture
        encoded = pipeline.process_capture(captured, playback)

        if encoded:
            # Simulate sending
            pipeline.send_packet(encoded, timestamp, sequence)

            # Simulate receiving (loopback for demo)
            # Add some jitter (random delay)
            pipeline.receive_packet(encoded, timestamp, sequence)

            sequence += 1

        timestamp += pipeline.frame_size

        # Get playback audio
        playback_audio = pipeline.get_playback_audio()

        # Print progress every second
        if (i + 1) % 50 == 0:
            sec = (i + 1) * frame_duration_ms // 1000
            level = pipeline.get_level_db()
            prob = pipeline.get_speech_probability()
            print(f"  Second {sec}: Level={level:6.1f}dB, "
                  f"Speech={prob:.2f}, "
                  f"Frames={pipeline.stats['frames_processed']}")

    # Print final statistics
    print()
    print("=" * 60)
    print("Processing Statistics:")
    print("=" * 60)
    stats = pipeline.stats

    total = stats['frames_processed']
    speech_pct = stats['speech_frames'] / total * 100 if total > 0 else 0
    silence_pct = stats['silence_frames'] / total * 100 if total > 0 else 0

    print(f"  Total Frames Processed: {total}")
    print(f"  Speech Frames: {stats['speech_frames']} ({speech_pct:.1f}%)")
    print(f"  Silence Frames: {stats['silence_frames']} ({silence_pct:.1f}%)")
    print(f"  Bytes Encoded: {stats['bytes_encoded']}")
    print(f"  Packets Sent: {stats['packets_sent']}")
    print(f"  Packets Received: {stats['packets_received']}")

    # Jitter buffer stats
    jitter_stats = pipeline.get_jitter_stats()
    print()
    print("Jitter Buffer Statistics:")
    for key, value in jitter_stats.items():
        print(f"  {key}: {value}")

    print()
    print("Simulation complete!")


if __name__ == "__main__":
    simulate_voip_call()
