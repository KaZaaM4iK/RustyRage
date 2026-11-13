add_library(rr_net_runtime STATIC
    src/Runtime.cpp
)

add_library(rr::net_runtime ALIAS rr_net_runtime)

target_include_directories(rr_net_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_net_runtime
    PUBLIC
        rr_core
        rr_net_proto
        rr_net_transport
)

rr_apply_compiler_options(rr_net_runtime)
rr_register_first_party_target(rr_net_runtime)
