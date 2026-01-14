/**
 * @file miniaudio.c
 * @brief miniaudio implementation wrapper
 * 
 * This file implements the miniaudio single-header library.
 * miniaudio is included as a header-only library, but CMake requires
 * a .c source file for the static library target.
 */

/* Define implementation before including */
#define MINIAUDIO_IMPLEMENTATION

/* Disable unused backends to reduce compile time */
#if defined(_WIN32)
    /* Windows: Use WASAPI */
    #define MA_ENABLE_WASAPI
    #define MA_NO_JACK
    #define MA_NO_PULSEAUDIO
    #define MA_NO_ALSA
    #define MA_NO_COREAUDIO
    #define MA_NO_AAUDIO
    #define MA_NO_OPENSL
#elif defined(__APPLE__)
    /* macOS/iOS: Use Core Audio */
    #define MA_ENABLE_COREAUDIO
    #define MA_NO_JACK
    #define MA_NO_PULSEAUDIO
    #define MA_NO_ALSA
    #define MA_NO_WASAPI
    #define MA_NO_AAUDIO
    #define MA_NO_OPENSL
#elif defined(__ANDROID__)
    /* Android: Use AAudio */
    #define MA_ENABLE_AAUDIO
    #define MA_NO_JACK
    #define MA_NO_PULSEAUDIO
    #define MA_NO_ALSA
    #define MA_NO_WASAPI
    #define MA_NO_COREAUDIO
#else
    /* Linux: Use ALSA/PulseAudio */
    #define MA_ENABLE_ALSA
    #define MA_ENABLE_PULSEAUDIO
    #define MA_NO_JACK
    #define MA_NO_WASAPI
    #define MA_NO_COREAUDIO
    #define MA_NO_AAUDIO
    #define MA_NO_OPENSL
#endif

/* Disable unused decoders */
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_VORBIS

/* Enable thread safety */
#define MA_NO_RUNTIME_LINKING

#include "miniaudio.h"
