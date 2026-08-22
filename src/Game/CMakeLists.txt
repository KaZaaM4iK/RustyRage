# Game module - Rusty Rage game logic
add_library(Game STATIC
    src/GameModes.cpp
    src/Host.cpp
    src/Matchmaking.cpp
    src/Player.cpp
    src/RustyRageGame.cpp
    src/Track.cpp
    src/UI.cpp
    src/Vehicle.cpp
)

target_include_directories(Game PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/src/Core/include
    ${CMAKE_SOURCE_DIR}/src/Editor/include
)

target_link_libraries(Game PUBLIC
    Core
    Editor
    SDL3::SDL3
    glm::glm
)