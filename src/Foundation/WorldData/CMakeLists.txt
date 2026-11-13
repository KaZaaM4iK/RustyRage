rr_fetch_json()

add_library(rr_world STATIC
    src/MapHeader.cpp
    src/MapSerializer.cpp
    src/MapValidator.cpp
    src/TrackSplineSampler.cpp
    src/TrackMeshGenerator.cpp
    src/RouteGraph.cpp
)

add_library(rr::world ALIAS rr_world)

target_include_directories(rr_world
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_world
    PUBLIC
        rr_core
    PRIVATE
        nlohmann_json::nlohmann_json
)

rr_apply_compiler_options(rr_world)
rr_register_first_party_target(rr_world)
