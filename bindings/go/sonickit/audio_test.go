package sonickit

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestAudioBuffer(t *testing.T) {
	buffer, err := NewAudioBuffer(1024)
	require.NoError(t, err)
	require.NotNil(t, buffer)
	defer buffer.Close()

	// Initially empty
	assert.Equal(t, 0, buffer.Available())
	assert.Equal(t, 1024, buffer.Space())

	// Write samples
	input := make([]int16, 256)
	for i := range input {
		input[i] = int16(i)
	}
	written := buffer.Write(input)
	assert.Equal(t, 256, written)
	assert.Equal(t, 256, buffer.Available())

	// Read samples
	output := buffer.Read(128)
	assert.Len(t, output, 128)
	assert.Equal(t, 128, buffer.Available())

	// Clear
	buffer.Clear()
	assert.Equal(t, 0, buffer.Available())
}

func TestAudioLevel(t *testing.T) {
	level, err := NewAudioLevel(16000, 20)
	require.NoError(t, err)
	require.NotNil(t, level)
	defer level.Close()

	// Process silence
	silence := make([]int16, 320)
	level.Process(silence)

	rms := level.GetRMS()
	peak := level.GetPeak()
	t.Logf("RMS: %.2f dBFS, Peak: %.2f dBFS", rms, peak)

	// Process signal
	signal := make([]int16, 320)
	for i := range signal {
		signal[i] = int16(10000)
	}
	level.Process(signal)
	rms = level.GetRMS()
	assert.Greater(t, rms, float32(-100))
}

func TestAudioMixer(t *testing.T) {
	mixer, err := NewAudioMixer(4, 160)
	require.NoError(t, err)
	require.NotNil(t, mixer)
	defer mixer.Close()

	// Set gains
	mixer.SetChannelGain(0, 1.0)
	mixer.SetChannelGain(1, 0.5)

	// Add channels
	ch0 := make([]int16, 160)
	ch1 := make([]int16, 160)
	for i := range ch0 {
		ch0[i] = 1000
		ch1[i] = 2000
	}
	mixer.AddChannel(0, ch0)
	mixer.AddChannel(1, ch1)

	// Mix
	output := mixer.Mix(160)
	assert.Len(t, output, 160)
}

func TestJitterBuffer(t *testing.T) {
	jitter, err := NewJitterBuffer(16000, 20, 40, 200)
	require.NoError(t, err)
	require.NotNil(t, jitter)
	defer jitter.Close()

	// Add packets
	packet := make([]int16, 320)
	for i := range packet {
		packet[i] = int16(i * 10)
	}
	jitter.Put(packet, 0, 0)
	jitter.Put(packet, 320, 1)

	// Get playback
	output := jitter.Get(320)
	assert.Len(t, output, 320)

	delay := jitter.GetDelay()
	t.Logf("Jitter buffer delay: %d ms", delay)
}

func TestSpatialRenderer(t *testing.T) {
	spatial, err := NewSpatialRenderer(48000, 480)
	require.NoError(t, err)
	require.NotNil(t, spatial)
	defer spatial.Close()

	// Set positions
	spatial.SetSourcePosition(2.0, 0.0, 1.0)
	spatial.SetListenerPosition(0.0, 0.0, 0.0)

	// Process mono to stereo
	mono := make([]int16, 480)
	for i := range mono {
		mono[i] = int16(i * 20)
	}
	stereo := spatial.Process(mono)
	assert.Len(t, stereo, 960) // 2x for stereo
}

func TestHrtf(t *testing.T) {
	hrtf, err := NewHrtf(48000)
	require.NoError(t, err)
	require.NotNil(t, hrtf)
	defer hrtf.Close()

	// Set direction
	hrtf.SetAzimuth(45)
	hrtf.SetElevation(0)

	// Process mono to binaural stereo
	mono := make([]int16, 480)
	for i := range mono {
		mono[i] = int16(i * 20)
	}
	stereo := hrtf.Process(mono)
	assert.Len(t, stereo, 960) // 2x for stereo
}

func TestG711Codec(t *testing.T) {
	// Test A-law
	alaw, err := NewG711Codec(true)
	require.NoError(t, err)
	require.NotNil(t, alaw)
	defer alaw.Close()

	assert.True(t, alaw.IsAlaw())

	// Encode/decode
	input := make([]int16, 160)
	for i := range input {
		input[i] = int16(i * 100)
	}
	encoded := alaw.Encode(input)
	assert.Len(t, encoded, len(input))

	decoded := alaw.Decode(encoded)
	assert.Len(t, decoded, len(encoded))

	// Test μ-law
	ulaw, err := NewG711Codec(false)
	require.NoError(t, err)
	require.NotNil(t, ulaw)
	defer ulaw.Close()

	assert.False(t, ulaw.IsAlaw())
}
