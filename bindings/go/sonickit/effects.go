package sonickit

/*
#include <stdlib.h>
#include "dsp/voice_effects.h"
#include "dsp/voice_reverb.h"
#include "dsp/voice_delay.h"
#include "dsp/voice_pitch.h"
#include "dsp/voice_chorus.h"
#include "dsp/voice_flanger.h"
#include "dsp/voice_time_stretch.h"
#include "dsp/voice_watermark.h"
*/
import "C"
import (
	"errors"
	"runtime"
	"unsafe"
)

// Reverb provides room reverb effect processing.
type Reverb struct {
	handle unsafe.Pointer
}

// NewReverb creates a new reverb effect processor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - roomSize: Room size factor (0.0-1.0)
//   - wetLevel: Wet/dry mix level (0.0-1.0)
func NewReverb(sampleRate int, roomSize, wetLevel float32) (*Reverb, error) {
	handle := C.voice_reverb_create(C.int(sampleRate), C.float(roomSize), C.float(wetLevel))
	if handle == nil {
		return nil, errors.New("failed to create reverb")
	}
	r := &Reverb{handle: handle}
	runtime.SetFinalizer(r, (*Reverb).Close)
	return r, nil
}

// SetRoomSize sets the room size.
func (r *Reverb) SetRoomSize(size float32) {
	if r.handle != nil {
		C.voice_reverb_set_room_size(r.handle, C.float(size))
	}
}

// SetWetLevel sets the wet/dry mix level.
func (r *Reverb) SetWetLevel(level float32) {
	if r.handle != nil {
		C.voice_reverb_set_wet_level(r.handle, C.float(level))
	}
}

