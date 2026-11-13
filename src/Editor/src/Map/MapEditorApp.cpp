#include "Editor/Map/MapEditorApp.hpp"
#include "Core/InputManager.hpp"
#include "Core/Logging.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

namespace Editor {

MapEditorApp::MapEditorApp(int width, int height, const std::string &title)
    : m_width(width), m_height(height), m_title(title), m_running(false),
      m_window(nullptr), m_renderer(nullptr),
      m_currentFilePath("maps/untitled.rrmap"), m_objectIdCounter(0),
      m_structureIdCounter(0), m_zoneIdCounter(0),
      m_selectedTrackPoint(-1), m_trackClosedCached(false), m_dragging(false),
      m_gridSize(1.0f), m_gridSnapEnabled(true), m_dirty(false),
      m_ctrlHeld(false), m_rightMouseHeld(false) {}

MapEditorApp::~MapEditorApp() { shutdown(); }

void MapEditorApp::run() {
  if (!initialize()) {
    return;
  }

  m_running = true;
  Uint64 lastTicks = SDL_GetTicks();

  while (m_running) {
    const Uint64 currentTicks = SDL_GetTicks();
    const float deltaTime = (currentTicks - lastTicks) / 1000.0f;
    lastTicks = currentTicks;

    processEvents();
    m_input->update();
    update(deltaTime);
    render();
  }

  shutdown();
}

bool MapEditorApp::initialize() {
  LOG_INFO("Initializing AAA Map & UGC Editor");

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
    LOG_ERRORF("SDL_Init failed: %s", SDL_GetError());
    return false;
  }

  m_window = SDL_CreateWindow(m_title.c_str(), m_width, m_height,
                              SDL_WINDOW_RESIZABLE);
  if (!m_window) {
    LOG_ERRORF("SDL_CreateWindow failed: %s", SDL_GetError());
    return false;
  }
  SDL_StartTextInput(m_window);

  m_renderer = SDL_CreateRenderer(m_window, nullptr);
  if (!m_renderer) {
    LOG_ERRORF("SDL_CreateRenderer failed: %s", SDL_GetError());
    return false;
  }

  m_input = std::make_unique<Core::InputManager>();
  m_input->initialize();

  m_camera = std::make_unique<EditorCamera>();
  m_sceneRenderer = std::make_unique<EditorRenderer>(m_renderer);
  m_ui = std::make_unique<EditorUI>();
  if (!m_ui->initialize(m_renderer, m_width, m_height)) {
    LOG_ERROR("Failed to initialize editor UI");
    return false;
  }

  m_ui->setOnNewMap([this]() { newMap(); });
  m_ui->setOnSaveMap([this]() { saveMap(); });
  m_ui->setOnLoadMap([this](const std::string &path) { loadMap(path); });
  m_ui->setOnDeleteSelected([this]() { deleteSelected(); });
  m_ui->setOnToggleTrackClosed([this]() { toggleTrackClosed(); });
  m_ui->setOnRaceGrid([this]() { placeRaceStartGrid(); });
  m_ui->setOnExportUgcPackage([this]() { exportUgcModPackage(); });
  m_ui->setOnSwitchMode([this](EditorWorkspaceMode mode) {
    m_ui->setWorkspaceMode(mode);
    m_selectedObjectId.clear();
    m_selectedTrackPoint = -1;
    if (mode == EditorWorkspaceMode::TrackAndArena) {
      m_ui->setStatusMessage("Режим Трасс и Арены активен");
    } else if (mode == EditorWorkspaceMode::BaseArchitect) {
      m_ui->setStatusMessage("Режим Архитектора Баз и Обороны активен");
    } else if (mode == EditorWorkspaceMode::UgcLogicStudio) {
      m_ui->setStatusMessage("Режим Студии UGC Логики и Правил активен");
    }
  });

  m_inspector = std::make_unique<PropertyInspector>();
  m_inspector->initialize(m_renderer, "assets/fonts/DejaVuSans.ttf");
  m_inspector->onWindowResize(m_height);
  m_inspector->setOnFieldChanged(
      [this](const std::string &id, int fieldIndex, const std::string &value) {
        onInspectorFieldChanged(id, fieldIndex, value);
      });
  m_inspector->setOnMoveObject(
      [this](const std::string &id, const glm::vec3 &position) {
        onInspectorMoveObject(id, position);
      });

  ensureMapsDirectory();
  newMap();
  refreshMapList();

  LOG_INFO("AAA Map & UGC Editor successfully initialized");
  return true;
}

