# Voice Library Examples
# CMake build configuration for examples

# Capture example
add_executable(example_capture example_capture.c)
target_link_libraries(example_capture PRIVATE voice)

# Playback example
add_executable(example_playback example_playback.c)
target_link_libraries(example_playback PRIVATE voice)

# File conversion example
add_executable(example_file_convert example_file_convert.c)
target_link_libraries(example_file_convert PRIVATE voice)

# Voice chat example
add_executable(example_voicechat example_voicechat.c)
target_link_libraries(example_voicechat PRIVATE voice)
if(WIN32)
    target_link_libraries(example_voicechat PRIVATE ws2_32)
endif()

# Install examples
install(TARGETS
    example_capture
    example_playback
    example_file_convert
    example_voicechat
    RUNTIME DESTINATION bin/examples
)
