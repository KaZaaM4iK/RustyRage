add_library(rr_sim STATIC
    src/SimulationClock.cpp
)

add_library(rr::sim ALIAS rr_sim)

target_include_directories(rr_sim
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_sim
    PUBLIC
        rr_core
        rr_world
        rr_physics_api
)

rr_apply_compiler_options(rr_sim)
rr_register_first_party_target(rr_sim)
