add_library(rr_physics_api INTERFACE)

add_library(rr::physics_api ALIAS rr_physics_api)

target_include_directories(rr_physics_api
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_physics_api
    INTERFACE
        rr_core
)

rr_apply_compiler_options(rr_physics_api)
rr_register_first_party_target(rr_physics_api)
