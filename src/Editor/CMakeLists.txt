add_library(Editor STATIC
    src/Map/MapData.cpp
    src/Shared/EditorCamera.cpp
    src/Shared/EditorRenderer.cpp
    src/Shared/EditorUI.cpp
    src/Map/MapEditorApp.cpp
    src/Map/TrackMesh.cpp
    src/Vehicle/VehicleDef.cpp
    src/Shared/UndoStack.cpp
    src/Shared/PropertyInspector.cpp
    src/Vehicle/VehicleEditorApp.cpp
)

target_include_directories(Editor PUBLIC
    include
    ${CMAKE_SOURCE_DIR}/src/Core/include
)

target_link_libraries(Editor PUBLIC
    Core
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    glm::glm
)
