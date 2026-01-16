"""Configuration for pytest."""

import pytest
import numpy as np


def pytest_configure(config):
    """Configure pytest with custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow"
    )
    config.addinivalue_line(
        "markers", "integration: marks tests as integration tests"
    )


@pytest.fixture
def sample_rate():
    """Default sample rate for tests."""
    return 16000


@pytest.fixture
def frame_size():
    """Default frame size for tests (10ms at 16kHz)."""
    return 160


@pytest.fixture
def silence(frame_size):
    """Generate silent audio."""
    return np.zeros(frame_size, dtype=np.int16)


@pytest.fixture
def noise(frame_size):
    """Generate random noise audio."""
    return np.random.randint(-16000, 16000, frame_size, dtype=np.int16)


@pytest.fixture
def sine_wave(sample_rate, frame_size):
    """Generate a sine wave at 440 Hz."""
    t = np.linspace(0, frame_size / sample_rate, frame_size)
    return (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)


@pytest.fixture
def one_second_audio(sample_rate):
    """Generate one second of audio."""
    t = np.linspace(0, 1.0, sample_rate)
    return (np.sin(2 * np.pi * 440 * t) * 16000).astype(np.int16)
