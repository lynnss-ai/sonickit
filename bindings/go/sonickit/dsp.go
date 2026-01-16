package sonickit

/*
#include <stdlib.h>
#include "dsp/voice_denoise.h"
#include "dsp/voice_aec.h"
#include "dsp/voice_agc.h"
#include "dsp/voice_vad.h"
#include "dsp/voice_resampler.h"
#include "dsp/voice_dtmf.h"
#include "dsp/voice_equalizer.h"
#include "dsp/voice_compressor.h"
#include "dsp/voice_comfort_noise.h"
*/
import "C"
import (
	"errors"
	"runtime"
	"unsafe"
)

// DenoiserEngine specifies the noise reduction algorithm.
type DenoiserEngine int

const (
	// DenoiserSpeexDSP uses the SpeexDSP noise reduction algorithm.
	DenoiserSpeexDSP DenoiserEngine = 0
	// DenoiserRNNoise uses the RNNoise neural network-based algorithm.
	DenoiserRNNoise DenoiserEngine = 1
)

// Denoiser performs noise reduction on audio samples.
type Denoiser struct {
	handle    unsafe.Pointer
	frameSize int
}

// NewDenoiser creates a new noise reduction processor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz (8000, 16000, 32000, 48000)
//   - frameSize: Number of samples per frame
//   - engine: Noise reduction algorithm to use
func NewDenoiser(sampleRate, frameSize int, engine DenoiserEngine) (*Denoiser, error) {
	handle := C.voice_denoise_create(C.int(sampleRate), C.int(frameSize), C.int(engine))
	if handle == nil {
		return nil, errors.New("failed to create denoiser")
	}
	d := &Denoiser{handle: handle, frameSize: frameSize}
	runtime.SetFinalizer(d, (*Denoiser).Close)
	return d, nil
}

