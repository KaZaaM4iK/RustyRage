# RustyRage — approved 0.1 foundation build.
#
# The pre-foundation PhysX/SDL3_ttf prototype is preserved under src/Core,
# src/Game and src/Editor. It is excluded from this build by default and can
# only be enabled explicitly with RUSTYRAGE_BUILD_LEGACY_PROTOTYPE=ON.
#
# Dependency direction and target boundaries are validated at configure time
# by cmake/TargetGraph.cmake; the same rule is re-run as a CTest test.

cmake_minimum_required(VERSION 3.28)

project(RustyRage
    VERSION 0.1.0
    DESCRIPTION "RustyRage multiplayer combat racing foundation"
    LANGUAGES CXX
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Use CMakePresets.json or a separate build directory.")
endif()

list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(RUSTYRAGE_BUILD_CLIENT "Build the graphical RustyRage client" ON)
option(RUSTYRAGE_BUILD_SERVER "Build the headless authoritative server" ON)
option(RUSTYRAGE_BUILD_TESTS "Build RustyRage tests and register them with CTest" ON)
option(RUSTYRAGE_BUILD_LEGACY_PROTOTYPE "Build the pre-foundation PhysX/SDL3_ttf prototype (not approved foundation)" OFF)
option(RUSTYRAGE_WARNINGS_AS_ERRORS "Treat first-party compiler warnings as errors" OFF)

# Project-wide language baseline. Jolt overrides this with C++17 in its own
# target scope as required by the approved architecture.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CompilerOptions)
include(PlatformChecks)
include(Dependencies)
include(TargetGraph)

rr_record_build_environment()

# GLM is approved for TASK_002 and is centralized through rr_core / rr::math.
rr_fetch_glm()

message(STATUS "RustyRage options: client=${RUSTYRAGE_BUILD_CLIENT} server=${RUSTYRAGE_BUILD_SERVER} tests=${RUSTYRAGE_BUILD_TESTS} legacy=${RUSTYRAGE_BUILD_LEGACY_PROTOTYPE}")

# --- First-party foundation targets ----------------------------------------
add_subdirectory(src/Foundation/Core)
add_subdirectory(src/Foundation/WorldData)
add_subdirectory(src/Foundation/Physics/API)
add_subdirectory(src/Foundation/Physics/Jolt)
add_subdirectory(src/Foundation/GameSimulation)
add_subdirectory(src/Foundation/Network/Protocol)
add_subdirectory(src/Foundation/Network/Transport)
add_subdirectory(src/Foundation/Network/Runtime)
add_subdirectory(src/Foundation/Network/SimulationAdapter)

if(RUSTYRAGE_BUILD_SERVER)
    add_subdirectory(src/Foundation/DedicatedServer)
endif()

if(RUSTYRAGE_BUILD_CLIENT)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Foundation/Platform/CMakeLists.txt")
        add_subdirectory(src/Foundation/Platform)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Foundation/Renderer/CMakeLists.txt")
        add_subdirectory(src/Foundation/Renderer)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Foundation/Client/CMakeLists.txt")
        add_subdirectory(src/Foundation/Client)
    else()
        message(STATUS "RUSTYRAGE_BUILD_CLIENT=ON: client target is introduced by Stage C (SDL3/SDL_GPU).")
    endif()
endif()

if(RUSTYRAGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# --- Optional legacy prototype ---------------------------------------------
# Included last so legacy targets and dependency lookups cannot affect the
# approved foundation targets or the isolation check.
rr_finalize_target_graph()

if(RUSTYRAGE_BUILD_LEGACY_PROTOTYPE)
    include(LegacyPrototype)
endif()
