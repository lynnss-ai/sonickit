package sonickit

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestVersion(t *testing.T) {
	version := Version()
	assert.NotEmpty(t, version)
	t.Logf("SonicKit version: %s", version)
}

func TestVersionInfo(t *testing.T) {
	info := GetVersionInfo()
	assert.GreaterOrEqual(t, info.Major, 0)
	assert.GreaterOrEqual(t, info.Minor, 0)
	assert.GreaterOrEqual(t, info.Patch, 0)
	t.Logf("SonicKit version: %s", info.String())
}

func TestDenoiser(t *testing.T) {
	denoiser, err := NewDenoiser(16000, 160, DenoiserSpeexDSP)
	require.NoError(t, err)
	require.NotNil(t, denoiser)
	defer denoiser.Close()

	// Process audio
	input := make([]int16, 160)
	for i := range input {
		input[i] = int16(i * 100)
	}
	output := denoiser.Process(input)
	assert.Len(t, output, len(input))

	// Set level
	denoiser.SetLevel(50)
}

func TestEchoCanceller(t *testing.T) {
	aec, err := NewEchoCanceller(16000, 160, 2000)
	require.NoError(t, err)
	require.NotNil(t, aec)
	defer aec.Close()

	captured := make([]int16, 160)
	playback := make([]int16, 160)
	for i := range captured {
		captured[i] = int16(i * 50)
		playback[i] = int16(i * 30)
	}
	output := aec.Process(captured, playback)
	assert.Len(t, output, len(captured))
}

func TestAgc(t *testing.T) {
	agc, err := NewAgc(16000, 160, AgcAdaptive, -3)
	require.NoError(t, err)
	require.NotNil(t, agc)
	defer agc.Close()

	input := make([]int16, 160)
	for i := range input {
		input[i] = int16(i * 20)
	}
	output := agc.Process(input)
	assert.Len(t, output, len(input))

	gain := agc.GetGain()
	t.Logf("AGC gain: %.2f dB", gain)
}

func TestVad(t *testing.T) {
	vad, err := NewVad(16000, VadLowBitrate)
	require.NoError(t, err)
	require.NotNil(t, vad)
	defer vad.Close()

	// Silence should not be detected as speech
	silence := make([]int16, 160)
	isSpeech := vad.IsSpeech(silence)
	assert.False(t, isSpeech)

	prob := vad.GetProbability()
	assert.GreaterOrEqual(t, prob, float32(0))
	assert.LessOrEqual(t, prob, float32(1))
}

func TestResampler(t *testing.T) {
	resampler, err := NewResampler(1, 16000, 48000, 5)
	require.NoError(t, err)
	require.NotNil(t, resampler)
	defer resampler.Close()

	input := make([]int16, 160)
	for i := range input {
		input[i] = int16(i * 100)
	}
	output := resampler.Process(input)
	// Output should be ~3x input (48000/16000)
	assert.Greater(t, len(output), 0)
}

func TestDtmfGenerator(t *testing.T) {
	generator, err := NewDtmfGenerator(8000, 100)
	require.NoError(t, err)
	require.NotNil(t, generator)
	defer generator.Close()

	// Generate single digit
	tone := generator.Generate('5')
	assert.Greater(t, len(tone), 0)

	// Generate sequence
	sequence := generator.GenerateSequence("123")
	assert.Greater(t, len(sequence), len(tone))
}

func TestDtmfDetector(t *testing.T) {
	detector, err := NewDtmfDetector(8000, 160)
	require.NoError(t, err)
	require.NotNil(t, detector)
	defer detector.Close()

	// Process silence
	silence := make([]int16, 160)
	digit := detector.Process(silence)
	assert.Equal(t, byte(0), digit)
}

func TestEqualizer(t *testing.T) {
	eq, err := NewEqualizer(48000, 5)
	require.NoError(t, err)
	require.NotNil(t, eq)
	defer eq.Close()

	// Set bands
	eq.SetBand(0, 100, 3.0, 1.0)
	eq.SetBand(1, 1000, -2.0, 1.0)
	eq.SetBand(2, 5000, 1.0, 1.0)

	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 10)
	}
	output := eq.Process(input)
	assert.Len(t, output, len(input))
}

func TestCompressor(t *testing.T) {
	comp, err := NewCompressor(48000, -20, 4.0, 10, 100)
	require.NoError(t, err)
	require.NotNil(t, comp)
	defer comp.Close()

	input := make([]int16, 480)
	for i := range input {
		input[i] = int16(i * 50)
	}
	output := comp.Process(input)
	assert.Len(t, output, len(input))

	gr := comp.GetGainReduction()
	t.Logf("Gain reduction: %.2f dB", gr)
}

func TestComfortNoiseGenerator(t *testing.T) {
	cng, err := NewComfortNoiseGenerator(16000, -40)
	require.NoError(t, err)
	require.NotNil(t, cng)
	defer cng.Close()

	noise := cng.Generate(160)
	assert.Len(t, noise, 160)

	// Set level
	cng.SetLevel(-50)
}
