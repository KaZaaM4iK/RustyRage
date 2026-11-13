# Core module - Engine components
add_library(Core STATIC
    src/Application.cpp
    src/Audio.cpp
    src/Camera.cpp
    src/Config.cpp
    src/InputManager.cpp
    src/Localization.cpp
    src/Logging.cpp
    src/Model.cpp
    src/Physics.cpp
    src/Renderer.cpp
    ${CMAKE_SOURCE_DIR}/volk/volk.c
)

target_include_directories(Core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/Game/include
        # PhysX включает пути автоматически при компоновке с целями
    PRIVATE
        # ${PHYSX_ROOT_DIR}/include/cooking <-- УДАЛИТЬ: Не нужно, если линкуемся
        # ${PHYSX_ROOT_DIR}/include/vehicle2 <-- УДАЛИТЬ: Не нужно, если линкуемся
)

target_link_libraries(Core PUBLIC
    SDL3::SDL3
    assimp::assimp
    glm::glm
    Vulkan::Vulkan
    PhysX
    PhysXVehicle2
    PhysXFoundation
    PhysXCommon
    PhysXExtensions
    PhysXCooking
)

# Apply FreeBSD patch during build
if(PX_FREEBSD)
    add_custom_command(TARGET Core PRE_BUILD
        COMMAND patch -p1 < ${physx_SOURCE_DIR}/physx-freebsd.patch
        WORKING_DIRECTORY ${physx_SOURCE_DIR}
        COMMENT "Applying PhysX FreeBSD patch"
    )
endif()