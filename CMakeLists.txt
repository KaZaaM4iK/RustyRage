cmake_minimum_required(VERSION 3.25)
project(RustyRage)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)

# ===============================
#  1️⃣ Поиск зависимостей
# ===============================
find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(assimp REQUIRED)
find_package(glm REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Git QUIET)
if(NOT Git_FOUND)
    message(FATAL_ERROR "Git is required for PhysX source download but was not found.")
endif()

# ===============================
#  2️⃣ Ручное скачивание PhysX
# ===============================
option(PX_FREEBSD "Enable FreeBSD PhysX patch" OFF)

set(PHYSX_CLONE_DIR ${CMAKE_BINARY_DIR}/_deps/physx-src)
set(PHYSX_REPO_URL https://github.com/NVIDIA-Omniverse/PhysX.git)
set(PHYSX_GIT_TAG 107.3-physx-5.6.1)

# Устанавливаем переменную пути к исходникам явно
set(physx_SOURCE_DIR ${PHYSX_CLONE_DIR})

# Скачиваем репозиторий, если он еще не существует
if(NOT EXISTS "${PHYSX_CLONE_DIR}/.git")
    message(STATUS "Cloning PhysX repository into ${physx_SOURCE_DIR}...")
    
    # Клонирование и переключение на нужный тег
    execute_process(
        COMMAND ${GIT_EXECUTABLE} clone --branch ${PHYSX_GIT_TAG} --depth 1 ${PHYSX_REPO_URL} ${PHYSX_CLONE_DIR}
        RESULT_VARIABLE GIT_CLONE_RESULT
        OUTPUT_VARIABLE GIT_CLONE_OUTPUT
        ERROR_VARIABLE GIT_CLONE_ERROR
    )
    if(NOT GIT_CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "Git clone failed: ${GIT_CLONE_ERROR}")
    endif()

    # Обновление сабмодулей
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
        WORKING_DIRECTORY ${PHYSX_CLONE_DIR}
        RESULT_VARIABLE GIT_SUBMODULE_RESULT
    )
    if(NOT GIT_SUBMODULE_RESULT EQUAL 0)
        message(FATAL_ERROR "Git submodule update failed.")
    endif()

    message(STATUS "PhysX repository cloned and updated successfully.")
endif()

message(STATUS "✅ PhysX source path set: ${physx_SOURCE_DIR}")

# ===============================
#  3️⃣ Применяем патч PhysX
# ===============================

# 3.1: Патч для C++ файла
set(PHYSX_PATCH_SOURCE_FILE "${CMAKE_SOURCE_DIR}/physx_patches/PhysXIndicatorLinux.cpp")
set(PHYSX_PATCH_DEST_DIR "${physx_SOURCE_DIR}/physx/source/physx/src/internal/device/linux")

if(EXISTS "${PHYSX_PATCH_SOURCE_FILE}")
    # Папка создается теперь, когда physx_SOURCE_DIR гарантированно не пуст
    file(MAKE_DIRECTORY "${PHYSX_PATCH_DEST_DIR}")
    file(COPY "${PHYSX_PATCH_SOURCE_FILE}" DESTINATION "${PHYSX_PATCH_DEST_DIR}")
    message(STATUS "✅ Applied C++ patch: ${PHYSX_PATCH_SOURCE_FILE} → ${PHYSX_PATCH_DEST_DIR}")
else()
    message(WARNING "⚠️ C++ Patch file not found: ${PHYSX_PATCH_SOURCE_FILE}")
endif()

# 3.2: Патчи для заголовочных файлов (НОВОЕ ИСПРАВЛЕНИЕ)
set(PHYSX_HEADER_DEST_DIR "${physx_SOURCE_DIR}/physx/source/physx/src/internal/device")

# Копируем PhysXIndicator.h
set(PHYSX_INDICATOR_HEADER_FILE "${CMAKE_SOURCE_DIR}/physx_patches/PhysXIndicator.h")
if(EXISTS "${PHYSX_INDICATOR_HEADER_FILE}")
    file(MAKE_DIRECTORY "${PHYSX_HEADER_DEST_DIR}")
    file(COPY "${PHYSX_INDICATOR_HEADER_FILE}" DESTINATION "${PHYSX_HEADER_DEST_DIR}")
    message(STATUS "✅ Applied Header patch: ${PHYSX_INDICATOR_HEADER_FILE} → ${PHYSX_HEADER_DEST_DIR}")
else()
    message(FATAL_ERROR "⚠️ Header patch file not found: ${PHYSX_INDICATOR_HEADER_FILE}. Он необходим для компиляции.")
endif()

# Копируем nvPhysXtoDrv.h (НОВЫЙ ФАЙЛ)
set(NVPYHSX_TO_DRV_HEADER_FILE "${CMAKE_SOURCE_DIR}/physx_patches/nvPhysXtoDrv.h")
if(EXISTS "${NVPYHSX_TO_DRV_HEADER_FILE}")
    file(COPY "${NVPYHSX_TO_DRV_HEADER_FILE}" DESTINATION "${PHYSX_HEADER_DEST_DIR}")
    message(STATUS "✅ Applied Header patch: ${NVPYHSX_TO_DRV_HEADER_FILE} → ${PHYSX_HEADER_DEST_DIR}")
