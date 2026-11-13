add_library(rr_net_sim_adapter STATIC
    src/SimulationAdapter.cpp
)

add_library(rr::net_sim_adapter ALIAS rr_net_sim_adapter)

target_include_directories(rr_net_sim_adapter
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_net_sim_adapter
    PUBLIC
        rr_core
        rr_net_proto
        rr_net_runtime
        rr_sim
)

rr_apply_compiler_options(rr_net_sim_adapter)
rr_register_first_party_target(rr_net_sim_adapter)
