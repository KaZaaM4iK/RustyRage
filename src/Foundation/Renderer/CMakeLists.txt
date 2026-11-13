rr_find_sdl3()

add_library(rr_render STATIC
    src/GpuRenderer.cpp
)

add_library(rr::render ALIAS rr_render)

target_include_directories(rr_render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_render
    PUBLIC
        rr_core
        rr_platform
    PRIVATE
        SDL3::SDL3
)

target_compile_definitions(rr_render
    PRIVATE
        RUSTYRAGE_FOUNDATION_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders/foundation"
)

rr_apply_compiler_options(rr_render)
rr_register_first_party_target(rr_render)