void MapEditorApp::shutdown() {
  if (m_ui) {
    m_ui->shutdown();
    m_ui.reset();
  }
  if (m_inspector) {
    m_inspector->shutdown();
    m_inspector.reset();
  }
  m_sceneRenderer.reset();
  m_camera.reset();
  m_input.reset();

  if (m_renderer) {
    SDL_DestroyRenderer(m_renderer);
    m_renderer = nullptr;
  }
  if (m_window) {
    SDL_DestroyWindow(m_window);
    m_window = nullptr;
  }
  SDL_Quit();
}

void MapEditorApp::cycleGridSize() {
  static const float kSteps[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f};
  for (size_t i = 0; i + 1 < sizeof(kSteps) / sizeof(kSteps[0]); ++i) {
    if (std::abs(m_gridSize - kSteps[i]) < 1e-3f) {
      m_gridSize = kSteps[i + 1];
      m_ui->setStatusMessage("Шаг сетки: " + std::to_string(m_gridSize) + " м");
      return;
    }
  }
  m_gridSize = 1.0f;
  m_ui->setStatusMessage("Шаг сетки: 1.0 м");
}

void MapEditorApp::processEvents() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      m_running = false;
    } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
      m_width = event.window.data1;
      m_height = event.window.data2;
      if (m_ui) {
        m_ui->onWindowResize(m_width, m_height);
      }
      if (m_inspector) {
        m_inspector->onWindowResize(m_height);
      }
    } else if (event.type == SDL_EVENT_KEY_DOWN) {
      if (event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) {
        m_ctrlHeld = true;
      }

      const bool inspectorFocused =
          m_inspector &&
          m_inspector->isMouseOverPanel(m_input->getMousePosition(), m_width);

      if (inspectorFocused && m_inspector) {
        m_inspector->handleKeyDown(event.key.key);
      } else if (event.key.key == SDLK_S && m_ctrlHeld) {
        saveMap();
      } else if (event.key.key == SDLK_N && m_ctrlHeld) {
        newMap();
      } else if (event.key.key == SDLK_Z && m_ctrlHeld) {
        undo();
      } else if (event.key.key == SDLK_Y && m_ctrlHeld) {
        redo();
      } else if (event.key.key == SDLK_G && m_ctrlHeld) {
        cycleGridSize();
      } else if (event.key.key == SDLK_DELETE) {
        deleteSelected();
      } else if (event.key.key == SDLK_D && m_ctrlHeld) {
        duplicateSelectedObject();
      } else if (event.key.key == SDLK_ESCAPE) {
        m_selectedObjectId.clear();
        m_selectedTrackPoint = -1;
        m_dragging = false;
        m_ui->setStatusMessage("Выделение снято");
      } else if (event.key.key == SDLK_TAB) {
        cycleSelection(m_ctrlHeld);
      } else if (event.key.key == SDLK_R && m_ctrlHeld) {
        placeRaceStartGrid();
      }
    } else if (event.type == SDL_EVENT_TEXT_INPUT) {
      if (m_inspector &&
          m_inspector->isMouseOverPanel(m_input->getMousePosition(), m_width)) {
        m_inspector->handleTextInput(std::string(event.text.text));
      }
    } else if (event.type == SDL_EVENT_KEY_UP) {
      if (event.key.key == SDLK_LCTRL || event.key.key == SDLK_RCTRL) {
        m_ctrlHeld = false;
      }
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
      if (event.button.button == SDL_BUTTON_RIGHT) {
        m_rightMouseHeld = true;
      } else if (event.button.button == SDL_BUTTON_LEFT) {
        const glm::vec2 mousePos(event.button.x, event.button.y);
        if (m_inspector && m_inspector->handleMouseClick(mousePos, m_width)) {
          // Inspector handled click
        } else if (m_ui->isMouseOverUI(mousePos)) {
          m_ui->handleMouseClick(mousePos);
        } else {
          const int prevTrackPoint = m_selectedTrackPoint;
          handleViewportClick(mousePos);

          const EditorTool tool = m_ui->getCurrentTool();
          if (tool == EditorTool::Select && !m_selectedObjectId.empty()) {
            glm::vec3 ground;
            const glm::ivec4 vp = m_ui->getViewportRect(
                (m_inspector && !m_selectedObjectId.empty())
                    ? m_inspector->panelWidth()
                    : 0);
            const glm::vec2 local(mousePos.x - vp.x, mousePos.y - vp.y);
            if (m_camera->screenToGround(local, ground)) {
              m_dragging = true;
              m_dragStartWorld = ground;
              if (MapObject *o = m_mapData.findObject(m_selectedObjectId)) {
                m_dragOriginalPos = o->position;
              } else if (StructureDef *s = m_mapData.findStructure(m_selectedObjectId)) {
                m_dragOriginalPos = s->position;
              }
            }
          } else if (tool == EditorTool::EditTrack &&
                     m_selectedTrackPoint >= 0 &&
                     m_selectedTrackPoint == prevTrackPoint) {
            glm::vec3 ground;
            const glm::ivec4 vp = m_ui->getViewportRect(0);
            const glm::vec2 local(mousePos.x - vp.x, mousePos.y - vp.y);
            if (m_camera->screenToGround(local, ground)) {
              m_dragging = true;
              m_dragStartWorld = ground;
              m_dragOriginalPos =
                  m_mapData.trackPoints[m_selectedTrackPoint].position;
            }
          }
        }
      }
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
      if (event.button.button == SDL_BUTTON_RIGHT) {
        m_rightMouseHeld = false;
      }
      if (event.button.button == SDL_BUTTON_LEFT && m_dragging) {
        m_dragging = false;
        const EditorTool tool = m_ui->getCurrentTool();
        if (tool == EditorTool::Select && !m_selectedObjectId.empty()) {
          if (MapObject *o = m_mapData.findObject(m_selectedObjectId)) {
            const glm::vec3 finalPos = o->position;
            const glm::vec3 delta = finalPos - m_dragOriginalPos;
            if (glm::dot(delta, delta) > 1e-6f) {
              o->position = m_dragOriginalPos;
              pushCommand(std::make_unique<MoveObjectCommand>(
                  m_selectedObjectId, m_dragOriginalPos, finalPos));
              m_dirty = true;
            }
          } else if (StructureDef *s = m_mapData.findStructure(m_selectedObjectId)) {
            const glm::vec3 finalPos = s->position;
            const glm::vec3 delta = finalPos - m_dragOriginalPos;
            if (glm::dot(delta, delta) > 1e-6f) {
              s->position = snapToGrid(finalPos);
              m_dirty = true;
            }
          }
        } else if (tool == EditorTool::EditTrack && m_selectedTrackPoint >= 0) {
          const size_t idx = static_cast<size_t>(m_selectedTrackPoint);
          if (idx < m_mapData.trackPoints.size()) {
            const glm::vec3 finalPos = m_mapData.trackPoints[idx].position;
            const glm::vec3 delta = finalPos - m_dragOriginalPos;
            if (glm::dot(delta, delta) > 1e-6f) {
              m_mapData.trackPoints[idx].position = m_dragOriginalPos;
              pushCommand(std::make_unique<MoveTrackPointCommand>(
                  idx, m_dragOriginalPos, finalPos));
              m_dirty = true;
            }
          }
        }
      }
    }

    if (m_input) {
      m_input->handleEvent(event);
    }
  }
}

