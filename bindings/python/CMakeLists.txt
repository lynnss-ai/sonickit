cmake_minimum_required(VERSION 3.16)
project(sonickit_python LANGUAGES C CXX)

# ============================================
# SonicKit Python Bindings (pybind11)
# ============================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ============================================
# 查找依赖
# ============================================

# 查找 Python
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
message(STATUS "Python3 found: ${Python3_EXECUTABLE}")
message(STATUS "Python3 include: ${Python3_INCLUDE_DIRS}")

# 查找或下载 pybind11
find_package(pybind11 QUIET)
if(NOT pybind11_FOUND)
    message(STATUS "pybind11 not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)
endif()

# ============================================
# 路径定义
# ============================================
set(SONICKIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
set(SONICKIT_INCLUDE "${SONICKIT_ROOT}/include")
set(SONICKIT_SRC "${SONICKIT_ROOT}/src")
set(SONICKIT_THIRD_PARTY "${SONICKIT_ROOT}/third_party")

# ============================================
# 包含目录
# ============================================
include_directories(
    ${SONICKIT_INCLUDE}
    ${SONICKIT_THIRD_PARTY}/miniaudio
)

# ============================================
# 源文件列表
# ============================================

# DSP 模块
set(DSP_SOURCES
    ${SONICKIT_SRC}/dsp/resampler.c
    ${SONICKIT_SRC}/dsp/denoiser.c
    ${SONICKIT_SRC}/dsp/echo_canceller.c
    ${SONICKIT_SRC}/dsp/agc.c
    ${SONICKIT_SRC}/dsp/vad.c
    ${SONICKIT_SRC}/dsp/equalizer.c
    ${SONICKIT_SRC}/dsp/compressor.c
    ${SONICKIT_SRC}/dsp/dtmf.c
    ${SONICKIT_SRC}/dsp/comfort_noise.c
    ${SONICKIT_SRC}/dsp/effects.c
    ${SONICKIT_SRC}/dsp/watermark.c
    ${SONICKIT_SRC}/dsp/spatial_audio.c
    ${SONICKIT_SRC}/dsp/hrtf.c
    ${SONICKIT_SRC}/dsp/time_stretcher.c
    ${SONICKIT_SRC}/dsp/delay_estimator.c
)

# 编解码器模块
set(CODEC_SOURCES
    ${SONICKIT_SRC}/codec/codec.c
    ${SONICKIT_SRC}/codec/codec_g711.c
)

# 音频处理模块
set(AUDIO_SOURCES
    ${SONICKIT_SRC}/audio/audio_buffer.c
    ${SONICKIT_SRC}/audio/audio_mixer.c
    ${SONICKIT_SRC}/audio/audio_quality.c
    ${SONICKIT_SRC}/audio/audio_level.c
    ${SONICKIT_SRC}/audio/file_io.c
    ${SONICKIT_SRC}/audio/jitter_buffer.c
)

# 工具模块
set(UTILS_SOURCES
    ${SONICKIT_SRC}/utils/simd_utils.c
)

# 合并所有源文件
set(SONICKIT_SOURCES
    ${DSP_SOURCES}
    ${CODEC_SOURCES}
    ${AUDIO_SOURCES}
    ${UTILS_SOURCES}
)

# ============================================
# 编译定义
# ============================================
add_compile_definitions(
    SONICKIT_BUILDING_DLL=1
)

if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# ============================================
# pybind11 模块
# ============================================
pybind11_add_module(_sonickit
    src/bindings.cpp
    src/dsp_bindings.cpp
    src/codec_bindings.cpp
    src/audio_bindings.cpp
    src/effects_bindings.cpp
    ${SONICKIT_SOURCES}
)

target_include_directories(_sonickit PRIVATE
    ${SONICKIT_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================
# 安装规则
# ============================================
install(TARGETS _sonickit
    LIBRARY DESTINATION sonickit
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/sonickit/
    DESTINATION sonickit
    FILES_MATCHING PATTERN "*.py"
)

# ============================================
# 输出信息
# ============================================
message(STATUS "=== SonicKit Python Bindings ===")
message(STATUS "Python: ${Python3_VERSION}")
message(STATUS "pybind11: ${pybind11_VERSION}")
message(STATUS "================================")
