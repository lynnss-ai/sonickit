/**
 * @file version.c
 * @brief SonicKit library version information implementation
 */

#include "voice/version.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

const char* sonickit_version_string(void) {
    return SONICKIT_VERSION_STRING;
}

int sonickit_version_major(void) {
    return SONICKIT_VERSION_MAJOR;
}

int sonickit_version_minor(void) {
    return SONICKIT_VERSION_MINOR;
}

int sonickit_version_patch(void) {
    return SONICKIT_VERSION_PATCH;
}

int sonickit_version_number(void) {
    return SONICKIT_VERSION_NUMBER;
}

const char* sonickit_name(void) {
    return SONICKIT_NAME;
}

const char* sonickit_build_date(void) {
    return SONICKIT_BUILD_DATE;
}

bool sonickit_has_feature(const char* feature) {
    if (!feature) return false;

#ifdef SONICKIT_HAVE_OPUS
    if (strcmp(feature, "opus") == 0) return true;
#endif

#ifdef VOICE_HAVE_G722
    if (strcmp(feature, "g722") == 0) return true;
#endif

#ifdef SONICKIT_HAVE_RNNOISE
    if (strcmp(feature, "rnnoise") == 0) return true;
#endif

#ifdef SONICKIT_HAVE_SPEEXDSP
    if (strcmp(feature, "speexdsp") == 0) return true;
#endif

#if defined(__SSE2__) || defined(__AVX__) || defined(__ARM_NEON)
    if (strcmp(feature, "simd") == 0) return true;
#endif

    return false;
}

const char* sonickit_features(void) {
    static char features[256] = {0};
    static bool initialized = false;

    if (!initialized) {
        char* p = features;

#ifdef SONICKIT_HAVE_OPUS
        p += sprintf(p, "opus,");
#endif

#ifdef VOICE_HAVE_G722
        p += sprintf(p, "g722,");
#endif

#ifdef SONICKIT_HAVE_RNNOISE
        p += sprintf(p, "rnnoise,");
#endif

#ifdef SONICKIT_HAVE_SPEEXDSP
        p += sprintf(p, "speexdsp,");
#endif

#if defined(__SSE2__) || defined(__AVX__)
        p += sprintf(p, "sse,");
#endif

#if defined(__AVX__)
        p += sprintf(p, "avx,");
#endif

#if defined(__ARM_NEON)
        p += sprintf(p, "neon,");
#endif

        /* Remove trailing comma */
        if (p > features && *(p-1) == ',') {
            *(p-1) = '\0';
        }

        initialized = true;
    }

    return features;
}