// Process applies noise reduction to the input samples.
func (d *Denoiser) Process(input []int16) []int16 {
	if d.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_denoise_process(d.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// SetLevel sets the noise reduction level (0-100).
func (d *Denoiser) SetLevel(level int) {
	if d.handle != nil {
		C.voice_denoise_set_level(d.handle, C.int(level))
	}
}

// Close releases the denoiser resources.
func (d *Denoiser) Close() error {
	if d.handle != nil {
		C.voice_denoise_destroy(d.handle)
		d.handle = nil
		runtime.SetFinalizer(d, nil)
	}
	return nil
}

// EchoCanceller performs acoustic echo cancellation.
type EchoCanceller struct {
	handle    unsafe.Pointer
	frameSize int
}

// NewEchoCanceller creates a new echo cancellation processor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - frameSize: Number of samples per frame
//   - filterLength: Echo tail length in samples
func NewEchoCanceller(sampleRate, frameSize, filterLength int) (*EchoCanceller, error) {
	handle := C.voice_aec_create(C.int(sampleRate), C.int(frameSize), C.int(filterLength))
	if handle == nil {
		return nil, errors.New("failed to create echo canceller")
	}
	e := &EchoCanceller{handle: handle, frameSize: frameSize}
	runtime.SetFinalizer(e, (*EchoCanceller).Close)
	return e, nil
}

// Process applies echo cancellation.
//
// Parameters:
//   - captured: Microphone input with echo
//   - playback: Reference signal being played to speaker
//
// Returns the echo-cancelled audio.
func (e *EchoCanceller) Process(captured, playback []int16) []int16 {
	if e.handle == nil || len(captured) == 0 {
		return nil
	}
	output := make([]int16, len(captured))
	C.voice_aec_process(e.handle,
		(*C.short)(unsafe.Pointer(&captured[0])),
		(*C.short)(unsafe.Pointer(&playback[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(captured)))
	return output
}

// Close releases the echo canceller resources.
func (e *EchoCanceller) Close() error {
	if e.handle != nil {
		C.voice_aec_destroy(e.handle)
		e.handle = nil
		runtime.SetFinalizer(e, nil)
	}
	return nil
}

// AgcMode specifies the automatic gain control mode.
type AgcMode int

const (
	// AgcAdaptive uses adaptive AGC mode.
	AgcAdaptive AgcMode = 0
	// AgcFixed uses fixed gain AGC mode.
	AgcFixed AgcMode = 1
)

// Agc performs automatic gain control.
type Agc struct {
	handle    unsafe.Pointer
	frameSize int
}

// NewAgc creates a new automatic gain control processor.
func NewAgc(sampleRate, frameSize int, mode AgcMode, targetLevel int) (*Agc, error) {
	handle := C.voice_agc_create(C.int(sampleRate), C.int(frameSize), C.int(mode), C.int(targetLevel))
	if handle == nil {
		return nil, errors.New("failed to create AGC")
	}
	a := &Agc{handle: handle, frameSize: frameSize}
	runtime.SetFinalizer(a, (*Agc).Close)
	return a, nil
}

// Process applies automatic gain control.
func (a *Agc) Process(input []int16) []int16 {
	if a.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_agc_process(a.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// GetGain returns the current gain in dB.
func (a *Agc) GetGain() float32 {
	if a.handle == nil {
		return 0
	}
	return float32(C.voice_agc_get_gain(a.handle))
}

// Close releases the AGC resources.
func (a *Agc) Close() error {
	if a.handle != nil {
		C.voice_agc_destroy(a.handle)
		a.handle = nil
		runtime.SetFinalizer(a, nil)
	}
	return nil
}

// VadMode specifies the voice activity detection sensitivity.
type VadMode int

const (
	// VadQuality is optimized for voice quality.
	VadQuality VadMode = 0
	// VadLowBitrate is optimized for low bitrate.
	VadLowBitrate VadMode = 1
	// VadAggressive uses aggressive detection.
	VadAggressive VadMode = 2
	// VadVeryAggressive uses very aggressive detection.
	VadVeryAggressive VadMode = 3
)

// Vad performs voice activity detection.
type Vad struct {
	handle    unsafe.Pointer
	frameSize int
}

// NewVad creates a new voice activity detector.
func NewVad(sampleRate int, mode VadMode) (*Vad, error) {
	handle := C.voice_vad_create(C.int(sampleRate), C.int(mode))
	if handle == nil {
		return nil, errors.New("failed to create VAD")
	}
	v := &Vad{handle: handle}
	runtime.SetFinalizer(v, (*Vad).Close)
	return v, nil
}

// IsSpeech detects if the audio frame contains speech.
func (v *Vad) IsSpeech(input []int16) bool {
	if v.handle == nil || len(input) == 0 {
		return false
	}
	result := C.voice_vad_process(v.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		C.int(len(input)))
	return result != 0
}

// GetProbability returns the speech probability (0.0-1.0).
func (v *Vad) GetProbability() float32 {
	if v.handle == nil {
		return 0
	}
	return float32(C.voice_vad_get_probability(v.handle))
}

// Close releases the VAD resources.
func (v *Vad) Close() error {
	if v.handle != nil {
		C.voice_vad_destroy(v.handle)
		v.handle = nil
		runtime.SetFinalizer(v, nil)
	}
	return nil
}

// Resampler performs sample rate conversion.
type Resampler struct {
	handle   unsafe.Pointer
	channels int
	inRate   int
	outRate  int
}

// NewResampler creates a new sample rate converter.
//
// Parameters:
//   - channels: Number of audio channels
//   - inRate: Input sample rate in Hz
//   - outRate: Output sample rate in Hz
//   - quality: Resampling quality (0-10, higher is better)
func NewResampler(channels, inRate, outRate, quality int) (*Resampler, error) {
	handle := C.voice_resampler_create(C.int(channels), C.int(inRate), C.int(outRate), C.int(quality))
	if handle == nil {
		return nil, errors.New("failed to create resampler")
	}
	r := &Resampler{
		handle:   handle,
		channels: channels,
		inRate:   inRate,
		outRate:  outRate,
	}
	runtime.SetFinalizer(r, (*Resampler).Close)
	return r, nil
}

// Process resamples the input audio.
func (r *Resampler) Process(input []int16) []int16 {
	if r.handle == nil || len(input) == 0 {
		return nil
	}
	// Calculate output size based on ratio
	outLen := (len(input) * r.outRate) / r.inRate
	if outLen == 0 {
		outLen = 1
	}
	output := make([]int16, outLen*2) // Extra space for edge cases

	inLen := C.uint(len(input))
	outLenC := C.uint(len(output))
	C.voice_resampler_process(r.handle,
		(*C.short)(unsafe.Pointer(&input[0])), &inLen,
		(*C.short)(unsafe.Pointer(&output[0])), &outLenC)

	return output[:outLenC]
}

// Close releases the resampler resources.
func (r *Resampler) Close() error {
	if r.handle != nil {
		C.voice_resampler_destroy(r.handle)
		r.handle = nil
		runtime.SetFinalizer(r, nil)
	}
	return nil
}

// DtmfDetector detects DTMF tones in audio.
type DtmfDetector struct {
	handle    unsafe.Pointer
	frameSize int
}

// NewDtmfDetector creates a new DTMF tone detector.
func NewDtmfDetector(sampleRate, frameSize int) (*DtmfDetector, error) {
	handle := C.voice_dtmf_detector_create(C.int(sampleRate), C.int(frameSize))
	if handle == nil {
		return nil, errors.New("failed to create DTMF detector")
	}
	d := &DtmfDetector{handle: handle, frameSize: frameSize}
	runtime.SetFinalizer(d, (*DtmfDetector).Close)
	return d, nil
}

// Process detects DTMF tones in the audio frame.
// Returns the detected digit ('0'-'9', 'A'-'D', '*', '#') or 0 if none.
func (d *DtmfDetector) Process(input []int16) byte {
	if d.handle == nil || len(input) == 0 {
		return 0
	}
	return byte(C.voice_dtmf_detector_process(d.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		C.int(len(input))))
}

// Close releases the detector resources.
func (d *DtmfDetector) Close() error {
	if d.handle != nil {
		C.voice_dtmf_detector_destroy(d.handle)
		d.handle = nil
		runtime.SetFinalizer(d, nil)
	}
	return nil
}

// DtmfGenerator generates DTMF tones.
type DtmfGenerator struct {
	handle     unsafe.Pointer
	sampleRate int
}

// NewDtmfGenerator creates a new DTMF tone generator.
func NewDtmfGenerator(sampleRate, toneDurationMs int) (*DtmfGenerator, error) {
	handle := C.voice_dtmf_generator_create(C.int(sampleRate), C.int(toneDurationMs))
	if handle == nil {
		return nil, errors.New("failed to create DTMF generator")
	}
	g := &DtmfGenerator{handle: handle, sampleRate: sampleRate}
	runtime.SetFinalizer(g, (*DtmfGenerator).Close)
	return g, nil
}

// Generate generates a single DTMF digit tone.
func (g *DtmfGenerator) Generate(digit byte) []int16 {
	if g.handle == nil {
		return nil
	}
	var outLen C.int
	ptr := C.voice_dtmf_generator_generate(g.handle, C.char(digit), &outLen)
	if ptr == nil || outLen == 0 {
		return nil
	}
	// Copy data to Go slice
	output := make([]int16, int(outLen))
	copy(output, (*[1 << 30]int16)(unsafe.Pointer(ptr))[:outLen:outLen])
	return output
}

// GenerateSequence generates DTMF tones for a sequence of digits.
func (g *DtmfGenerator) GenerateSequence(digits string) []int16 {
	if g.handle == nil || len(digits) == 0 {
		return nil
	}
	var result []int16
	for i := 0; i < len(digits); i++ {
		tone := g.Generate(digits[i])
		result = append(result, tone...)
	}
	return result
}

// Close releases the generator resources.
func (g *DtmfGenerator) Close() error {
	if g.handle != nil {
		C.voice_dtmf_generator_destroy(g.handle)
		g.handle = nil
		runtime.SetFinalizer(g, nil)
	}
	return nil
}

// Equalizer provides parametric equalization.
type Equalizer struct {
	handle unsafe.Pointer
	bands  int
}

// NewEqualizer creates a new parametric equalizer.
func NewEqualizer(sampleRate, numBands int) (*Equalizer, error) {
	handle := C.voice_equalizer_create(C.int(sampleRate), C.int(numBands))
	if handle == nil {
		return nil, errors.New("failed to create equalizer")
	}
	e := &Equalizer{handle: handle, bands: numBands}
	runtime.SetFinalizer(e, (*Equalizer).Close)
	return e, nil
}

// SetBand configures a specific equalizer band.
//
// Parameters:
//   - band: Band index (0 to numBands-1)
//   - frequency: Center frequency in Hz
//   - gain: Gain in dB
//   - q: Q factor (bandwidth)
func (e *Equalizer) SetBand(band int, frequency, gain, q float32) {
	if e.handle != nil && band >= 0 && band < e.bands {
		C.voice_equalizer_set_band(e.handle, C.int(band),
			C.float(frequency), C.float(gain), C.float(q))
	}
}

// Process applies equalization to the audio.
func (e *Equalizer) Process(input []int16) []int16 {
	if e.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_equalizer_process(e.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the equalizer resources.
func (e *Equalizer) Close() error {
	if e.handle != nil {
		C.voice_equalizer_destroy(e.handle)
		e.handle = nil
		runtime.SetFinalizer(e, nil)
	}
	return nil
}

// Compressor provides dynamic range compression.
type Compressor struct {
	handle unsafe.Pointer
}

// NewCompressor creates a new dynamic range compressor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - threshold: Compression threshold in dB
//   - ratio: Compression ratio (e.g., 4.0 for 4:1)
//   - attackMs: Attack time in milliseconds
//   - releaseMs: Release time in milliseconds
func NewCompressor(sampleRate int, threshold, ratio, attackMs, releaseMs float32) (*Compressor, error) {
	handle := C.voice_compressor_create(C.int(sampleRate),
		C.float(threshold), C.float(ratio),
		C.float(attackMs), C.float(releaseMs))
	if handle == nil {
		return nil, errors.New("failed to create compressor")
	}
	c := &Compressor{handle: handle}
	runtime.SetFinalizer(c, (*Compressor).Close)
	return c, nil
}

// Process applies compression to the audio.
func (c *Compressor) Process(input []int16) []int16 {
	if c.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_compressor_process(c.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// GetGainReduction returns the current gain reduction in dB.
func (c *Compressor) GetGainReduction() float32 {
	if c.handle == nil {
		return 0
	}
	return float32(C.voice_compressor_get_gain_reduction(c.handle))
}

// Close releases the compressor resources.
func (c *Compressor) Close() error {
	if c.handle != nil {
		C.voice_compressor_destroy(c.handle)
		c.handle = nil
		runtime.SetFinalizer(c, nil)
	}
	return nil
}

// ComfortNoiseGenerator generates comfort noise.
type ComfortNoiseGenerator struct {
	handle unsafe.Pointer
}

// NewComfortNoiseGenerator creates a new comfort noise generator.
func NewComfortNoiseGenerator(sampleRate int, level float32) (*ComfortNoiseGenerator, error) {
	handle := C.voice_cng_create(C.int(sampleRate), C.float(level))
	if handle == nil {
		return nil, errors.New("failed to create CNG")
	}
	c := &ComfortNoiseGenerator{handle: handle}
	runtime.SetFinalizer(c, (*ComfortNoiseGenerator).Close)
	return c, nil
}

// Generate generates comfort noise samples.
func (c *ComfortNoiseGenerator) Generate(numSamples int) []int16 {
	if c.handle == nil || numSamples <= 0 {
		return nil
	}
	output := make([]int16, numSamples)
	C.voice_cng_generate(c.handle,
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(numSamples))
	return output
}

// SetLevel sets the noise level in dBFS.
func (c *ComfortNoiseGenerator) SetLevel(level float32) {
	if c.handle != nil {
		C.voice_cng_set_level(c.handle, C.float(level))
	}
}

// Close releases the generator resources.
func (c *ComfortNoiseGenerator) Close() error {
	if c.handle != nil {
		C.voice_cng_destroy(c.handle)
		c.handle = nil
		runtime.SetFinalizer(c, nil)
	}
	return nil
}