// Process applies reverb to the audio.
func (r *Reverb) Process(input []int16) []int16 {
	if r.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_reverb_process(r.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the reverb resources.
func (r *Reverb) Close() error {
	if r.handle != nil {
		C.voice_reverb_destroy(r.handle)
		r.handle = nil
		runtime.SetFinalizer(r, nil)
	}
	return nil
}

// Delay provides echo/delay effect processing.
type Delay struct {
	handle unsafe.Pointer
}

// NewDelay creates a new delay effect processor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - delayMs: Delay time in milliseconds
//   - feedback: Feedback amount (0.0-1.0)
func NewDelay(sampleRate int, delayMs, feedback float32) (*Delay, error) {
	handle := C.voice_delay_create(C.int(sampleRate), C.float(delayMs), C.float(feedback))
	if handle == nil {
		return nil, errors.New("failed to create delay")
	}
	d := &Delay{handle: handle}
	runtime.SetFinalizer(d, (*Delay).Close)
	return d, nil
}

// SetDelayTime sets the delay time in milliseconds.
func (d *Delay) SetDelayTime(ms float32) {
	if d.handle != nil {
		C.voice_delay_set_time(d.handle, C.float(ms))
	}
}

// SetFeedback sets the feedback amount.
func (d *Delay) SetFeedback(feedback float32) {
	if d.handle != nil {
		C.voice_delay_set_feedback(d.handle, C.float(feedback))
	}
}

// Process applies delay to the audio.
func (d *Delay) Process(input []int16) []int16 {
	if d.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_delay_process(d.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the delay resources.
func (d *Delay) Close() error {
	if d.handle != nil {
		C.voice_delay_destroy(d.handle)
		d.handle = nil
		runtime.SetFinalizer(d, nil)
	}
	return nil
}

// PitchShifter provides pitch shifting effect.
type PitchShifter struct {
	handle unsafe.Pointer
}

// NewPitchShifter creates a new pitch shifter.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - semitones: Pitch shift amount in semitones
func NewPitchShifter(sampleRate int, semitones float32) (*PitchShifter, error) {
	handle := C.voice_pitch_create(C.int(sampleRate), C.float(semitones))
	if handle == nil {
		return nil, errors.New("failed to create pitch shifter")
	}
	p := &PitchShifter{handle: handle}
	runtime.SetFinalizer(p, (*PitchShifter).Close)
	return p, nil
}

// SetPitch sets the pitch shift amount in semitones.
func (p *PitchShifter) SetPitch(semitones float32) {
	if p.handle != nil {
		C.voice_pitch_set_shift(p.handle, C.float(semitones))
	}
}

// Process applies pitch shifting to the audio.
func (p *PitchShifter) Process(input []int16) []int16 {
	if p.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_pitch_process(p.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the pitch shifter resources.
func (p *PitchShifter) Close() error {
	if p.handle != nil {
		C.voice_pitch_destroy(p.handle)
		p.handle = nil
		runtime.SetFinalizer(p, nil)
	}
	return nil
}

// Chorus provides chorus effect processing.
type Chorus struct {
	handle unsafe.Pointer
}

// NewChorus creates a new chorus effect processor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - depth: Modulation depth (0.0-1.0)
//   - rate: Modulation rate in Hz
func NewChorus(sampleRate int, depth, rate float32) (*Chorus, error) {
	handle := C.voice_chorus_create(C.int(sampleRate), C.float(depth), C.float(rate))
	if handle == nil {
		return nil, errors.New("failed to create chorus")
	}
	c := &Chorus{handle: handle}
	runtime.SetFinalizer(c, (*Chorus).Close)
	return c, nil
}

// SetDepth sets the modulation depth.
func (c *Chorus) SetDepth(depth float32) {
	if c.handle != nil {
		C.voice_chorus_set_depth(c.handle, C.float(depth))
	}
}

// SetRate sets the modulation rate.
func (c *Chorus) SetRate(rate float32) {
	if c.handle != nil {
		C.voice_chorus_set_rate(c.handle, C.float(rate))
	}
}

// Process applies chorus to the audio.
func (c *Chorus) Process(input []int16) []int16 {
	if c.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_chorus_process(c.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the chorus resources.
func (c *Chorus) Close() error {
	if c.handle != nil {
		C.voice_chorus_destroy(c.handle)
		c.handle = nil
		runtime.SetFinalizer(c, nil)
	}
	return nil
}

// Flanger provides flanger effect processing.
type Flanger struct {
	handle unsafe.Pointer
}

// NewFlanger creates a new flanger effect processor.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - depth: Modulation depth (0.0-1.0)
//   - rate: Modulation rate in Hz
func NewFlanger(sampleRate int, depth, rate float32) (*Flanger, error) {
	handle := C.voice_flanger_create(C.int(sampleRate), C.float(depth), C.float(rate))
	if handle == nil {
		return nil, errors.New("failed to create flanger")
	}
	f := &Flanger{handle: handle}
	runtime.SetFinalizer(f, (*Flanger).Close)
	return f, nil
}

// SetDepth sets the modulation depth.
func (f *Flanger) SetDepth(depth float32) {
	if f.handle != nil {
		C.voice_flanger_set_depth(f.handle, C.float(depth))
	}
}

// SetRate sets the modulation rate.
func (f *Flanger) SetRate(rate float32) {
	if f.handle != nil {
		C.voice_flanger_set_rate(f.handle, C.float(rate))
	}
}

// Process applies flanger to the audio.
func (f *Flanger) Process(input []int16) []int16 {
	if f.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_flanger_process(f.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the flanger resources.
func (f *Flanger) Close() error {
	if f.handle != nil {
		C.voice_flanger_destroy(f.handle)
		f.handle = nil
		runtime.SetFinalizer(f, nil)
	}
	return nil
}

// TimeStretcher provides time stretching without pitch change.
type TimeStretcher struct {
	handle unsafe.Pointer
}

// NewTimeStretcher creates a new time stretcher.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - ratio: Time stretch ratio (1.0 = no change, 2.0 = half speed)
func NewTimeStretcher(sampleRate int, ratio float32) (*TimeStretcher, error) {
	handle := C.voice_time_stretch_create(C.int(sampleRate), C.float(ratio))
	if handle == nil {
		return nil, errors.New("failed to create time stretcher")
	}
	t := &TimeStretcher{handle: handle}
	runtime.SetFinalizer(t, (*TimeStretcher).Close)
	return t, nil
}

// SetRatio sets the time stretch ratio.
func (t *TimeStretcher) SetRatio(ratio float32) {
	if t.handle != nil {
		C.voice_time_stretch_set_ratio(t.handle, C.float(ratio))
	}
}

// Process applies time stretching to the audio.
func (t *TimeStretcher) Process(input []int16) []int16 {
	if t.handle == nil || len(input) == 0 {
		return nil
	}
	// Output size depends on stretch ratio
	outputLen := int(float32(len(input)) * 2) // Max possible size
	output := make([]int16, outputLen)
	var actualLen C.int
	C.voice_time_stretch_process(t.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		C.int(len(input)),
		(*C.short)(unsafe.Pointer(&output[0])),
		&actualLen)
	return output[:actualLen]
}

// Close releases the time stretcher resources.
func (t *TimeStretcher) Close() error {
	if t.handle != nil {
		C.voice_time_stretch_destroy(t.handle)
		t.handle = nil
		runtime.SetFinalizer(t, nil)
	}
	return nil
}

// WatermarkEmbedder embeds audio watermarks.
type WatermarkEmbedder struct {
	handle unsafe.Pointer
}

// NewWatermarkEmbedder creates a new watermark embedder.
//
// Parameters:
//   - sampleRate: Audio sample rate in Hz
//   - strength: Watermark strength (0.0-1.0)
func NewWatermarkEmbedder(sampleRate int, strength float32) (*WatermarkEmbedder, error) {
	handle := C.voice_watermark_embedder_create(C.int(sampleRate), C.float(strength))
	if handle == nil {
		return nil, errors.New("failed to create watermark embedder")
	}
	w := &WatermarkEmbedder{handle: handle}
	runtime.SetFinalizer(w, (*WatermarkEmbedder).Close)
	return w, nil
}

// Embed embeds a watermark payload into the audio.
func (w *WatermarkEmbedder) Embed(input []int16, payload []byte) []int16 {
	if w.handle == nil || len(input) == 0 || len(payload) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_watermark_embed(w.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)),
		(*C.uchar)(unsafe.Pointer(&payload[0])),
		C.int(len(payload)))
	return output
}

// Close releases the embedder resources.
func (w *WatermarkEmbedder) Close() error {
	if w.handle != nil {
		C.voice_watermark_embedder_destroy(w.handle)
		w.handle = nil
		runtime.SetFinalizer(w, nil)
	}
	return nil
}

// WatermarkDetector detects audio watermarks.
type WatermarkDetector struct {
	handle unsafe.Pointer
}

// NewWatermarkDetector creates a new watermark detector.
func NewWatermarkDetector(sampleRate int) (*WatermarkDetector, error) {
	handle := C.voice_watermark_detector_create(C.int(sampleRate))
	if handle == nil {
		return nil, errors.New("failed to create watermark detector")
	}
	d := &WatermarkDetector{handle: handle}
	runtime.SetFinalizer(d, (*WatermarkDetector).Close)
	return d, nil
}

// Detect attempts to detect and extract a watermark from the audio.
// Returns the extracted payload and a confidence score (0.0-1.0).
func (d *WatermarkDetector) Detect(input []int16) ([]byte, float32) {
	if d.handle == nil || len(input) == 0 {
		return nil, 0
	}
	payload := make([]byte, 256) // Max payload size
	var payloadLen C.int
	var confidence C.float
	C.voice_watermark_detect(d.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		C.int(len(input)),
		(*C.uchar)(unsafe.Pointer(&payload[0])),
		&payloadLen,
		&confidence)
	if payloadLen == 0 {
		return nil, 0
	}
	return payload[:payloadLen], float32(confidence)
}

// Close releases the detector resources.
func (d *WatermarkDetector) Close() error {
	if d.handle != nil {
		C.voice_watermark_detector_destroy(d.handle)
		d.handle = nil
		runtime.SetFinalizer(d, nil)
	}
	return nil
}
