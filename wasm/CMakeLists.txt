cmake_minimum_required(VERSION 3.16)
project(SonicKitWASM C CXX)

# ============================================
# SonicKit WebAssembly 构建配置
# 作者: wangxuebing <lynnss.codeai@gmail.com>
# ============================================

# ============================================
# Emscripten 检测
# ============================================
if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This CMakeLists.txt requires Emscripten. Use: emcmake cmake")
endif()

message(STATUS "Building SonicKit for WebAssembly")

# ============================================
# 编译选项
# ============================================
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 构建选项
option(WASM_ENABLE_OPUS "Enable Opus codec" ON)
option(WASM_ENABLE_G722 "Enable G.722 codec" OFF)
option(WASM_ENABLE_RNNOISE "Enable RNNoise denoiser" ON)
option(WASM_ENABLE_THREADS "Enable pthreads (experimental)" OFF)
option(WASM_ENABLE_SIMD "Enable WASM SIMD" OFF)

# ============================================
# 路径定义
# ============================================
set(SONICKIT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(SONICKIT_INCLUDE "${SONICKIT_ROOT}/include")
set(SONICKIT_SRC "${SONICKIT_ROOT}/src")
set(SONICKIT_THIRD_PARTY "${SONICKIT_ROOT}/third_party")

# ============================================
# 编译定义
# ============================================
add_compile_definitions(
    SONICKIT_PLATFORM_WASM=1
    __EMSCRIPTEN__=1
    MA_NO_DEVICE_IO=1
    MA_NO_THREADING=1
    SONICKIT_NO_PLATFORM_AUDIO=1
)

# 线程支持
if(WASM_ENABLE_THREADS)
    add_compile_definitions(__EMSCRIPTEN_PTHREADS__=1)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
endif()

# ============================================
# 包含目录
# ============================================
include_directories(
    ${SONICKIT_INCLUDE}
    ${SONICKIT_THIRD_PARTY}/miniaudio
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ============================================
# 核心源文件列表
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
)

# 编解码器模块
set(CODEC_SOURCES
    ${SONICKIT_SRC}/codec/codec.c
    ${SONICKIT_SRC}/codec/codec_g711.c
)

if(WASM_ENABLE_OPUS)
    list(APPEND CODEC_SOURCES ${SONICKIT_SRC}/codec/codec_opus.c)
    add_compile_definitions(SONICKIT_HAVE_OPUS=1)
endif()

if(WASM_ENABLE_G722)
    list(APPEND CODEC_SOURCES ${SONICKIT_SRC}/codec/codec_g722.c)
    add_compile_definitions(SONICKIT_HAVE_G722=1)
endif()

# 音频处理模块
set(AUDIO_SOURCES
    ${SONICKIT_SRC}/audio/audio_buffer.c
    ${SONICKIT_SRC}/audio/audio_mixer.c
    ${SONICKIT_SRC}/audio/audio_quality.c
    ${SONICKIT_SRC}/audio/audio_level.c
    ${SONICKIT_SRC}/audio/file_io.c
)

# 工具模块
set(UTILS_SOURCES
    ${SONICKIT_SRC}/utils/simd_utils.c
)

# WASM 平台适配层
set(WASM_PLATFORM_SOURCES
    platform/wasm_platform.c
    platform/wasm_time.c
)

# WASM 桩实现
set(WASM_STUB_SOURCES
    stubs/device_stub.c
    stubs/network_stub.c
)

# 合并所有源文件
set(SONICKIT_WASM_SOURCES
    ${DSP_SOURCES}
    ${CODEC_SOURCES}
    ${AUDIO_SOURCES}
    ${UTILS_SOURCES}
    ${WASM_PLATFORM_SOURCES}
    ${WASM_STUB_SOURCES}
)

# ============================================
# 第三方库配置
# ============================================

# miniaudio（仅头文件）
add_library(miniaudio_wasm STATIC
    ${SONICKIT_THIRD_PARTY}/miniaudio/miniaudio.c
)
target_include_directories(miniaudio_wasm PUBLIC
    ${SONICKIT_THIRD_PARTY}/miniaudio
)
target_compile_definitions(miniaudio_wasm PRIVATE
    MA_NO_DEVICE_IO=1
    MA_NO_THREADING=1
)

# SpeexDSP（如果存在）
if(EXISTS "${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/resample.c")
    add_library(speexdsp_wasm STATIC
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/resample.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/preprocess.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/mdf.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/filterbank.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/fftwrap.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/kiss_fft.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/kiss_fftr.c
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp/jitter.c
    )
    target_include_directories(speexdsp_wasm PUBLIC
        ${SONICKIT_THIRD_PARTY}/speexdsp/include
        ${SONICKIT_THIRD_PARTY}/speexdsp/libspeexdsp
    )
    target_compile_definitions(speexdsp_wasm PRIVATE
        HAVE_CONFIG_H
        FLOATING_POINT
        USE_KISS_FFT
        EXPORT=
    )
    add_compile_definitions(SONICKIT_HAVE_SPEEXDSP=1)
    set(HAVE_SPEEXDSP TRUE)
else()
    message(WARNING "SpeexDSP not found, some DSP features will be disabled")
    set(HAVE_SPEEXDSP FALSE)
endif()

# RNNoise（如果存在）
if(WASM_ENABLE_RNNOISE AND EXISTS "${SONICKIT_THIRD_PARTY}/rnnoise")
    # RNNoise 需要单独编译或使用预编译版本
    message(STATUS "RNNoise enabled - ensure it's pre-built for WASM")
    add_compile_definitions(SONICKIT_HAVE_RNNOISE=1)
endif()

# ============================================
# 核心库（静态库）
# ============================================
add_library(sonickit_core STATIC ${SONICKIT_WASM_SOURCES})

target_link_libraries(sonickit_core PUBLIC
    miniaudio_wasm
)

if(HAVE_SPEEXDSP)
    target_link_libraries(sonickit_core PUBLIC speexdsp_wasm)
endif()

# ============================================
# JavaScript API（使用 Embind）
# ============================================
add_executable(sonickit api/sonickit_api.cpp)

target_link_libraries(sonickit PRIVATE sonickit_core)

# ============================================
# Emscripten 链接选项
# ============================================

set(EMSCRIPTEN_LINK_FLAGS
    --bind
    -sEXPORTED_FUNCTIONS=['_malloc','_free']
    -sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']
    -sALLOW_MEMORY_GROWTH=1
    -sINITIAL_MEMORY=67108864
    -sSTACK_SIZE=5242880
    -sMODULARIZE=1
    -sEXPORT_NAME='createSonicKit'
    -sENVIRONMENT=web,worker
    -sNO_EXIT_RUNTIME=1
    --no-entry
)

# 线程支持
if(WASM_ENABLE_THREADS)
    list(APPEND EMSCRIPTEN_LINK_FLAGS
        -pthread
        -sPTHREAD_POOL_SIZE=4
        -sALLOW_MEMORY_GROWTH=0
    )
endif()

# SIMD 支持
if(WASM_ENABLE_SIMD)
    list(APPEND EMSCRIPTEN_LINK_FLAGS -msimd128)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msimd128")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msimd128")
