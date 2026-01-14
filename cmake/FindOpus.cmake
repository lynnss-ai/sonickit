# FindOpus.cmake
# Find the Opus codec library
#
# OPUS_FOUND - system has Opus
# OPUS_INCLUDE_DIRS - Opus include directories
# OPUS_LIBRARIES - Opus libraries

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_OPUS QUIET opus)
endif()

find_path(OPUS_INCLUDE_DIR
    NAMES opus/opus.h
    PATHS
        ${PC_OPUS_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        /opt/local/include
)

find_library(OPUS_LIBRARY
    NAMES opus
    PATHS
        ${PC_OPUS_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Opus
    REQUIRED_VARS OPUS_LIBRARY OPUS_INCLUDE_DIR
)

if(Opus_FOUND)
    set(OPUS_LIBRARIES ${OPUS_LIBRARY})
    set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDE_DIR})
    
    if(NOT TARGET Opus::opus)
        add_library(Opus::opus UNKNOWN IMPORTED)
        set_target_properties(Opus::opus PROPERTIES
            IMPORTED_LOCATION "${OPUS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPUS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(OPUS_INCLUDE_DIR OPUS_LIBRARY)
