add_library(rr_core STATIC
    src/BinaryReader.cpp
    src/BinaryWriter.cpp
    src/Log.cpp
)

add_library(rr::core ALIAS rr_core)

target_include_directories(rr_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(rr_core
    PUBLIC
        Threads::Threads
        glm::glm
)

# One centralized GLM configuration for the whole project. Do not define
# GLM_FORCE_LEFT_HANDED or the obsolete GLM_FORCE_RADIANS. The renderer owns
# the World -> View -> Clip conversion and must use this same depth convention.
target_compile_definitions(rr_core
    PUBLIC
        GLM_FORCE_DEPTH_ZERO_TO_ONE
)

rr_apply_compiler_options(rr_core)
rr_register_first_party_target(rr_core)
