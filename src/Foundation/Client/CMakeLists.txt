rr_find_sdl3()

add_executable(rusty_client
    src/main.cpp
)

target_link_libraries(rusty_client
    PRIVATE
        rr_core
        rr_platform
        rr_render
        SDL3::SDL3
)

target_compile_definitions(rusty_client
    PRIVATE
        RUSTYRAGE_VERSION="${PROJECT_VERSION}"
        RUSTYRAGE_FOUNDATION_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/foundation"
)

if(ANDROID)
    # SDL Android packaging expects the application shared library to be named
    # libmain.so while keeping the logical CMake target identity rusty_client.
    set_target_properties(rusty_client PROPERTIES OUTPUT_NAME main)
endif()

add_custom_command(TARGET rusty_client POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders/foundation"
        "${CMAKE_BINARY_DIR}/shaders/foundation"
    COMMENT "Copying foundation test shaders"
)

rr_apply_compiler_options(rusty_client)
rr_register_first_party_target(rusty_client)