endif()

# 优化级别
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    list(APPEND EMSCRIPTEN_LINK_FLAGS -O3 -flto)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    list(APPEND EMSCRIPTEN_LINK_FLAGS -Oz -flto)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Oz")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Oz")
else()
    list(APPEND EMSCRIPTEN_LINK_FLAGS -O2 -g)
endif()

# 应用链接选项
target_link_options(sonickit PRIVATE ${EMSCRIPTEN_LINK_FLAGS})

# ============================================
# 输出配置
# ============================================
set_target_properties(sonickit PROPERTIES
    OUTPUT_NAME "sonickit"
    SUFFIX ".js"
)

# ============================================
# 安装规则
# ============================================
install(TARGETS sonickit
    RUNTIME DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/sonickit.wasm
    DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/dist
)

# ============================================
# 构建信息
# ============================================
message(STATUS "=== SonicKit WASM Configuration ===")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Opus Codec: ${WASM_ENABLE_OPUS}")
message(STATUS "G.722 Codec: ${WASM_ENABLE_G722}")
message(STATUS "RNNoise: ${WASM_ENABLE_RNNOISE}")
message(STATUS "Threads: ${WASM_ENABLE_THREADS}")
message(STATUS "SIMD: ${WASM_ENABLE_SIMD}")
message(STATUS "SpeexDSP: ${HAVE_SPEEXDSP}")
message(STATUS "==================================")
