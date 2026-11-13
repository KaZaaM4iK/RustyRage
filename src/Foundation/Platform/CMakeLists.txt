rr_find_sdl3()

add_library(rr_platform STATIC
    src/Window.cpp
)

add_library(rr::platform ALIAS rr_platform)

target_include_directories(rr_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_platform
    PUBLIC
        rr_core
    PRIVATE
        SDL3::SDL3
)

rr_apply_compiler_options(rr_platform)
rr_register_first_party_target(rr_platform)