void MapEditorApp::update(float deltaTime) {
  const int inspectorW = (m_inspector && (!m_selectedObjectId.empty() || m_selectedTrackPoint >= 0))
                             ? m_inspector->panelWidth()
                             : 0;
  const glm::ivec4 viewport = m_ui->getViewportRect(inspectorW);
  m_camera->setViewportSize(viewport.z, viewport.w);

  const glm::vec2 mousePos = m_input->getMousePosition();
  m_ui->setMousePosition(mousePos);
  const bool mouseOverUI =
      m_ui->isMouseOverUI(mousePos) ||
      (m_inspector && m_inspector->isMouseOverPanel(mousePos, m_width));
  m_camera->processInput(*m_input, deltaTime, m_rightMouseHeld && !mouseOverUI);

  if (m_dragging) {
    const int inspectorWDrag = (m_inspector && (!m_selectedObjectId.empty() || m_selectedTrackPoint >= 0))
                                   ? m_inspector->panelWidth()
                                   : 0;
    const glm::ivec4 vp = m_ui->getViewportRect(inspectorWDrag);
    const glm::vec2 local(mousePos.x - vp.x, mousePos.y - vp.y);
    glm::vec3 ground;
    if (m_camera->screenToGround(local, ground)) {
      const glm::vec3 delta = ground - m_dragStartWorld;
      const EditorTool tool = m_ui->getCurrentTool();
      if (tool == EditorTool::Select) {
        if (MapObject *o = m_mapData.findObject(m_selectedObjectId)) {
          o->position = snapToGrid(m_dragOriginalPos + delta);
          o->position.y = m_dragOriginalPos.y;
        } else if (StructureDef *s = m_mapData.findStructure(m_selectedObjectId)) {
          s->position = snapToGrid(m_dragOriginalPos + delta);
          s->position.y = m_dragOriginalPos.y;
        }
      } else if (tool == EditorTool::EditTrack && m_selectedTrackPoint >= 0) {
        const size_t idx = static_cast<size_t>(m_selectedTrackPoint);
        if (idx < m_mapData.trackPoints.size()) {
          glm::vec3 pos = snapToGrid(m_dragOriginalPos + delta);
          pos.y = m_dragOriginalPos.y;
          m_mapData.trackPoints[idx].position = pos;
        }
      }
    }
  }

  // Quick mode hotkeys
  if (m_input->isKeyJustPressed(SDL_SCANCODE_1) && !m_ctrlHeld) m_ui->setCurrentTool(EditorTool::Select);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_2) && !m_ctrlHeld) {
    if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::TrackAndArena) m_ui->setCurrentTool(EditorTool::EditTrack);
    else if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::BaseArchitect) m_ui->setCurrentTool(EditorTool::PlaceStructureWall);
    else if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::UgcLogicStudio) m_ui->setCurrentTool(EditorTool::PlaceTriggerZone);
  }
  if (m_input->isKeyJustPressed(SDL_SCANCODE_3) && !m_ctrlHeld) {
    if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::TrackAndArena) m_ui->setCurrentTool(EditorTool::PlaceSpawn);
    else if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::BaseArchitect) m_ui->setCurrentTool(EditorTool::PlaceStructureTurret);
  }
  if (m_input->isKeyJustPressed(SDL_SCANCODE_4) && !m_ctrlHeld) {
    if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::TrackAndArena) m_ui->setCurrentTool(EditorTool::PlaceCheckpoint);
    else if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::BaseArchitect) m_ui->setCurrentTool(EditorTool::PlaceStructureUtility);
  }
  if (m_input->isKeyJustPressed(SDL_SCANCODE_5) && !m_ctrlHeld) {
    if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::TrackAndArena) m_ui->setCurrentTool(EditorTool::PlacePowerUp);
    else if (m_ui->getWorkspaceMode() == EditorWorkspaceMode::BaseArchitect) m_ui->setCurrentTool(EditorTool::PlaceStructurePower);
  }

  if (m_inspector) {
    if (MapObject *selectedObj = m_mapData.findObject(m_selectedObjectId)) {
      m_inspector->setSelected(selectedObj);
    } else if (StructureDef *selectedStruct = m_mapData.findStructure(m_selectedObjectId)) {
      m_inspector->setSelectedStructure(selectedStruct);
    } else if (m_selectedTrackPoint >= 0 && m_selectedTrackPoint < static_cast<int>(m_mapData.trackPoints.size())) {
      m_inspector->setSelectedTrackPoint(&m_mapData.trackPoints[m_selectedTrackPoint], m_selectedTrackPoint);
    } else {
      m_inspector->clearSelection();
    }
    m_inspector->update(deltaTime);
  }

  m_ui->setMapName(m_mapData.name);
  m_ui->setObjectCount(static_cast<int>(m_mapData.objects.size()));
  m_ui->setStructureCount(static_cast<int>(m_mapData.structures.size()));
  m_ui->setMapMode(mapModeToString(m_mapData.mode));
  m_ui->setTrackPointCount(static_cast<int>(m_mapData.trackPoints.size()));
  m_ui->setLapCount(m_mapData.lapCount);
  m_ui->setSelectedObjectId(m_selectedObjectId);
  m_ui->setCurrentFilePath(m_currentFilePath);
  m_ui->setUndoLabel(m_undoStack.undoLabel());
  m_ui->setRedoLabel(m_undoStack.redoLabel());
  m_ui->setGridInfo(m_gridSize, m_gridSnapEnabled);
  m_ui->setDirty(m_dirty);
}

