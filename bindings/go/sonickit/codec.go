package sonickit

/*
#include <stdlib.h>
#include "codec/voice_g711.h"
*/
import "C"
import (
	"errors"
	"runtime"
	"unsafe"
)

// G711Codec provides G.711 A-law/μ-law encoding and decoding.
type G711Codec struct {
	handle unsafe.Pointer
	isAlaw bool
}

// NewG711Codec creates a new G.711 codec.
//
// Parameters:
//   - useAlaw: true for A-law, false for μ-law
func NewG711Codec(useAlaw bool) (*G711Codec, error) {
	var alawInt C.int
	if useAlaw {
		alawInt = 1
	}
	handle := C.voice_g711_create(alawInt)
	if handle == nil {
		return nil, errors.New("failed to create G.711 codec")
	}
	c := &G711Codec{handle: handle, isAlaw: useAlaw}
	runtime.SetFinalizer(c, (*G711Codec).Close)
	return c, nil
}

// IsAlaw returns true if using A-law encoding, false for μ-law.
func (c *G711Codec) IsAlaw() bool {
	return c.isAlaw
}

// Encode encodes 16-bit PCM samples to G.711.
func (c *G711Codec) Encode(input []int16) []byte {
	if c.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]byte, len(input))
	C.voice_g711_encode(c.handle,
		(*C.short)(unsafe.Pointer(&input[0])),
		(*C.uchar)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Decode decodes G.711 data to 16-bit PCM samples.
func (c *G711Codec) Decode(input []byte) []int16 {
	if c.handle == nil || len(input) == 0 {
		return nil
	}
	output := make([]int16, len(input))
	C.voice_g711_decode(c.handle,
		(*C.uchar)(unsafe.Pointer(&input[0])),
		(*C.short)(unsafe.Pointer(&output[0])),
		C.int(len(input)))
	return output
}

// Close releases the codec resources.
func (c *G711Codec) Close() error {
	if c.handle != nil {
		C.voice_g711_destroy(c.handle)
		c.handle = nil
		runtime.SetFinalizer(c, nil)
	}
	return nil
}
