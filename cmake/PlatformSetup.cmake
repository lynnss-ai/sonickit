# PlatformSetup.cmake
# Platform-specific configuration

# 检测编译器特性
include(CheckCCompilerFlag)
include(CheckIncludeFile)

# 通用警告选项
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wno-unused-parameter)
    
    # 优化选项
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3 -ffast-math)
        
        # SIMD 优化
        check_c_compiler_flag(-msse2 HAS_SSE2)
        check_c_compiler_flag(-mavx HAS_AVX)
        check_c_compiler_flag(-mavx2 HAS_AVX2)
        
        if(HAS_SSE2)
            add_compile_options(-msse2)
        endif()
    endif()
elseif(MSVC)
    add_compile_options(/W3)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    
    # SIMD 优化
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(/O2 /fp:fast)
    endif()
endif()

# 平台特定检查
check_include_file(pthread.h HAVE_PTHREAD)
check_include_file(sys/socket.h HAVE_SYS_SOCKET)
check_include_file(winsock2.h HAVE_WINSOCK2)

# 配置头文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/voice/voice_config_gen.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/voice/voice_config_gen.h
)