void MapEditorApp::render() {
  SDL_SetRenderDrawColor(m_renderer, 14, 16, 22, 255);
  SDL_RenderClear(m_renderer);

  const int inspectorW = (m_inspector && (!m_selectedObjectId.empty() || m_selectedTrackPoint >= 0))
                             ? m_inspector->panelWidth()
                             : 0;
  const glm::ivec4 viewport = m_ui->getViewportRect(inspectorW);
  m_sceneRenderer->setViewport(viewport.x, viewport.y, viewport.z, viewport.w);
  m_sceneRenderer->renderScene(m_mapData, *m_camera, m_selectedObjectId,
                               m_selectedTrackPoint);
  m_ui->render();

  if (m_inspector) {
    m_inspector->render(m_width);
  }

  SDL_RenderPresent(m_renderer);
}

void MapEditorApp::newMap() {
  m_objectIdCounter = 0;
  m_structureIdCounter = 0;
  m_zoneIdCounter = 0;
  m_mapData.createDefault(m_objectIdCounter);
  m_selectedObjectId.clear();
  m_selectedTrackPoint = -1;
  m_currentFilePath = mapsDirectory() + "/untitled.rrmap";
  m_undoStack.clear();
  m_dirty = false;
  m_ui->setStatusMessage("Создана новая карта RustyRage");
}

