# FindSRTP2.cmake
# Find the libsrtp2 library
#
# SRTP2_FOUND - system has libsrtp2
# SRTP2_INCLUDE_DIRS - libsrtp2 include directories
# SRTP2_LIBRARIES - libsrtp2 libraries

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_SRTP2 QUIET libsrtp2)
endif()

find_path(SRTP2_INCLUDE_DIR
    NAMES srtp2/srtp.h
    PATHS
        ${PC_SRTP2_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        /opt/local/include
)

find_library(SRTP2_LIBRARY
    NAMES srtp2 libsrtp2
    PATHS
        ${PC_SRTP2_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /opt/local/lib
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SRTP2
    REQUIRED_VARS SRTP2_LIBRARY SRTP2_INCLUDE_DIR
)

if(SRTP2_FOUND)
    set(SRTP2_LIBRARIES ${SRTP2_LIBRARY})
    set(SRTP2_INCLUDE_DIRS ${SRTP2_INCLUDE_DIR})
    
    if(NOT TARGET SRTP2::srtp2)
        add_library(SRTP2::srtp2 UNKNOWN IMPORTED)
        set_target_properties(SRTP2::srtp2 PROPERTIES
            IMPORTED_LOCATION "${SRTP2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SRTP2_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(SRTP2_INCLUDE_DIR SRTP2_LIBRARY)
