// Package sonickit provides Go bindings for the SonicKit audio processing library.
//
// SonicKit is a high-performance C library for real-time audio processing,
// including noise reduction, echo cancellation, voice activity detection,
// sample rate conversion, and various audio effects.
//
// # Basic Usage
//
//	import "github.com/aspect-build/sonickit-go"
//
//	// Create a denoiser
//	denoiser, err := sonickit.NewDenoiser(16000, 160, sonickit.DenoiserSpeexDSP)
//	if err != nil {
//	    log.Fatal(err)
//	}
//	defer denoiser.Close()
//
//	// Process audio
//	output := denoiser.Process(inputSamples)
//
// # Thread Safety
//
// Individual processor instances are NOT thread-safe. Use separate instances
// for different goroutines, or implement your own synchronization.
package sonickit

/*
#cgo CFLAGS: -I${SRCDIR}/../../../include
#cgo LDFLAGS: -L${SRCDIR}/../../../build -lsonickit
#cgo linux LDFLAGS: -lm -lpthread
#cgo darwin LDFLAGS: -lm -lpthread
#cgo windows LDFLAGS: -lwinmm

#include <stdlib.h>
#include "voice/voice_version.h"
*/
import "C"
import (
	"fmt"
	"runtime"
)

// Version returns the SonicKit library version string.
func Version() string {
	return C.GoString(C.voice_get_version())
}

// VersionInfo contains detailed version information.
type VersionInfo struct {
	Major int
	Minor int
	Patch int
}

// GetVersionInfo returns detailed version information.
func GetVersionInfo() VersionInfo {
	var major, minor, patch C.int
	C.voice_get_version_numbers(&major, &minor, &patch)
	return VersionInfo{
		Major: int(major),
		Minor: int(minor),
		Patch: int(patch),
	}
}

// String returns the version as a formatted string.
func (v VersionInfo) String() string {
	return fmt.Sprintf("%d.%d.%d", v.Major, v.Minor, v.Patch)
}

// keepAlive prevents the GC from collecting an object while native code is using it.
func keepAlive(obj interface{}) {
	runtime.KeepAlive(obj)
}