void MapEditorApp::saveMap() {
  ensureMapsDirectory();

  const std::string safeName = m_mapData.name.empty() ? "untitled" : m_mapData.name;
  std::string fileName = safeName;
  std::replace(fileName.begin(), fileName.end(), ' ', '_');
  m_currentFilePath = mapsDirectory() + "/" + fileName + ".rrmap";

  if (!m_mapData.saveToFile(m_currentFilePath)) {
    m_ui->setStatusMessage("Ошибка сохранения карты");
    return;
  }

  m_dirty = false;
  refreshMapList();
  m_ui->setStatusMessage("Карта и база успешно сохранены: " + fileName);
}

void MapEditorApp::exportUgcModPackage() {
  ensureMapsDirectory();
  std::string safeName = m_mapData.name.empty() ? "custom_mod" : m_mapData.name;
  std::replace(safeName.begin(), safeName.end(), ' ', '_');
  std::string manifestPath = mapsDirectory() + "/" + safeName + "_ugc_package.json";

  std::ofstream file(manifestPath);
  if (!file.is_open()) {
    m_ui->setStatusMessage("Ошибка экспорта UGC пакета");
    return;
  }

  file << "{\n";
  file << "  \"ugcVersion\": \"1.0.0\",\n";
  file << "  \"modTitle\": \"" << m_mapData.name << "\",\n";
  file << "  \"author\": \"CommunityCreator\",\n";
  file << "  \"trackPointCount\": " << m_mapData.trackPoints.size() << ",\n";
  file << "  \"structuresCount\": " << m_mapData.structures.size() << ",\n";
  file << "  \"objectsCount\": " << m_mapData.objects.size() << ",\n";
  file << "  \"triggerZonesCount\": " << m_mapData.ugcRules.triggerZones.size() << ",\n";
  file << "  \"customRulesCount\": " << m_mapData.ugcRules.rules.size() << ",\n";
  file << "  \"compatibleGameVersion\": \"1.0.0-PROD\"\n";
  file << "}\n";
  file.close();

  m_ui->setStatusMessage("UGC Мод успешно экспортирован в: " + manifestPath);
}

void MapEditorApp::loadMap(const std::string &path) {
  const std::string fullPath = mapsDirectory() + "/" + path;
  if (m_mapData.loadFromFile(fullPath)) {
    m_currentFilePath = fullPath;
    m_selectedObjectId.clear();
    m_selectedTrackPoint = -1;
    m_undoStack.clear();
    m_dirty = false;
    m_ui->setStatusMessage("Карта загружена: " + path);
  } else {
    m_ui->setStatusMessage("Ошибка загрузки файла");
  }
}

void MapEditorApp::refreshMapList() {
  ensureMapsDirectory();
  std::vector<std::string> files;
  for (const auto &entry : std::filesystem::directory_iterator(mapsDirectory())) {
    if (entry.is_regular_file() && entry.path().extension() == ".rrmap") {
      files.push_back(entry.path().filename().string());
    }
  }
  std::sort(files.begin(), files.end());
  m_ui->setMapFileList(files);
}

void MapEditorApp::deleteSelected() {
  if (m_ui->getCurrentTool() == EditorTool::EditTrack && m_selectedTrackPoint >= 0) {
    deleteTrackPoint(m_selectedTrackPoint);
    return;
  }
  if (!m_selectedObjectId.empty()) {
    if (m_mapData.removeStructure(m_selectedObjectId)) {
      m_ui->setStatusMessage("Строение снесено: " + m_selectedObjectId);
      m_selectedObjectId.clear();
      m_dirty = true;
      return;
    }
    pushCommand(std::make_unique<RemoveObjectCommand>(m_selectedObjectId));
    m_ui->setStatusMessage("Объект удалён");
    m_selectedObjectId.clear();
    m_dirty = true;
  }
}