else()
    message(FATAL_ERROR "⚠️ Header patch file not found: ${NVPYHSX_TO_DRV_HEADER_FILE}. Он необходим для компиляции.")
endif()

# ===============================
#  4️⃣ Настройки PhysX
# ===============================
# Установка PHYSX_ROOT_DIR и CMAKE_MODULE_PATH
set(PHYSX_ROOT_DIR "${physx_SOURCE_DIR}/physx" CACHE PATH "PhysX root directory" FORCE)
message(STATUS "PHYSX ROOT: ${PHYSX_ROOT_DIR}")

set(PXSHARED_PATH ${physx_SOURCE_DIR}/pxshared)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    ${PXSHARED_PATH}/cmake
    ${physx_SOURCE_DIR}/physx/source/compiler/cmake/modules
)

# Конфигурация PhysX
set(PUBLIC_RELEASE 0 CACHE BOOL "Default to private release" FORCE)
set(PX_GENERATE_STATIC_LIBRARIES ON CACHE BOOL "Generate static PhysX libraries" FORCE)
set(PX_GENERATE_VEHICLE ON CACHE BOOL "Generate PhysXVehicle library" FORCE)
set(PX_GENERATE_COOKING ON CACHE BOOL "Generate PhysX Cooking library" FORCE)
set(PX_BUILDSNIPPETS OFF CACHE BOOL "Disable PhysX snippets" FORCE)
set(PX_BUILDPVDRUNTIME OFF CACHE BOOL "Disable PhysX PVD runtime" FORCE)
set(PX_EXCLUDE_DEFAULT_TARGETS ON CACHE BOOL "Exclude PhysX snippets and samples" FORCE)

if(ANDROID OR IOS)
    set(PX_NO_CUDA ON CACHE BOOL "Disable CUDA for mobile platforms" FORCE)
endif()

# Платформа и пути сборки
set(TARGET_BUILD_PLATFORM "linux" CACHE INTERNAL "")
set(PX_OUTPUT_LIB_DIR "${CMAKE_BINARY_DIR}/physx_libs" CACHE PATH "PhysX libraries output directory" FORCE)
set(PX_OUTPUT_BIN_DIR "${CMAKE_BINARY_DIR}/physx_bins" CACHE PATH "PhysX binaries output directory" FORCE)

# ===============================
#  5️⃣ Активируем PhysX
# ===============================
message(STATUS "✅ Activating PhysX targets with add_subdirectory()")
# Активация PhysX через add_subdirectory, который создает цели
add_subdirectory("${PHYSX_ROOT_DIR}/source/compiler/cmake" "${CMAKE_BINARY_DIR}/physx-build")

# Теперь библиотеки PhysX и их цели доступны (PhysX, PhysXCooking и т.д.)
message(STATUS "PhysX targets are now available.")

# ===============================
#  6️⃣ Подпроекты ядра и игры
# ===============================
add_subdirectory(src/Core)
add_subdirectory(src/Game)
add_subdirectory(src/Editor)

# НОВОЕ ИСПРАВЛЕНИЕ: Добавляем путь к заголовочным файлам PhysX для цели Core
set(PHYSX_INCLUDE_DIR "${PHYSX_ROOT_DIR}/include")
target_include_directories(Core PRIVATE
    ${PHYSX_INCLUDE_DIR}
)

message(STATUS "✅ Added PhysX include path for Core target: ${PHYSX_INCLUDE_DIR}")

# ===============================
#  7️⃣ Основной исполняемый файл
# ===============================
add_executable(RustyRage src/main.cpp)

target_include_directories(RustyRage PRIVATE
    ${SDL3_INCLUDE_DIRS}
    ${SDL3_TTF_INCLUDE_DIRS}
    ${assimp_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src/Core/include
    ${CMAKE_SOURCE_DIR}/src/Game/include
)

target_link_libraries(RustyRage PRIVATE
    Core
    Game
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    assimp::assimp
    glm::glm
    Vulkan::Vulkan
    # Используем raw-имена целей, которые создает add_subdirectory:
    PhysX
    PhysXVehicle2
    PhysXFoundation
    PhysXCommon
    PhysXExtensions
    PhysXCooking
)

# ===============================
#  7️⃣b Редактор карт
# ===============================
add_executable(RustyRageEditor src/main_editor.cpp)

target_include_directories(RustyRageEditor PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Editor/include
    ${CMAKE_SOURCE_DIR}/src/Core/include
)

target_link_libraries(RustyRageEditor PRIVATE
    Editor
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    glm::glm
)

# ===============================
#  8️⃣ Копирование ассетов
# ===============================
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/assets/fonts/DejaVuSans.ttf DESTINATION ${CMAKE_BINARY_DIR}/assets/fonts)
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})

# ===============================
#  9️⃣ FreeBSD patch (если нужен)
# ===============================
if(PX_FREEBSD)
    file(COPY ${CMAKE_SOURCE_DIR}/patches/physx-freebsd.patch DESTINATION ${physx_SOURCE_DIR})
    message(STATUS "Applied FreeBSD PhysX patch")
endif()