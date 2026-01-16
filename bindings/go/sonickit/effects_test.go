package sonickit

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestReverb(t *testing.T) {
	reverb, err := NewReverb(48000, 0.7, 0.3)
	require.NoError(t, err)
	require.NotNil(t, reverb)
	defer reverb.Close()

	// Set parameters
	reverb.SetRoomSize(0.8)
	reverb.SetWetLevel(0.4)

	// Process
	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 30)
	}
	output := reverb.Process(input)
	assert.Len(t, output, len(input))
}

func TestDelay(t *testing.T) {
	delay, err := NewDelay(48000, 250, 0.4)
	require.NoError(t, err)
	require.NotNil(t, delay)
	defer delay.Close()

	// Set parameters
	delay.SetDelayTime(300)
	delay.SetFeedback(0.5)

	// Process
	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 30)
	}
	output := delay.Process(input)
	assert.Len(t, output, len(input))
}

func TestPitchShifter(t *testing.T) {
	shifter, err := NewPitchShifter(48000, 5.0)
	require.NoError(t, err)
	require.NotNil(t, shifter)
	defer shifter.Close()

	// Set pitch
	shifter.SetPitch(-3.0)

	// Process
	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 30)
	}
	output := shifter.Process(input)
	assert.Len(t, output, len(input))
}

func TestChorus(t *testing.T) {
	chorus, err := NewChorus(48000, 0.5, 1.5)
	require.NoError(t, err)
	require.NotNil(t, chorus)
	defer chorus.Close()

	// Set parameters
	chorus.SetDepth(0.6)
	chorus.SetRate(2.0)

	// Process
	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 30)
	}
	output := chorus.Process(input)
	assert.Len(t, output, len(input))
}

func TestFlanger(t *testing.T) {
	flanger, err := NewFlanger(48000, 0.5, 0.5)
	require.NoError(t, err)
	require.NotNil(t, flanger)
	defer flanger.Close()

	// Set parameters
	flanger.SetDepth(0.7)
	flanger.SetRate(0.8)

	// Process
	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 30)
	}
	output := flanger.Process(input)
	assert.Len(t, output, len(input))
}

func TestTimeStretcher(t *testing.T) {
	stretcher, err := NewTimeStretcher(48000, 1.5)
	require.NoError(t, err)
	require.NotNil(t, stretcher)
	defer stretcher.Close()

	// Set ratio
	stretcher.SetRatio(0.8)

	// Process
	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 30)
	}
	output := stretcher.Process(input)
	assert.Greater(t, len(output), 0)
}

func TestWatermarkEmbedder(t *testing.T) {
	embedder, err := NewWatermarkEmbedder(48000, 0.1)
	require.NoError(t, err)
	require.NotNil(t, embedder)
	defer embedder.Close()

	// Embed watermark
	input := make([]int16, 4800) // 100ms at 48kHz
	for i := range input {
		input[i] = int16(i * 5)
	}
	payload := []byte("test-watermark")
	output := embedder.Embed(input, payload)
	assert.Len(t, output, len(input))
}

func TestWatermarkDetector(t *testing.T) {
	detector, err := NewWatermarkDetector(48000)
	require.NoError(t, err)
	require.NotNil(t, detector)
	defer detector.Close()

	// Try to detect (won't find in random data)
	input := make([]int16, 4800)
	for i := range input {
		input[i] = int16(i * 5)
	}
	payload, confidence := detector.Detect(input)
	t.Logf("Watermark payload len: %d, confidence: %.2f", len(payload), confidence)
}
