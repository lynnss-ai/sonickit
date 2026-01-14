cmake_minimum_required(VERSION 3.16)
project(SonicKit C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================
# 编译选项
# ============================================
option(SONICKIT_BUILD_EXAMPLES "Build example programs" ON)
option(SONICKIT_BUILD_TESTS "Build tests" ON)
option(SONICKIT_ENABLE_RNNOISE "Enable RNNoise denoiser" ON)
option(SONICKIT_ENABLE_OPUS "Enable Opus codec" ON)
option(SONICKIT_ENABLE_SPEEX "Enable Speex codec" OFF)
option(SONICKIT_ENABLE_G722 "Enable G.722 codec" OFF)
option(SONICKIT_ENABLE_SRTP "Enable SRTP encryption" ON)
option(SONICKIT_ENABLE_DTLS "Enable DTLS-SRTP key exchange" ON)

# ============================================
# 平台检测
# ============================================
if(WIN32)
    set(SONICKIT_PLATFORM "windows")
    add_compile_definitions(SONICKIT_PLATFORM_WINDOWS)
    set(PLATFORM_LIBS ws2_32 winmm ole32 iphlpapi)
elseif(APPLE)
    if(IOS)
        set(SONICKIT_PLATFORM "ios")
        add_compile_definitions(SONICKIT_PLATFORM_IOS)
    else()
        set(SONICKIT_PLATFORM "macos")
        add_compile_definitions(SONICKIT_PLATFORM_MACOS)
    endif()
    find_library(COREAUDIO_LIB CoreAudio)
    find_library(AUDIOTOOLBOX_LIB AudioToolbox)
    find_library(COREFOUNDATION_LIB CoreFoundation)
    find_library(AVFOUNDATION_LIB AVFoundation)
    set(PLATFORM_LIBS 
        ${COREAUDIO_LIB} 
        ${AUDIOTOOLBOX_LIB} 
        ${COREFOUNDATION_LIB}
        ${AVFOUNDATION_LIB}
    )
elseif(ANDROID)
    set(SONICKIT_PLATFORM "android")
    add_compile_definitions(SONICKIT_PLATFORM_ANDROID)
    set(PLATFORM_LIBS aaudio log)
elseif(UNIX)
    set(SONICKIT_PLATFORM "linux")
    add_compile_definitions(SONICKIT_PLATFORM_LINUX)
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS Threads::Threads m dl asound)
endif()

message(STATUS "Building for platform: ${SONICKIT_PLATFORM}")

# ============================================
# 第三方库
# ============================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# miniaudio (单头文件，直接包含)
add_library(miniaudio STATIC
    third_party/miniaudio/miniaudio.c
)
target_include_directories(miniaudio PUBLIC third_party/miniaudio)
if(APPLE AND IOS)
    set_source_files_properties(third_party/miniaudio/miniaudio.c 
        PROPERTIES COMPILE_FLAGS "-x objective-c")
endif()

# SpeexDSP (可选)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/speexdsp/libspeexdsp/resample.c")
    add_library(speexdsp STATIC
        third_party/speexdsp/libspeexdsp/resample.c
        third_party/speexdsp/libspeexdsp/preprocess.c
        third_party/speexdsp/libspeexdsp/mdf.c
        third_party/speexdsp/libspeexdsp/filterbank.c
        third_party/speexdsp/libspeexdsp/fftwrap.c
        third_party/speexdsp/libspeexdsp/kiss_fft.c
        third_party/speexdsp/libspeexdsp/kiss_fftr.c
        third_party/speexdsp/libspeexdsp/jitter.c
    )
    target_include_directories(speexdsp PUBLIC 
        third_party/speexdsp/include
        third_party/speexdsp/libspeexdsp
    )
    target_compile_definitions(speexdsp PRIVATE 
        HAVE_CONFIG_H
        FLOATING_POINT
        USE_KISS_FFT
        EXPORT=
    )
    add_compile_definitions(SONICKIT_HAVE_SPEEXDSP)
    set(SONICKIT_HAVE_SPEEXDSP ON)
else()
    message(STATUS "SpeexDSP source not found, using stub implementation")
    set(SONICKIT_HAVE_SPEEXDSP OFF)
endif()