void MapEditorApp::handleViewportClick(const glm::vec2 &screenPos) {
  const int inspectorW = (m_inspector && (!m_selectedObjectId.empty() || m_selectedTrackPoint >= 0))
                             ? m_inspector->panelWidth()
                             : 0;
  const glm::ivec4 viewport = m_ui->getViewportRect(inspectorW);
  const glm::vec2 localPos(screenPos.x - viewport.x, screenPos.y - viewport.y);

  EditorTool tool = m_ui->getCurrentTool();

  if (tool == EditorTool::EditTrack) {
    int idx = findTrackPointNear(localPos);
    if (idx >= 0) {
      m_selectedTrackPoint = idx;
      m_selectedObjectId.clear();
      m_ui->setStatusMessage("Выбрана точка трассы #" + std::to_string(idx));
      return;
    }
    glm::vec3 groundPoint;
    if (m_camera->screenToGround(localPos, groundPoint)) {
      addTrackPointAt(groundPoint);
    }
    return;
  }

  if (tool == EditorTool::Select || tool == EditorTool::Delete) {
    StructureDef *s = findStructureNear(localPos);
    if (s) {
      m_selectedObjectId = s->id;
      m_selectedTrackPoint = -1;
      if (tool == EditorTool::Delete) deleteSelected();
      else m_ui->setStatusMessage("Выбрано строение: " + s->name);
      return;
    }

    MapObject *object = findObjectNear(localPos);
    if (object) {
      m_selectedObjectId = object->id;
      m_selectedTrackPoint = -1;
      if (tool == EditorTool::Delete) deleteSelected();
      else m_ui->setStatusMessage("Выбран объект " + object->id);
      return;
    }

    m_selectedObjectId.clear();
    return;
  }

  glm::vec3 groundPoint;
  if (!m_camera->screenToGround(localPos, groundPoint)) {
    return;
  }

if (tool == EditorTool::PlaceStructureWall) {
    placeStructureAt(groundPoint, StructureType::FortifiedWall);
  } else if (tool == EditorTool::PlaceStructureTurret) {
    placeStructureAt(groundPoint, StructureType::AutoMinigunTurret);
  } else if (tool == EditorTool::PlaceStructureUtility) {
    placeStructureAt(groundPoint, StructureType::RepairBay);
  } else if (tool == EditorTool::PlaceStructurePower) {
    placeStructureAt(groundPoint, StructureType::PowerGenerator);
  } else if (tool == EditorTool::PlaceTriggerZone) {
    placeTriggerZoneAt(groundPoint);
  } else {
    placeObjectAt(groundPoint);
  }
}

void MapEditorApp::placeStructureAt(const glm::vec3 &position, StructureType type) {
  StructureDef def = StructureDef::createPreset(type, snapToGrid(position));
  def.id = "struct_" + std::to_string(++m_structureIdCounter);
  m_mapData.structures.push_back(def);
  m_selectedObjectId = def.id;
  m_dirty = true;
  m_ui->setStatusMessage("Построено: " + def.name);
}

void MapEditorApp::placeTriggerZoneAt(const glm::vec3 &position) {
  UgcTriggerZone zone;
  zone.id = "zone_" + std::to_string(++m_zoneIdCounter);
  zone.name = "Зона захвата #" + std::to_string(m_zoneIdCounter);
  zone.center = snapToGrid(position);
  zone.extents = glm::vec3(12.0f, 6.0f, 12.0f);
  m_mapData.ugcRules.triggerZones.push_back(zone);
  m_dirty = true;
  m_ui->setStatusMessage("Создана UGC триггер-зона: " + zone.id);
}

void MapEditorApp::placeObjectAt(const glm::vec3 &position) {
  MapObject object;
  object.position = snapToGrid(position);
  object.position.y = 0.0f;

  switch (m_ui->getCurrentTool()) {
  case EditorTool::PlaceSpawn:
    object.type = MapObjectType::SpawnPoint;
    object.row = m_ui->getSpawnRow();
    object.id = m_mapData.generateObjectId(MapObjectType::SpawnPoint, m_objectIdCounter);
    break;
  case EditorTool::PlaceCheckpoint:
    object.type = MapObjectType::Checkpoint;
    object.radius = 12.0f;
    object.id = m_mapData.generateObjectId(MapObjectType::Checkpoint, m_objectIdCounter);
    break;
  case EditorTool::PlacePowerUp:
    object.type = MapObjectType::PowerUpSpawn;
    object.id = m_mapData.generateObjectId(MapObjectType::PowerUpSpawn, m_objectIdCounter);
    break;
  case EditorTool::PlaceHazard:
    object.type = MapObjectType::HazardCrusher;
    object.id = m_mapData.generateObjectId(MapObjectType::HazardCrusher, m_objectIdCounter);
    break;
  case EditorTool::PlaceJumpPad:
    object.type = MapObjectType::JumpPad;
    object.id = m_mapData.generateObjectId(MapObjectType::JumpPad, m_objectIdCounter);
    break;
  default:
    return;
  }

  pushCommand(std::make_unique<AddObjectCommand>(object));
  m_selectedObjectId = object.id;
  m_ui->setStatusMessage("Добавлен объект " + object.id);
  m_dirty = true;
}

void MapEditorApp::addTrackPointAt(const glm::vec3 &position) {
  TrackPoint point(snapToGrid(position));
  point.width = m_mapData.trackPoints.empty() ? 14.0f : m_mapData.trackPoints.back().width;
  pushCommand(std::make_unique<AddTrackPointCommand>(point));
  m_selectedTrackPoint = static_cast<int>(m_mapData.trackPoints.size()) - 1;
  m_dirty = true;
  m_ui->setStatusMessage("Добавлена точка трассы #" + std::to_string(m_selectedTrackPoint));
}

void MapEditorApp::deleteTrackPoint(int index) {
  if (index < 0 || index >= static_cast<int>(m_mapData.trackPoints.size())) return;
  pushCommand(std::make_unique<RemoveTrackPointCommand>(static_cast<size_t>(index)));
  m_selectedTrackPoint = -1;
  m_dirty = true;
  m_ui->setStatusMessage("Удалена точка трассы");
}

