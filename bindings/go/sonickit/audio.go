package sonickit

/*
#include <stdlib.h>
#include "audio/voice_buffer.h"
#include "audio/voice_level.h"
#include "audio/voice_mixer.h"
#include "audio/voice_jitter.h"
#include "audio/voice_spatial.h"
#include "audio/voice_hrtf.h"
*/
import "C"
import (
	"errors"
	"runtime"
	"unsafe"
)

// AudioBuffer provides a ring buffer for audio samples.
type AudioBuffer struct {
	handle unsafe.Pointer
}

// NewAudioBuffer creates a new audio ring buffer.
//
// Parameters:
//   - capacity: Maximum number of samples the buffer can hold
func NewAudioBuffer(capacity int) (*AudioBuffer, error) {
	handle := C.voice_buffer_create(C.int(capacity))
	if handle == nil {
		return nil, errors.New("failed to create audio buffer")
	}
	b := &AudioBuffer{handle: handle}
	runtime.SetFinalizer(b, (*AudioBuffer).Close)
	return b, nil
}

// Write writes samples to the buffer.
// Returns the number of samples actually written.
func (b *AudioBuffer) Write(samples []int16) int {
	if b.handle == nil || len(samples) == 0 {
		return 0
	}
	return int(C.voice_buffer_write(b.handle,
		(*C.short)(unsafe.Pointer(&samples[0])),
		C.int(len(samples))))
}

// Read reads samples from the buffer.
// Returns the actual samples read.
func (b *AudioBuffer) Read(numSamples int) []int16 {
	if b.handle == nil || numSamples <= 0 {
		return nil
	}
	output := make([]int16, numSamples)
	read := C.voice_buffer_read(b.handle,
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(numSamples))
	return output[:read]
}

// Available returns the number of samples available for reading.
func (b *AudioBuffer) Available() int {
	if b.handle == nil {
		return 0
	}
	return int(C.voice_buffer_available(b.handle))
}

// Space returns the number of samples that can be written.
func (b *AudioBuffer) Space() int {
	if b.handle == nil {
		return 0
	}
	return int(C.voice_buffer_space(b.handle))
}

// Clear clears all samples from the buffer.
func (b *AudioBuffer) Clear() {
	if b.handle != nil {
		C.voice_buffer_clear(b.handle)
	}
}

// Close releases the buffer resources.
func (b *AudioBuffer) Close() error {
	if b.handle != nil {
		C.voice_buffer_destroy(b.handle)
		b.handle = nil
		runtime.SetFinalizer(b, nil)
	}
	return nil
}

// AudioLevel provides audio level metering.
type AudioLevel struct {
	handle unsafe.Pointer
}

// NewAudioLevel creates a new audio level meter.
func NewAudioLevel(sampleRate, windowMs int) (*AudioLevel, error) {
	handle := C.voice_level_create(C.int(sampleRate), C.int(windowMs))
	if handle == nil {
		return nil, errors.New("failed to create audio level meter")
	}
	l := &AudioLevel{handle: handle}
	runtime.SetFinalizer(l, (*AudioLevel).Close)
	return l, nil
}

// Process processes audio and updates level measurement.
func (l *AudioLevel) Process(input []int16) {
	if l.handle == nil || len(input) == 0 {
		return
	}
	C.voice_level_process(l.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		C.int(len(input)))
}

// GetRMS returns the current RMS level in dBFS.
func (l *AudioLevel) GetRMS() float32 {
	if l.handle == nil {
		return -100
	}
	return float32(C.voice_level_get_rms(l.handle))
}

// GetPeak returns the current peak level in dBFS.
func (l *AudioLevel) GetPeak() float32 {
	if l.handle == nil {
		return -100
	}
	return float32(C.voice_level_get_peak(l.handle))
}

// Close releases the level meter resources.
func (l *AudioLevel) Close() error {
	if l.handle != nil {
		C.voice_level_destroy(l.handle)
		l.handle = nil
		runtime.SetFinalizer(l, nil)
	}
	return nil
}

// AudioMixer provides multi-channel audio mixing.
type AudioMixer struct {
	handle   unsafe.Pointer
	channels int
}

// NewAudioMixer creates a new audio mixer.
//
// Parameters:
//   - channels: Maximum number of input channels
//   - frameSize: Samples per frame
func NewAudioMixer(channels, frameSize int) (*AudioMixer, error) {
	handle := C.voice_mixer_create(C.int(channels), C.int(frameSize))
	if handle == nil {
		return nil, errors.New("failed to create audio mixer")
	}
	m := &AudioMixer{handle: handle, channels: channels}
	runtime.SetFinalizer(m, (*AudioMixer).Close)
	return m, nil
}

// SetChannelGain sets the gain for a specific channel.
func (m *AudioMixer) SetChannelGain(channel int, gain float32) {
	if m.handle != nil && channel >= 0 && channel < m.channels {
		C.voice_mixer_set_gain(m.handle, C.int(channel), C.float(gain))
	}
}

// AddChannel adds audio from a channel to the mix.
func (m *AudioMixer) AddChannel(channel int, input []int16) {
	if m.handle == nil || channel < 0 || channel >= m.channels || len(input) == 0 {
		return
	}
	C.voice_mixer_add(m.handle, C.int(channel),
		(*C.short)(unsafe.Pointer(&input[0])),
		C.int(len(input)))
}

// Mix returns the mixed output and clears internal buffers.
func (m *AudioMixer) Mix(frameSize int) []int16 {
	if m.handle == nil || frameSize <= 0 {
		return nil
	}
	output := make([]int16, frameSize)
	C.voice_mixer_mix(m.handle,
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(frameSize))
	return output
}