# RNNoise (可选)
if(SONICKIT_ENABLE_RNNOISE)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rnnoise/src/denoise.c")
        add_library(rnnoise STATIC
            third_party/rnnoise/src/denoise.c
            third_party/rnnoise/src/rnn.c
            third_party/rnnoise/src/rnn_data.c
            third_party/rnnoise/src/pitch.c
            third_party/rnnoise/src/kiss_fft.c
            third_party/rnnoise/src/celt_lpc.c
        )
        target_include_directories(rnnoise PUBLIC third_party/rnnoise/include)
        target_compile_definitions(rnnoise PRIVATE COMPILE_OPUS)
        add_compile_definitions(SONICKIT_HAVE_RNNOISE)
    else()
        message(STATUS "RNNoise source not found, disabling")
        set(SONICKIT_ENABLE_RNNOISE OFF)
    endif()
endif()

# Opus (可选)
if(SONICKIT_ENABLE_OPUS)
    find_package(Opus)
    if(Opus_FOUND)
        add_compile_definitions(SONICKIT_HAVE_OPUS)
    else()
        message(STATUS "Opus not found, building from source...")
        # 如果系统没有安装，从源码编译
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus/CMakeLists.txt")
            add_subdirectory(third_party/opus)
            add_compile_definitions(SONICKIT_HAVE_OPUS)
        else()
            message(WARNING "Opus source not found, Opus codec disabled")
            set(SONICKIT_ENABLE_OPUS OFF)
        endif()
    endif()
endif()

# libsrtp2 (可选)
if(SONICKIT_ENABLE_SRTP)
    find_package(SRTP2)
    if(SRTP2_FOUND)
        add_compile_definitions(SONICKIT_HAVE_SRTP)
    else()
        message(STATUS "libsrtp2 not found, building from source...")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/srtp/CMakeLists.txt")
            set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
            add_subdirectory(third_party/srtp)
            add_compile_definitions(SONICKIT_HAVE_SRTP)
        else()
            message(WARNING "libsrtp2 source not found, SRTP disabled")
            set(SONICKIT_ENABLE_SRTP OFF)
        endif()
    endif()
endif()

# OpenSSL (DTLS)
if(SONICKIT_ENABLE_DTLS)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        add_compile_definitions(SONICKIT_HAVE_DTLS)
    else()
        message(WARNING "OpenSSL not found, DTLS-SRTP disabled")
        set(SONICKIT_ENABLE_DTLS OFF)
    endif()
endif()

# ============================================
# 主库源文件
# ============================================
set(SONICKIT_SOURCES
    # 核心
    src/voice/SONICKIT_init.c
    src/voice/pipeline.c
    src/voice/statistics.c
    
    # 音频
    src/audio/audio_buffer.c
    src/audio/device.c
    src/audio/file_io.c
    src/audio/audio_mixer.c
    src/audio/audio_quality.c
    src/audio/audio_level.c
    src/audio/audio_recorder.c
    
    # DSP
    src/dsp/resampler.c
    src/dsp/denoiser.c
    src/dsp/echo_canceller.c
    src/dsp/vad.c
    src/dsp/agc.c
    src/dsp/comfort_noise.c
    src/dsp/dtmf.c
    src/dsp/equalizer.c
    src/dsp/compressor.c
    src/dsp/effects.c
    src/dsp/watermark.c
    
    # 编解码器
    src/codec/codec.c
    src/codec/codec_g711.c
    
    # 网络
    src/network/rtp.c
    src/network/srtp.c
    src/network/jitter_buffer.c
    src/network/bandwidth_estimator.c
    src/network/ice.c
    src/network/transport.c
    src/network/datachannel.c
    
    # SIP 协议
    src/sip/sip_core.c
    src/sip/sip_ua.c
    
    # 平台通用
    src/platform/platform_common.c
    
    # 工具
    src/utils/simd_utils.c
    src/utils/diagnostics.c
)

# 平台特定源文件
if(APPLE)
    if(IOS)
        list(APPEND SONICKIT_SOURCES src/platform/platform_ios.m)
    endif()
elseif(ANDROID)
    list(APPEND SONICKIT_SOURCES src/platform/platform_android.c)
endif()

# 可选编解码器
if(SONICKIT_ENABLE_OPUS)
    list(APPEND SONICKIT_SOURCES src/codec/codec_opus.c)
endif()

if(SONICKIT_ENABLE_G722)
    list(APPEND SONICKIT_SOURCES src/codec/codec_g722.c)
endif()

# SRTP 已包含在主源文件列表中

