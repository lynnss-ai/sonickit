/**
 * @file export.h
 * @brief SonicKit library export/import macros for multi-platform support
 * @author SonicKit Team
 *
 * This header provides the VOICE_API macro for controlling symbol visibility
 * when building SonicKit as a shared library. It supports:
 * - Windows: __declspec(dllexport/dllimport)
 * - GCC/Clang: __attribute__((visibility("default")))
 * - Static library builds: No special attributes
 *
 * Usage:
 * - When building shared library: Define SONICKIT_BUILDING_DLL
 * - When using static library: Define SONICKIT_STATIC
 * - When using shared library as client: Neither defined
 */

#ifndef VOICE_EXPORT_H
#define VOICE_EXPORT_H

/*
 * VOICE_API - Symbol visibility macro
 *
 * Controls whether functions are exported from or imported into the library.
 * This is essential for creating shared libraries (.dll/.so/.dylib) that can
 * be used by other programming languages through FFI (Foreign Function Interface).
 */
#if defined(SONICKIT_STATIC)
    /* Static library - no special attributes needed */
    #define VOICE_API
#elif defined(_WIN32) || defined(__CYGWIN__)
    /* Windows platform */
    #ifdef SONICKIT_BUILDING_DLL
        /* Building the DLL - export symbols */
        #define VOICE_API __declspec(dllexport)
    #else
        /* Using the DLL - import symbols */
        #define VOICE_API __declspec(dllimport)
    #endif
#elif defined(__GNUC__) && __GNUC__ >= 4
    /* GCC/Clang with visibility support */
    #define VOICE_API __attribute__((visibility("default")))
#else
    /* Unknown compiler - no special attributes */
    #define VOICE_API
#endif

/*
 * VOICE_LOCAL - Internal symbol macro
 *
 * Marks functions as internal to the library, hidden from external linkage.
 * Use this for helper functions that should not be part of the public API.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
    #define VOICE_LOCAL
#elif defined(__GNUC__) && __GNUC__ >= 4
    #define VOICE_LOCAL __attribute__((visibility("hidden")))
#else
    #define VOICE_LOCAL
#endif

/*
 * C++ compatibility macros
 *
 * Ensures C linkage when headers are included from C++ code,
 * which is essential for FFI compatibility with other languages.
 */
#ifdef __cplusplus
    #define VOICE_EXTERN_C extern "C"
    #define VOICE_EXTERN_C_BEGIN extern "C" {
    #define VOICE_EXTERN_C_END }
#else
    #define VOICE_EXTERN_C
    #define VOICE_EXTERN_C_BEGIN
    #define VOICE_EXTERN_C_END
#endif

/*
 * Calling convention macro
 *
 * Ensures consistent calling convention across platforms.
 * CDECL is the default for C and is widely supported by FFI systems.
 */
#if defined(_WIN32) && !defined(__GNUC__)
    #define VOICE_CALL __cdecl
#else
    #define VOICE_CALL
#endif

/*
 * Deprecated function macro
 *
 * Marks functions as deprecated to warn users about API changes.
 */
#if defined(__GNUC__) || defined(__clang__)
    #define VOICE_DEPRECATED(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
    #define VOICE_DEPRECATED(msg) __declspec(deprecated(msg))
#else
    #define VOICE_DEPRECATED(msg)
#endif

/*
 * Inline hint macro
 *
 * Provides a portable way to suggest function inlining.
 */
#if defined(_MSC_VER)
    #define VOICE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define VOICE_INLINE inline __attribute__((always_inline))
#else
    #define VOICE_INLINE inline
#endif

#endif /* VOICE_EXPORT_H */
