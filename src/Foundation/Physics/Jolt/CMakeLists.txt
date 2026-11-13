rr_fetch_jolt()

add_library(rr_physics_jolt STATIC
    src/JoltPhysicsWorld.cpp
)

add_library(rr::physics_jolt ALIAS rr_physics_jolt)

target_include_directories(rr_physics_jolt
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(rr_physics_jolt
    PUBLIC
        rr_physics_api
    PRIVATE
        Jolt
)

rr_apply_compiler_options(rr_physics_jolt)
rr_register_first_party_target(rr_physics_jolt)