# ============================================
# 主库目标
# ============================================
add_library(voice STATIC ${SONICKIT_SOURCES})

target_include_directories(voice PUBLIC 
    include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(voice PUBLIC
    miniaudio
    ${PLATFORM_LIBS}
)

# 链接 SpeexDSP（如果可用）
if(SONICKIT_HAVE_SPEEXDSP)
    target_link_libraries(voice PUBLIC speexdsp)
endif()

# 链接可选库
if(SONICKIT_ENABLE_RNNOISE)
    target_link_libraries(voice PUBLIC rnnoise)
endif()

if(SONICKIT_ENABLE_OPUS)
    if(TARGET Opus::opus)
        target_link_libraries(voice PUBLIC Opus::opus)
    elseif(TARGET opus)
        target_link_libraries(voice PUBLIC opus)
    endif()
endif()

if(SONICKIT_ENABLE_SRTP)
    if(SRTP2_FOUND)
        target_link_libraries(voice PUBLIC ${SRTP2_LIBRARIES})
        target_include_directories(voice PUBLIC ${SRTP2_INCLUDE_DIRS})
    elseif(TARGET srtp2)
        target_link_libraries(voice PUBLIC srtp2)
    endif()
endif()

if(SONICKIT_ENABLE_DTLS)
    target_link_libraries(voice PUBLIC OpenSSL::SSL OpenSSL::Crypto)
endif()

# ============================================
# 示例程序
# ============================================
if(SONICKIT_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# ============================================
# 测试
# ============================================
if(SONICKIT_BUILD_TESTS)
    enable_testing()
    
    # 测试公共头文件目录
    set(TEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    
    # 音频缓冲区测试
    add_executable(test_buffer tests/test_buffer.c)
    target_include_directories(test_buffer PRIVATE ${TEST_INCLUDE_DIR})
    target_link_libraries(test_buffer voice)
    add_test(NAME test_buffer COMMAND test_buffer)
    
    # 重采样器测试
    add_executable(test_resampler tests/test_resampler.c)
    target_include_directories(test_resampler PRIVATE ${TEST_INCLUDE_DIR})
    target_link_libraries(test_resampler voice)
    add_test(NAME test_resampler COMMAND test_resampler)
    
    # 编解码器测试
    add_executable(test_codec tests/test_codec.c)
    target_include_directories(test_codec PRIVATE ${TEST_INCLUDE_DIR})
    target_link_libraries(test_codec voice)
    add_test(NAME test_codec COMMAND test_codec)
    
    # DSP模块测试
    add_executable(test_dsp tests/test_dsp.c)
    target_include_directories(test_dsp PRIVATE ${TEST_INCLUDE_DIR})
    target_link_libraries(test_dsp voice)
    add_test(NAME test_dsp COMMAND test_dsp)
    
    # 网络模块测试
    add_executable(test_network tests/test_network.c)
    target_include_directories(test_network PRIVATE ${TEST_INCLUDE_DIR})
    target_link_libraries(test_network voice)
    add_test(NAME test_network COMMAND test_network)
endif()

# ============================================
# 安装
# ============================================
install(TARGETS voice
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY include/voice DESTINATION include)
install(DIRECTORY include/audio DESTINATION include)
install(DIRECTORY include/dsp DESTINATION include)
install(DIRECTORY include/codec DESTINATION include)
install(DIRECTORY include/network DESTINATION include)
install(DIRECTORY include/sip DESTINATION include)
install(DIRECTORY include/utils DESTINATION include)

message(STATUS "")
message(STATUS "SonicKit Configuration:")
message(STATUS "  Platform:     ${SONICKIT_PLATFORM}")
message(STATUS "  RNNoise:      ${SONICKIT_ENABLE_RNNOISE}")
message(STATUS "  Opus:         ${SONICKIT_ENABLE_OPUS}")
message(STATUS "  Speex:        ${SONICKIT_ENABLE_SPEEX}")
message(STATUS "  G.722:        ${SONICKIT_ENABLE_G722}")
message(STATUS "  SRTP:         ${SONICKIT_ENABLE_SRTP}")
message(STATUS "  DTLS:         ${SONICKIT_ENABLE_DTLS}")
message(STATUS "  Examples:     ${SONICKIT_BUILD_EXAMPLES}")
message(STATUS "  Tests:        ${SONICKIT_BUILD_TESTS}")
message(STATUS "")
