rr_find_gns()

add_library(rr_net_transport STATIC
    src/GnsTransport.cpp
)

add_library(rr::net_transport ALIAS rr_net_transport)

target_include_directories(rr_net_transport
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_net_transport
    PUBLIC
        rr_core
    PRIVATE
        rr_gns
)

rr_apply_compiler_options(rr_net_transport)
rr_register_first_party_target(rr_net_transport)