void MapEditorApp::adjustSelectedTrackWidth(float delta) {
  if (m_selectedTrackPoint < 0 || m_selectedTrackPoint >= static_cast<int>(m_mapData.trackPoints.size())) return;
  size_t idx = static_cast<size_t>(m_selectedTrackPoint);
  float oldW = m_mapData.trackPoints[idx].width;
  float newW = std::clamp(oldW + delta, 4.0f, 40.0f);
  pushCommand(std::make_unique<SetTrackPointWidthCommand>(idx, oldW, newW));
  m_dirty = true;
}

void MapEditorApp::insertTrackPointAfterSelected() {
  if (m_selectedTrackPoint < 0 || m_selectedTrackPoint >= static_cast<int>(m_mapData.trackPoints.size())) return;
  size_t idx = static_cast<size_t>(m_selectedTrackPoint);
  size_t n = m_mapData.trackPoints.size();
  size_t nextIdx = m_mapData.trackClosed ? (idx + 1) % n : (idx + 1);
  if (!m_mapData.trackClosed && idx + 1 >= n) return;

  TrackPoint mid;
  mid.position = snapToGrid((m_mapData.trackPoints[idx].position + m_mapData.trackPoints[nextIdx].position) * 0.5f);
  mid.width = (m_mapData.trackPoints[idx].width + m_mapData.trackPoints[nextIdx].width) * 0.5f;

  pushCommand(std::make_unique<InsertTrackPointCommand>(idx + 1, mid));
  m_selectedTrackPoint = static_cast<int>(idx + 1);
  m_dirty = true;
}

void MapEditorApp::duplicateSelectedObject() {
  if (m_selectedObjectId.empty()) return;
  if (const MapObject *src = m_mapData.findObject(m_selectedObjectId)) {
    MapObject copy = *src;
    copy.id = m_mapData.generateObjectId(src->type, m_objectIdCounter);
    copy.position = snapToGrid(src->position + glm::vec3(m_gridSize, 0.0f, 0.0f));
    pushCommand(std::make_unique<AddObjectCommand>(copy));
    m_selectedObjectId = copy.id;
    m_dirty = true;
  }
}

void MapEditorApp::nudgeSelected(const glm::vec3 &delta) {
  if (!m_selectedObjectId.empty()) {
    if (MapObject *o = m_mapData.findObject(m_selectedObjectId)) {
      glm::vec3 newPos = snapToGrid(o->position + delta);
      pushCommand(std::make_unique<MoveObjectCommand>(m_selectedObjectId, o->position, newPos));
      m_dirty = true;
    }
  }
}

void MapEditorApp::cycleSelection(bool reverse) {
  if (m_mapData.objects.empty()) return;
  m_selectedObjectId = m_mapData.objects[0].id;
  m_ui->setStatusMessage("Выбран: " + m_selectedObjectId);
}

void MapEditorApp::placeRaceStartGrid() {
  if (m_mapData.trackPoints.empty()) return;
  const glm::vec3 start = m_mapData.trackPoints[0].position;
  glm::vec3 forward(0.f, 0.f, 1.f);
  if (m_mapData.trackPoints.size() >= 2) {
    forward = glm::normalize(m_mapData.trackPoints[1].position - start);
  }
  glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));

  for (int row = 1; row <= 2; ++row) {
    for (int col = 0; col < 4; ++col) {
      MapObject spawn;
      spawn.type = MapObjectType::SpawnPoint;
      spawn.row = row;
      spawn.id = m_mapData.generateObjectId(MapObjectType::SpawnPoint, m_objectIdCounter);
      spawn.position = snapToGrid(start + right * ((col - 1.5f) * 6.0f) - forward * ((row - 1) * 8.0f));
      pushCommand(std::make_unique<AddObjectCommand>(spawn));
    }
  }
  m_dirty = true;
  m_ui->setStatusMessage("Стартовая сетка 4+4 расставлена");
}

void MapEditorApp::toggleTrackClosed() {
  pushCommand(std::make_unique<SetTrackClosedCommand>(m_mapData.trackClosed, !m_mapData.trackClosed));
  m_dirty = true;
  m_ui->setStatusMessage(m_mapData.trackClosed ? "Трасса замкнута в кольцо" : "Трасса разомкнута");
}

void MapEditorApp::selectTrackPointNear(const glm::vec2 &screenPos) {
  m_selectedTrackPoint = findTrackPointNear(screenPos);
}

void MapEditorApp::selectObjectAt(const glm::vec2 &screenPos) {
  MapObject *object = findObjectNear(screenPos);
  m_selectedObjectId = object ? object->id : "";
}

