add_library(rr_net_proto STATIC
    src/Protocol.cpp
)

add_library(rr::net_proto ALIAS rr_net_proto)

target_include_directories(rr_net_proto
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_net_proto
    PUBLIC
        rr_core
)

rr_apply_compiler_options(rr_net_proto)
rr_register_first_party_target(rr_net_proto)
