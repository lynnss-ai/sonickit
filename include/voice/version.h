/**
 * @file version.h
 * @brief SonicKit library version information
 * @author SonicKit Team
 *
 * Provides compile-time and runtime version information for the library.
 * This is useful for:
 * - Checking API compatibility at runtime
 * - Logging library version in applications
 * - Verifying correct library is loaded in FFI scenarios
 */

#ifndef VOICE_VERSION_H
#define VOICE_VERSION_H

#include "export.h"
#include <stdbool.h>

/*
 * Version number components
 *
 * SONICKIT_VERSION follows semantic versioning (https://semver.org/):
 * - MAJOR: Incompatible API changes
 * - MINOR: Backwards-compatible functionality additions
 * - PATCH: Backwards-compatible bug fixes
 */
#define SONICKIT_VERSION_MAJOR 1
#define SONICKIT_VERSION_MINOR 0
#define SONICKIT_VERSION_PATCH 0

/*
 * Version as a single integer for easy comparison
 * Formula: MAJOR * 10000 + MINOR * 100 + PATCH
 * Example: 1.2.3 = 10203
 */
#define SONICKIT_VERSION_NUMBER \
    (SONICKIT_VERSION_MAJOR * 10000 + \
     SONICKIT_VERSION_MINOR * 100 + \
     SONICKIT_VERSION_PATCH)

/*
 * Version as a string
 */
#define SONICKIT_VERSION_STRING "1.0.0"

/*
 * Library name and description
 */
#define SONICKIT_NAME "SonicKit"
#define SONICKIT_DESCRIPTION "High-performance audio processing library"

/*
 * Build information (can be overridden at compile time)
 */
#ifndef SONICKIT_BUILD_DATE
    #define SONICKIT_BUILD_DATE __DATE__
#endif

#ifndef SONICKIT_BUILD_TIME
    #define SONICKIT_BUILD_TIME __TIME__
#endif

VOICE_EXTERN_C_BEGIN

/**
 * @brief Get the library version as a string
 * @details Returns the version in "MAJOR.MINOR.PATCH" format.
 *          This is useful for logging and display purposes.
 * @return Version string, e.g., "1.0.0"
 */
VOICE_API const char* sonickit_version_string(void);

/**
 * @brief Get the major version number
 * @details Major version changes indicate incompatible API changes.
 *          Applications should check this to ensure compatibility.
 * @return Major version number
 */
VOICE_API int sonickit_version_major(void);

/**
 * @brief Get the minor version number
 * @details Minor version changes indicate new features that are
 *          backwards-compatible with previous versions.
 * @return Minor version number
 */
VOICE_API int sonickit_version_minor(void);

/**
 * @brief Get the patch version number
 * @details Patch version changes indicate backwards-compatible bug fixes.
 * @return Patch version number
 */
VOICE_API int sonickit_version_patch(void);

/**
 * @brief Get the version as a single integer for comparison
 * @details Returns MAJOR * 10000 + MINOR * 100 + PATCH.
 *          This makes it easy to compare versions numerically.
 * @return Version number as integer
 *
 * @code
 * if (sonickit_version_number() >= 10200) {
 *     // Use features from version 1.2.0 or later
 * }
 * @endcode
 */
VOICE_API int sonickit_version_number(void);

/**
 * @brief Get the library name
 * @return Library name string "SonicKit"
 */
VOICE_API const char* sonickit_name(void);

/**
 * @brief Get the build date
 * @details Returns the date when the library was compiled.
 * @return Build date string in "MMM DD YYYY" format
 */
VOICE_API const char* sonickit_build_date(void);

/**
 * @brief Check if a specific feature is available
 * @details Some features may be conditionally compiled based on
 *          available dependencies (e.g., Opus, RNNoise).
 * @param feature Feature name to check
 * @return true if feature is available, false otherwise
 *
 * Supported feature names:
 * - "opus" - Opus codec support
 * - "g722" - G.722 codec support
 * - "rnnoise" - RNNoise denoiser
 * - "speexdsp" - SpeexDSP processing
 * - "simd" - SIMD optimizations (SSE/AVX/NEON)
 */
VOICE_API bool sonickit_has_feature(const char* feature);

/**
 * @brief Get a comma-separated list of available features
 * @details Returns all features compiled into the library.
 * @return Feature list string, e.g., "opus,rnnoise,simd"
 */
VOICE_API const char* sonickit_features(void);

VOICE_EXTERN_C_END

#endif /* VOICE_VERSION_H */
