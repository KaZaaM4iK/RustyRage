add_executable(rusty_server
    src/main.cpp
)

target_link_libraries(rusty_server
    PRIVATE
        rr_core
        rr_world
        rr_sim
        rr_physics_jolt
        rr_net_proto
        rr_net_runtime
        rr_net_sim_adapter
)

target_compile_definitions(rusty_server
    PRIVATE
        RUSTYRAGE_VERSION="${PROJECT_VERSION}"
)

rr_apply_compiler_options(rusty_server)
rr_register_first_party_target(rusty_server)