// Close releases the mixer resources.
func (m *AudioMixer) Close() error {
	if m.handle != nil {
		C.voice_mixer_destroy(m.handle)
		m.handle = nil
		runtime.SetFinalizer(m, nil)
	}
	return nil
}

// JitterBuffer provides network jitter compensation.
type JitterBuffer struct {
	handle unsafe.Pointer
}

// NewJitterBuffer creates a new jitter buffer.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - frameSizeMs: Frame duration in milliseconds
//   - minDelayMs: Minimum buffering delay in milliseconds
//   - maxDelayMs: Maximum buffering delay in milliseconds
func NewJitterBuffer(sampleRate, frameSizeMs, minDelayMs, maxDelayMs int) (*JitterBuffer, error) {
	handle := C.voice_jitter_create(C.int(sampleRate), C.int(frameSizeMs),
		C.int(minDelayMs), C.int(maxDelayMs))
	if handle == nil {
		return nil, errors.New("failed to create jitter buffer")
	}
	j := &JitterBuffer{handle: handle}
	runtime.SetFinalizer(j, (*JitterBuffer).Close)
	return j, nil
}

// Put adds a packet to the jitter buffer.
//
// Parameters:
//   - data: Audio samples
//   - timestamp: RTP timestamp
//   - sequence: RTP sequence number
func (j *JitterBuffer) Put(data []int16, timestamp uint32, sequence uint16) {
	if j.handle == nil || len(data) == 0 {
		return
	}
	C.voice_jitter_put(j.handle,
		(*C.short)(unsafe.Pointer(&data[0])),
		C.int(len(data)),
		C.uint(timestamp),
		C.ushort(sequence))
}

// Get retrieves audio for playback.
func (j *JitterBuffer) Get(numSamples int) []int16 {
	if j.handle == nil || numSamples <= 0 {
		return nil
	}
	output := make([]int16, numSamples)
	C.voice_jitter_get(j.handle,
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(numSamples))
	return output
}

// GetDelay returns the current buffer delay in milliseconds.
func (j *JitterBuffer) GetDelay() int {
	if j.handle == nil {
		return 0
	}
	return int(C.voice_jitter_get_delay(j.handle))
}

// Close releases the jitter buffer resources.
func (j *JitterBuffer) Close() error {
	if j.handle != nil {
		C.voice_jitter_destroy(j.handle)
		j.handle = nil
		runtime.SetFinalizer(j, nil)
	}
	return nil
}

// SpatialRenderer provides 3D spatial audio rendering.
type SpatialRenderer struct {
	handle unsafe.Pointer
}

// NewSpatialRenderer creates a new spatial audio renderer.
func NewSpatialRenderer(sampleRate, frameSize int) (*SpatialRenderer, error) {
	handle := C.voice_spatial_create(C.int(sampleRate), C.int(frameSize))
	if handle == nil {
		return nil, errors.New("failed to create spatial renderer")
	}
	s := &SpatialRenderer{handle: handle}
	runtime.SetFinalizer(s, (*SpatialRenderer).Close)
	return s, nil
}

// SetSourcePosition sets the audio source position in 3D space.
func (s *SpatialRenderer) SetSourcePosition(x, y, z float32) {
	if s.handle != nil {
		C.voice_spatial_set_source(s.handle, C.float(x), C.float(y), C.float(z))
	}
}

// SetListenerPosition sets the listener position in 3D space.
func (s *SpatialRenderer) SetListenerPosition(x, y, z float32) {
	if s.handle != nil {
		C.voice_spatial_set_listener(s.handle, C.float(x), C.float(y), C.float(z))
	}
}

// Process renders mono input to stereo output with spatial positioning.
func (s *SpatialRenderer) Process(input []int16) []int16 {
	if s.handle == nil || len(input) == 0 {
		return nil
	}
	// Stereo output is 2x the input length
	output := make([]int16, len(input)*2)
	C.voice_spatial_process(s.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the spatial renderer resources.
func (s *SpatialRenderer) Close() error {
	if s.handle != nil {
		C.voice_spatial_destroy(s.handle)
		s.handle = nil
		runtime.SetFinalizer(s, nil)
	}
	return nil
}

// Hrtf provides head-related transfer function processing.
type Hrtf struct {
	handle unsafe.Pointer
}

// NewHrtf creates a new HRTF processor.
func NewHrtf(sampleRate int) (*Hrtf, error) {
	handle := C.voice_hrtf_create(C.int(sampleRate))
	if handle == nil {
		return nil, errors.New("failed to create HRTF processor")
	}
	h := &Hrtf{handle: handle}
	runtime.SetFinalizer(h, (*Hrtf).Close)
	return h, nil
}

// SetAzimuth sets the horizontal angle in degrees (-180 to 180).
func (h *Hrtf) SetAzimuth(azimuth float32) {
	if h.handle != nil {
		C.voice_hrtf_set_azimuth(h.handle, C.float(azimuth))
	}
}

// SetElevation sets the vertical angle in degrees (-90 to 90).
func (h *Hrtf) SetElevation(elevation float32) {
	if h.handle != nil {
		C.voice_hrtf_set_elevation(h.handle, C.float(elevation))
	}
}

// Process renders mono input to binaural stereo output.
func (h *Hrtf) Process(input []int16) []int16 {
	if h.handle == nil || len(input) == 0 {
		return nil
	}
	// Stereo output is 2x the input length
	output := make([]int16, len(input)*2)
	C.voice_hrtf_process(h.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the HRTF processor resources.
func (h *Hrtf) Close() error {
	if h.handle != nil {
		C.voice_hrtf_destroy(h.handle)
		h.handle = nil
		runtime.SetFinalizer(h, nil)
	}
	return nil
}