MapObject *MapEditorApp::findObjectNear(const glm::vec2 &screenPos, float maxDistance) {
  MapObject *closest = nullptr;
  float closestDist = maxDistance;
  for (MapObject &object : m_mapData.objects) {
    glm::vec2 projected = m_camera->worldToScreen(object.position);
    float dist = glm::length(projected - screenPos);
    if (dist < closestDist) {
      closestDist = dist;
      closest = &object;
    }
  }
  return closest;
}

StructureDef *MapEditorApp::findStructureNear(const glm::vec2 &screenPos, float maxDistance) {
  StructureDef *closest = nullptr;
  float closestDist = maxDistance;
  for (StructureDef &s : m_mapData.structures) {
    glm::vec2 projected = m_camera->worldToScreen(s.position);
    float dist = glm::length(projected - screenPos);
    if (dist < closestDist) {
      closestDist = dist;
      closest = &s;
    }
  }
  return closest;
}

int MapEditorApp::findTrackPointNear(const glm::vec2 &screenPos, float maxDistance) const {
  int closestIdx = -1;
  float closestDist = maxDistance;
  for (size_t i = 0; i < m_mapData.trackPoints.size(); ++i) {
    glm::vec2 projected = m_camera->worldToScreen(m_mapData.trackPoints[i].position);
    float dist = glm::length(projected - screenPos);
    if (dist < closestDist) {
      closestDist = dist;
      closestIdx = static_cast<int>(i);
    }
  }
  return closestIdx;
}

std::string MapEditorApp::mapsDirectory() const { return "maps"; }

void MapEditorApp::ensureMapsDirectory() const {
  std::filesystem::create_directories(mapsDirectory());
}

void MapEditorApp::pushCommand(std::unique_ptr<Command> cmd) {
  m_undoStack.execute(std::move(cmd), m_mapData);
}

glm::vec3 MapEditorApp::snapToGrid(const glm::vec3 &p) const {
  if (!m_gridSnapEnabled || m_gridSize <= 1e-3f) return p;
  const float g = m_gridSize;
  return glm::vec3(std::round(p.x / g) * g, p.y, std::round(p.z / g) * g);
}

void MapEditorApp::undo() {
  if (m_undoStack.canUndo()) {
    std::string label = m_undoStack.undoLabel();
    m_undoStack.undo(m_mapData);
    m_ui->setStatusMessage("Отменено: " + label);
    m_dirty = true;
  }
}

void MapEditorApp::redo() {
  if (m_undoStack.canRedo()) {
    std::string label = m_undoStack.redoLabel();
    m_undoStack.redo(m_mapData);
    m_ui->setStatusMessage("Возврат: " + label);
    m_dirty = true;
  }
}

void MapEditorApp::onInspectorFieldChanged(const std::string &id, int fieldIndex, const std::string &value) {
  if (MapObject *obj = m_mapData.findObject(id)) {
    if (fieldIndex == 2) {
      try { obj->radius = std::stof(value); } catch (...) {}
    } else if (fieldIndex == 3) {
      try { obj->row = std::stoi(value); } catch (...) {}
    } else if (fieldIndex == 4) {
      obj->isFinishLine = (value == "true" || value == "1");
    } else if (fieldIndex == 5) {
      try { obj->health = std::stof(value); } catch (...) {}
    }
    m_dirty = true;
  } else if (StructureDef *s = m_mapData.findStructure(id)) {
    if (fieldIndex == 1) s->name = value;
    else if (fieldIndex == 3) { try { s->maxHealth = std::stof(value); } catch (...) {} }
    else if (fieldIndex == 4) { try { s->armorRating = std::stof(value); } catch (...) {} }
    else if (fieldIndex == 5) { try { s->attackRange = std::stof(value); } catch (...) {} }
    else if (fieldIndex == 6) { try { s->damagePerSecond = std::stof(value); } catch (...) {} }
    else if (fieldIndex == 7) { try { s->scrapCost = std::stoi(value); } catch (...) {} }
    else if (fieldIndex == 8) { try { s->energyDrain = std::stoi(value); } catch (...) {} }
    else if (fieldIndex == 9) { try { s->teamId = std::stoi(value); } catch (...) {} }
    else if (fieldIndex == 10) s->triggerTag = value;
    m_dirty = true;
  }
}

void MapEditorApp::onInspectorMoveObject(const std::string &id, const glm::vec3 &position) {
  if (MapObject *obj = m_mapData.findObject(id)) {
    glm::vec3 snapped = snapToGrid(position);
    pushCommand(std::make_unique<MoveObjectCommand>(id, obj->position, snapped));
    m_dirty = true;
  } else if (StructureDef *s = m_mapData.findStructure(id)) {
    s->position = snapToGrid(position);
    m_dirty = true;
  }
}

} // namespace Editor
