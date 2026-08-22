#include "Editor/Map/MapEditorApp.hpp"
#include "Core/InputManager.hpp"
#include "Core/Logging.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

namespace Editor {

MapEditorApp::MapEditorApp(int width, int height, const std::string &title)
    : m_width(width), m_height(height), m_title(title), m_running(false),
      m_window(nullptr), m_renderer(nullptr),
      m_currentFilePath("maps/untitled.rrmap"), m_objectIdCounter(0),
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
  LOG_INFO("Initializing map editor");

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

  LOG_INFO("Map editor initialized");
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

      // Inspector focus: when the panel is hovered, route keys to it first
      // so typing in a field doesn't trigger global hotkeys.
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
      } else if (event.key.key == SDLK_F2) {
        static const char *kNames[] = {"New Map", "Arena Alpha", "Desert Run",
                                       "Night Circuit", "Harbor Clash"};
        static size_t kIdx = 0;
        kIdx = (kIdx + 1) % (sizeof(kNames) / sizeof(kNames[0]));
        m_mapData.name = kNames[kIdx];
        m_dirty = true;
        m_ui->setStatusMessage(std::string("Имя карты: ") + m_mapData.name);
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
          // Inspector panel swallowed the click.
        } else if (m_ui->isMouseOverUI(mousePos)) {
          m_ui->handleMouseClick(mousePos);
        } else {
          const std::string prevSelectedId = m_selectedObjectId;
          const int prevTrackPoint = m_selectedTrackPoint;
          handleViewportClick(mousePos);
          // Drag только если выбор сохранился / сменился на реальный
          // объект/точку
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
  const int inspectorW = (m_inspector && !m_selectedObjectId.empty())
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
    const int inspectorWDrag = (m_inspector && !m_selectedObjectId.empty())
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

  if (m_input->isKeyJustPressed(SDL_SCANCODE_1))
    m_ui->setCurrentTool(EditorTool::Select);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_2))
    m_ui->setCurrentTool(EditorTool::PlaceSpawn);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_3))
    m_ui->setCurrentTool(EditorTool::PlaceCheckpoint);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_4))
    m_ui->setCurrentTool(EditorTool::PlacePowerUp);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_5))
    m_ui->setCurrentTool(EditorTool::PlaceDestructible);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_7))
    m_ui->setCurrentTool(EditorTool::PlaceObstacle);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_8))
    m_ui->setCurrentTool(EditorTool::PlaceDecoration);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_6))
    m_ui->setCurrentTool(EditorTool::EditTrack);
  if (m_input->isKeyJustPressed(SDL_SCANCODE_C) && m_ctrlHeld)
    toggleTrackClosed();
  if (m_input->isKeyJustPressed(SDL_SCANCODE_M) && !m_ctrlHeld) {
    switch (m_mapData.mode) {
    case MapMode::Both:
      m_mapData.mode = MapMode::Arena;
      break;
    case MapMode::Arena:
      m_mapData.mode = MapMode::Vigilante;
      break;
    case MapMode::Vigilante:
      m_mapData.mode = MapMode::Both;
      break;
    }
    m_dirty = true;
    m_ui->setStatusMessage(std::string("Режим карты: ") +
                           mapModeToString(m_mapData.mode));
  }
  if (m_input->isKeyJustPressed(SDL_SCANCODE_L) && !m_ctrlHeld) {
    static const int kLaps[] = {1, 3, 5, 10};
    int idx = 0;
    for (int i = 0; i < 4; ++i) {
      if (m_mapData.lapCount == kLaps[i]) {
        idx = i;
        break;
      }
    }
    m_mapData.lapCount = kLaps[(idx + 1) % 4];
    m_dirty = true;
    m_ui->setStatusMessage("Кругов: " + std::to_string(m_mapData.lapCount));
  }
  if (m_input->isKeyJustPressed(SDL_SCANCODE_P) && !m_ctrlHeld) {
    if (MapObject *o = m_mapData.findObject(m_selectedObjectId)) {
      if (o->type == MapObjectType::PowerUpSpawn) {
        const int oldSub = o->subtype;
        const int newSub = (oldSub + 1) % 6; // 0..5
        pushCommand(std::make_unique<SetObjectFieldCommand>(
            o->id, SetObjectFieldCommand::Field::Subtype,
            std::to_string(oldSub), std::to_string(newSub)));
        m_dirty = true;
        m_ui->setStatusMessage(std::string("Пауэр-ап: ") +
                               powerUpKindToString(powerUpKindFromInt(newSub)));
      }
    }
  }
  if (m_ui->getCurrentTool() == EditorTool::EditTrack &&
      m_selectedTrackPoint >= 0) {
    const float wheel = m_input->getMouseWheelDelta();
    if (wheel != 0.0f && m_ctrlHeld) {
      adjustSelectedTrackWidth(wheel * 1.0f);
    }
    if (m_input->isKeyJustPressed(SDL_SCANCODE_LEFTBRACKET)) {
      adjustSelectedTrackWidth(-1.0f);
    }
    if (m_input->isKeyJustPressed(SDL_SCANCODE_RIGHTBRACKET)) {
      adjustSelectedTrackWidth(1.0f);
    }
  }
  if (m_ui->getCurrentTool() == EditorTool::EditTrack &&
      m_input->isKeyJustPressed(SDL_SCANCODE_I)) {
    insertTrackPointAfterSelected();
  }
  if (m_input->isKeyJustPressed(SDL_SCANCODE_DELETE))
    deleteSelected();

  if (m_input->isKeyJustPressed(SDL_SCANCODE_G) && !m_ctrlHeld) {
    m_gridSnapEnabled = !m_gridSnapEnabled;
    m_ui->setStatusMessage(m_gridSnapEnabled ? "Привязка к сетке: вкл"
                                             : "Привязка к сетке: выкл");
    m_ui->setGridInfo(m_gridSize, m_gridSnapEnabled);
  }

  if (m_input->isKeyJustPressed(SDL_SCANCODE_F) && !m_ctrlHeld) {
    if (!m_selectedObjectId.empty()) {
      if (const MapObject *o = m_mapData.findObject(m_selectedObjectId)) {
        m_camera->focusOn(o->position, 35.0f);
        m_ui->setStatusMessage("Камера: " + o->id);
      }
    } else if (m_selectedTrackPoint >= 0 &&
               m_selectedTrackPoint <
                   static_cast<int>(m_mapData.trackPoints.size())) {
      m_camera->focusOn(
          m_mapData.trackPoints[static_cast<size_t>(m_selectedTrackPoint)]
              .position,
          35.0f);
      m_ui->setStatusMessage("Камера: точка #" +
                             std::to_string(m_selectedTrackPoint));
    }
  }

  {
    const float step = m_gridSnapEnabled ? m_gridSize : 0.25f;
    if (m_input->isKeyJustPressed(SDL_SCANCODE_LEFT))
      nudgeSelected({-step, 0.f, 0.f});
    if (m_input->isKeyJustPressed(SDL_SCANCODE_RIGHT))
      nudgeSelected({step, 0.f, 0.f});
    if (m_input->isKeyJustPressed(SDL_SCANCODE_UP))
      nudgeSelected({0.f, 0.f, -step});
    if (m_input->isKeyJustPressed(SDL_SCANCODE_DOWN))
      nudgeSelected({0.f, 0.f, step});
    if (m_input->isKeyJustPressed(SDL_SCANCODE_PAGEUP))
      nudgeSelected({0.f, step, 0.f});
    if (m_input->isKeyJustPressed(SDL_SCANCODE_PAGEDOWN))
      nudgeSelected({0.f, -step, 0.f});
  }

  if (m_inspector) {
    MapObject *selected = m_mapData.findObject(m_selectedObjectId);
    m_inspector->setSelected(selected);
    m_inspector->update(deltaTime);
  }

  m_ui->setMapName(m_mapData.name);
  m_ui->setObjectCount(static_cast<int>(m_mapData.objects.size()));
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
  SDL_SetRenderDrawColor(m_renderer, 12, 14, 18, 255);
  SDL_RenderClear(m_renderer);

  const int inspectorW = (m_inspector && !m_selectedObjectId.empty())
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
  if (m_dirty) {
    static Uint64 lastWarnTicks = 0;
    const Uint64 now = SDL_GetTicks();
    if (now - lastWarnTicks > 2000) {
      lastWarnTicks = now;
      m_ui->setStatusMessage("Несохранено! Ctrl+N ещё раз чтобы сбросить");
      return;
    }
  }
  m_objectIdCounter = 0;
  m_mapData.createDefault(m_objectIdCounter);
  m_selectedObjectId.clear();
  m_selectedTrackPoint = -1;
  m_currentFilePath = mapsDirectory() + "/untitled.rrmap";
  m_undoStack.clear();
  m_dirty = false;
  m_ui->setStatusMessage("Создана новая карта");
  LOG_INFO("Created new map");
}

void MapEditorApp::saveMap() {
  ensureMapsDirectory();

  int spawnCount = 0;
  int row1 = 0;
  int row2 = 0;
  int finishCount = 0;
  for (const MapObject &o : m_mapData.objects) {
    if (o.type == MapObjectType::SpawnPoint) {
      ++spawnCount;
      if (o.row == 1)
        ++row1;
      else
        ++row2;
    }
    if (o.type == MapObjectType::Checkpoint && o.isFinishLine) {
      ++finishCount;
    }
  }

  if (m_mapData.trackPoints.size() < 2) {
    m_ui->setStatusMessage("Ошибка: трасса должна иметь ≥ 2 точек");
    return;
  }

  const std::string safeName =
      m_mapData.name.empty() ? "untitled" : m_mapData.name;
  std::string fileName = safeName;
  std::replace(fileName.begin(), fileName.end(), ' ', '_');
  m_currentFilePath = mapsDirectory() + "/" + fileName + ".rrmap";

  if (!m_mapData.saveToFile(m_currentFilePath)) {
    m_ui->setStatusMessage("Ошибка сохранения");
    return;
  }

  m_dirty = false;
  refreshMapList();

  std::string msg = "Карта сохранена";
  const bool needsRaceGrid =
      m_mapData.mode == MapMode::Arena || m_mapData.mode == MapMode::Both;
  if (needsRaceGrid) {
    if (spawnCount < 8) {
      msg += " | спавнов " + std::to_string(spawnCount) + "/8";
    } else if (row1 < 4 || row2 < 4) {
      msg += " | ряды " + std::to_string(row1) + "+" + std::to_string(row2) +
             " (нужно 4+4)";
    }
    if (finishCount < 1) {
      msg += " | нет финишного чекпоинта";
    }
  }
  m_ui->setStatusMessage(msg);
}

void MapEditorApp::loadMap(const std::string &path) {
  if (m_dirty) {
    static Uint64 lastWarnTicks = 0;
    const Uint64 now = SDL_GetTicks();
    if (now - lastWarnTicks > 2000) {
      lastWarnTicks = now;
      m_ui->setStatusMessage("Несохранено! Откройте файл ещё раз для сброса");
      return;
    }
  }
  const std::string fullPath = mapsDirectory() + "/" + path;
  if (m_mapData.loadFromFile(fullPath)) {
    m_currentFilePath = fullPath;
    m_selectedObjectId.clear();
    m_selectedTrackPoint = -1;
    m_undoStack.clear();
    m_dirty = false;
    m_ui->setStatusMessage("Карта загружена");
    m_objectIdCounter = 0;
    for (const MapObject &obj : m_mapData.objects) {
      // id вида "spawn_12" → берём максимальный числовой суффикс
      const auto pos = obj.id.rfind('_');
      if (pos == std::string::npos)
        continue;
      try {
        const uint32_t n =
            static_cast<uint32_t>(std::stoul(obj.id.substr(pos + 1)));
        if (n + 1 > m_objectIdCounter) {
          m_objectIdCounter = n + 1;
        }
      } catch (...) {
      }
    }
  } else {
    m_ui->setStatusMessage("Ошибка загрузки");
  }
}

void MapEditorApp::refreshMapList() {
  ensureMapsDirectory();
  std::vector<std::string> files;
  for (const auto &entry :
       std::filesystem::directory_iterator(mapsDirectory())) {
    if (!entry.is_regular_file()) {
      continue;
    }
    if (entry.path().extension() == ".rrmap") {
      files.push_back(entry.path().filename().string());
    }
  }
  std::sort(files.begin(), files.end());
  m_ui->setMapFileList(files);
}

void MapEditorApp::deleteSelected() {
  if (m_ui->getCurrentTool() == EditorTool::EditTrack) {
    if (m_selectedTrackPoint >= 0) {
      deleteTrackPoint(m_selectedTrackPoint);
    }
    return;
  }
  if (m_selectedObjectId.empty()) {
    return;
  }
  pushCommand(std::make_unique<RemoveObjectCommand>(m_selectedObjectId));
  m_ui->setStatusMessage("Объект удалён");
  m_selectedObjectId.clear();
  m_dirty = true;
}

void MapEditorApp::handleViewportClick(const glm::vec2 &screenPos) {
  const int inspectorW = (m_inspector && !m_selectedObjectId.empty())
                             ? m_inspector->panelWidth()
                             : 0;
  const glm::ivec4 viewport = m_ui->getViewportRect(inspectorW);
  const glm::vec2 localPos(screenPos.x - viewport.x, screenPos.y - viewport.y);

  EditorTool tool = m_ui->getCurrentTool();

  if (tool == EditorTool::EditTrack) {
    int idx = findTrackPointNear(localPos);
    if (idx >= 0) {
      m_selectedTrackPoint = idx;
      m_ui->setStatusMessage("Выбрана точка трассы #" + std::to_string(idx));
      return;
    }
    glm::vec3 groundPoint;
    if (!m_camera->screenToGround(localPos, groundPoint)) {
      m_ui->setStatusMessage("Не удалось определить точку на земле");
      return;
    }
    addTrackPointAt(groundPoint);
    return;
  }

  if (tool == EditorTool::Select || tool == EditorTool::Delete) {
    MapObject *object = findObjectNear(localPos);
    if (!object) {
      m_selectedObjectId.clear();
      return;
    }
    if (tool == EditorTool::Delete) {
      m_selectedObjectId = object->id;
      deleteSelected();
    } else {
      m_selectedObjectId = object->id;
      m_ui->setStatusMessage("Выбран объект " + object->id);
    }
    return;
  }

  glm::vec3 groundPoint;
  if (!m_camera->screenToGround(localPos, groundPoint)) {
    m_ui->setStatusMessage("Не удалось определить точку на земле");
    return;
  }

  placeObjectAt(groundPoint);
}

void MapEditorApp::placeObjectAt(const glm::vec3 &position) {
  MapObject object;
  object.position = snapToGrid(position);
  object.position.y = 0.0f;

  switch (m_ui->getCurrentTool()) {
  case EditorTool::PlaceSpawn:
    object.type = MapObjectType::SpawnPoint;
    object.row = m_ui->getSpawnRow();
    object.id = m_mapData.generateObjectId(MapObjectType::SpawnPoint,
                                           m_objectIdCounter);
    break;
  case EditorTool::PlaceCheckpoint:
    object.type = MapObjectType::Checkpoint;
    object.radius = 12.0f;
    object.id = m_mapData.generateObjectId(MapObjectType::Checkpoint,
                                           m_objectIdCounter);
    break;
  case EditorTool::PlacePowerUp:
    object.type = MapObjectType::PowerUpSpawn;
    object.id = m_mapData.generateObjectId(MapObjectType::PowerUpSpawn,
                                           m_objectIdCounter);
    break;
  case EditorTool::PlaceDestructible:
    object.type = MapObjectType::Destructible;
    object.health = 50.0f;
    object.modelPath = "assets/barrel.obj";
    object.id = m_mapData.generateObjectId(MapObjectType::Destructible,
                                           m_objectIdCounter);
    break;
  case EditorTool::PlaceObstacle:
    object.type = MapObjectType::Obstacle;
    object.modelPath = "assets/barrier.obj";
    object.id =
        m_mapData.generateObjectId(MapObjectType::Obstacle, m_objectIdCounter);
    break;
  case EditorTool::PlaceDecoration:
    object.type = MapObjectType::Decoration;
    object.modelPath = "assets/prop.obj";
    object.id = m_mapData.generateObjectId(MapObjectType::Decoration,
                                           m_objectIdCounter);
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
  if (m_mapData.trackPoints.empty()) {
    point.width = 14.0f;
  } else {
    point.width = m_mapData.trackPoints.back().width;
  }
  pushCommand(std::make_unique<AddTrackPointCommand>(point));
  m_selectedTrackPoint = static_cast<int>(m_mapData.trackPoints.size()) - 1;
  m_ui->setStatusMessage("Добавлена точка трассы #" +
                         std::to_string(m_selectedTrackPoint));
  m_dirty = true;
}

void MapEditorApp::deleteTrackPoint(int index) {
  if (index < 0 || index >= static_cast<int>(m_mapData.trackPoints.size())) {
    return;
  }
  const size_t safeIndex = static_cast<size_t>(index);
  pushCommand(std::make_unique<RemoveTrackPointCommand>(safeIndex));
  if (m_selectedTrackPoint == index) {
    m_selectedTrackPoint = -1;
  } else if (m_selectedTrackPoint > index) {
    --m_selectedTrackPoint;
  }
  m_ui->setStatusMessage("Удалена точка трассы #" + std::to_string(index));
  m_dirty = true;
}

void MapEditorApp::adjustSelectedTrackWidth(float delta) {
  if (m_selectedTrackPoint < 0 ||
      m_selectedTrackPoint >= static_cast<int>(m_mapData.trackPoints.size())) {
    return;
  }
  const size_t idx = static_cast<size_t>(m_selectedTrackPoint);
  const float oldW = m_mapData.trackPoints[idx].width;
  const float newW = std::clamp(oldW + delta, 4.0f, 40.0f);
  if (std::abs(newW - oldW) < 1e-4f) {
    return;
  }
  pushCommand(std::make_unique<SetTrackPointWidthCommand>(idx, oldW, newW));
  m_dirty = true;
  m_ui->setStatusMessage("Ширина точки #" +
                         std::to_string(m_selectedTrackPoint) + ": " +
                         std::to_string(newW) + " м");
}

void MapEditorApp::insertTrackPointAfterSelected() {
  if (m_selectedTrackPoint < 0 ||
      m_selectedTrackPoint >= static_cast<int>(m_mapData.trackPoints.size())) {
    return;
  }
  const size_t idx = static_cast<size_t>(m_selectedTrackPoint);
  const size_t n = m_mapData.trackPoints.size();
  if (!m_mapData.trackClosed && idx + 1 >= n) {
    m_ui->setStatusMessage(
        "Нельзя вставить после последней точки открытой трассы");
    return;
  }
  const size_t nextIdx = m_mapData.trackClosed ? (idx + 1) % n : (idx + 1);

  const TrackPoint &a = m_mapData.trackPoints[idx];
  const TrackPoint &b = m_mapData.trackPoints[nextIdx];

  TrackPoint mid;
  mid.position = snapToGrid((a.position + b.position) * 0.5f);
  mid.width = (a.width + b.width) * 0.5f;

  const size_t insertAt = idx + 1;
  pushCommand(std::make_unique<InsertTrackPointCommand>(insertAt, mid));
  m_selectedTrackPoint = static_cast<int>(insertAt);
  m_dirty = true;
  m_ui->setStatusMessage("Вставлена точка трассы #" +
                         std::to_string(m_selectedTrackPoint));
}

void MapEditorApp::duplicateSelectedObject() {
  if (m_selectedObjectId.empty()) {
    return;
  }
  const MapObject *src = m_mapData.findObject(m_selectedObjectId);
  if (!src) {
    return;
  }
  MapObject copy = *src;
  copy.id = m_mapData.generateObjectId(src->type, m_objectIdCounter);
  copy.position = snapToGrid(src->position + glm::vec3(m_gridSize, 0.0f, 0.0f));
  pushCommand(std::make_unique<AddObjectCommand>(copy));
  m_selectedObjectId = copy.id;
  m_dirty = true;
  m_ui->setStatusMessage("Дубликат: " + copy.id);
}

void MapEditorApp::nudgeSelected(const glm::vec3 &delta) {
  if (!m_selectedObjectId.empty()) {
    MapObject *o = m_mapData.findObject(m_selectedObjectId);
    if (!o)
      return;
    const glm::vec3 oldPos = o->position;
    glm::vec3 newPos = snapToGrid(oldPos + delta);
    if (glm::dot(newPos - oldPos, newPos - oldPos) < 1e-8f)
      return;
    pushCommand(std::make_unique<MoveObjectCommand>(m_selectedObjectId, oldPos,
                                                    newPos));
    m_dirty = true;
    return;
  }
  if (m_selectedTrackPoint >= 0 &&
      m_selectedTrackPoint < static_cast<int>(m_mapData.trackPoints.size())) {
    const size_t idx = static_cast<size_t>(m_selectedTrackPoint);
    const glm::vec3 oldPos = m_mapData.trackPoints[idx].position;
    glm::vec3 newPos = snapToGrid(oldPos + delta);
    if (glm::dot(newPos - oldPos, newPos - oldPos) < 1e-8f)
      return;
    pushCommand(std::make_unique<MoveTrackPointCommand>(idx, oldPos, newPos));
    m_dirty = true;
  }
}

void MapEditorApp::cycleSelection(bool reverse) {
  if (m_ui->getCurrentTool() == EditorTool::EditTrack) {
    const int n = static_cast<int>(m_mapData.trackPoints.size());
    if (n == 0)
      return;
    if (m_selectedTrackPoint < 0) {
      m_selectedTrackPoint = reverse ? n - 1 : 0;
    } else {
      m_selectedTrackPoint = reverse ? (m_selectedTrackPoint - 1 + n) % n
                                     : (m_selectedTrackPoint + 1) % n;
    }
    m_selectedObjectId.clear();
    m_ui->setStatusMessage("Точка трассы #" +
                           std::to_string(m_selectedTrackPoint));
    return;
  }
  if (m_mapData.objects.empty())
    return;
  int idx = -1;
  for (size_t i = 0; i < m_mapData.objects.size(); ++i) {
    if (m_mapData.objects[i].id == m_selectedObjectId) {
      idx = static_cast<int>(i);
      break;
    }
  }
  const int n = static_cast<int>(m_mapData.objects.size());
  idx = (idx < 0) ? (reverse ? n - 1 : 0)
                  : (reverse ? (idx - 1 + n) % n : (idx + 1) % n);
  m_selectedObjectId = m_mapData.objects[static_cast<size_t>(idx)].id;
  m_selectedTrackPoint = -1;
  m_ui->setStatusMessage("Выбран: " + m_selectedObjectId);
}

void MapEditorApp::placeRaceStartGrid() {
  if (m_mapData.trackPoints.empty()) {
    m_ui->setStatusMessage("Нужна хотя бы одна точка трассы (старт)");
    return;
  }

  // Удаляем старые спавны через команды — чтобы работал undo одним стеком
  // неудобно, поэтому снимаем спавны напрямую и пушим только добавления (один
  // undo-шаг на каждый). Для чистого UX: сначала удаляем без undo, потом
  // добавляем 8 с undo.
  std::vector<std::string> toRemove;
  for (const MapObject &o : m_mapData.objects) {
    if (o.type == MapObjectType::SpawnPoint) {
      toRemove.push_back(o.id);
    }
  }
  for (const std::string &id : toRemove) {
    pushCommand(std::make_unique<RemoveObjectCommand>(id));
  }

  const glm::vec3 start = m_mapData.trackPoints[0].position;
  glm::vec3 forward(0.f, 0.f, 1.f);
  if (m_mapData.trackPoints.size() >= 2) {
    forward = glm::normalize(m_mapData.trackPoints[1].position - start);
  }
  glm::vec3 right =
      glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
  if (glm::length(right) < 1e-3f) {
    right = glm::vec3(1.f, 0.f, 0.f);
  }

  constexpr float kColSpacing = 6.0f;
  constexpr float kRowSpacing = 8.0f;
  constexpr int kCols = 4;

  for (int row = 1; row <= 2; ++row) {
    for (int col = 0; col < kCols; ++col) {
      const float lateral = (col - 1.5f) * kColSpacing;
      const float along = -(row - 1) * kRowSpacing; // ряд 1 ближе к старту
      MapObject spawn;
      spawn.type = MapObjectType::SpawnPoint;
      spawn.row = row;
      spawn.id = m_mapData.generateObjectId(MapObjectType::SpawnPoint,
                                            m_objectIdCounter);
      spawn.position = snapToGrid(start + right * lateral + forward * along);
      spawn.position.y = 0.0f;
      pushCommand(std::make_unique<AddObjectCommand>(spawn));
    }
  }

  m_dirty = true;
  m_ui->setStatusMessage("Стартовая сетка 4+4 расставлена у точки #0");
}

void MapEditorApp::toggleTrackClosed() {
  pushCommand(std::make_unique<SetTrackClosedCommand>(m_mapData.trackClosed,
                                                      !m_mapData.trackClosed));
  m_ui->setStatusMessage(m_mapData.trackClosed ? "Трасса замкнута"
                                               : "Трасса открыта");
  m_dirty = true;
}

void MapEditorApp::selectTrackPointNear(const glm::vec2 &screenPos) {
  m_selectedTrackPoint = findTrackPointNear(screenPos);
}

void MapEditorApp::selectObjectAt(const glm::vec2 &screenPos) {
  MapObject *object = findObjectNear(screenPos);
  m_selectedObjectId = object ? object->id : "";
}

MapObject *MapEditorApp::findObjectNear(const glm::vec2 &screenPos,
                                        float maxDistance) {
  MapObject *closest = nullptr;
  float closestDistance = maxDistance;

  for (MapObject &object : m_mapData.objects) {
    const glm::vec2 projected = m_camera->worldToScreen(object.position);
    if (projected.x < 0.0f) {
      continue;
    }
    const float distance = glm::length(projected - screenPos);
    if (distance < closestDistance) {
      closestDistance = distance;
      closest = &object;
    }
  }

  return closest;
}

int MapEditorApp::findTrackPointNear(const glm::vec2 &screenPos,
                                     float maxDistance) const {
  int closestIdx = -1;
  float closestDistance = maxDistance;

  for (size_t i = 0; i < m_mapData.trackPoints.size(); ++i) {
    const glm::vec2 projected =
        m_camera->worldToScreen(m_mapData.trackPoints[i].position);
    if (projected.x < 0.0f) {
      continue;
    }
    const float distance = glm::length(projected - screenPos);
    if (distance < closestDistance) {
      closestDistance = distance;
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
  if (!m_gridSnapEnabled || m_gridSize <= 1e-3f) {
    return p;
  }
  const float g = m_gridSize;
  return glm::vec3(std::round(p.x / g) * g, p.y, std::round(p.z / g) * g);
}

void MapEditorApp::undo() {
  if (!m_undoStack.canUndo()) {
    return;
  }
  const std::string label = m_undoStack.undoLabel();
  m_undoStack.undo(m_mapData);
  m_ui->setStatusMessage("Отменено: " + label);
  m_dirty = true;
}

void MapEditorApp::redo() {
  if (!m_undoStack.canRedo()) {
    return;
  }
  const std::string label = m_undoStack.redoLabel();
  m_undoStack.redo(m_mapData);
  m_ui->setStatusMessage("Возврат: " + label);
  m_dirty = true;
}

void MapEditorApp::onInspectorFieldChanged(const std::string &id,
                                           int fieldIndex,
                                           const std::string &value) {
  MapObject *obj = m_mapData.findObject(id);
  if (!obj)
    return;

  auto pushSet = [&](SetObjectFieldCommand::Field field) {
    std::string oldValue;
    switch (field) {
    case SetObjectFieldCommand::Field::Radius:
      oldValue = std::to_string(obj->radius);
      break;
    case SetObjectFieldCommand::Field::Health:
      oldValue = std::to_string(obj->health);
      break;
    case SetObjectFieldCommand::Field::Row:
      oldValue = std::to_string(obj->row);
      break;
    case SetObjectFieldCommand::Field::ModelPath:
      oldValue = obj->modelPath;
      break;
    case SetObjectFieldCommand::Field::IsFinishLine:
      oldValue = obj->isFinishLine ? "true" : "false";
      break;
    case SetObjectFieldCommand::Field::Subtype:
      oldValue = std::to_string(obj->subtype);
      break;
    }
    if (oldValue == value)
      return;
    pushCommand(
        std::make_unique<SetObjectFieldCommand>(id, field, oldValue, value));
    m_dirty = true;
  };

  // Field indices match the order of addFloat/addInt/addBool/addString calls in
  // PropertyInspector::rebuildRows().
  switch (fieldIndex) {
  case 2:
    pushSet(SetObjectFieldCommand::Field::Radius);
    break;
  case 3:
    pushSet(SetObjectFieldCommand::Field::Row);
    break;
  case 4:
    pushSet(SetObjectFieldCommand::Field::IsFinishLine);
    break;
  case 5:
    pushSet(SetObjectFieldCommand::Field::Health);
    break;
  case 6:
    pushSet(SetObjectFieldCommand::Field::ModelPath);
    break;
  case 10: // subtype, только если power-up и поле добавлено в конец
    pushSet(SetObjectFieldCommand::Field::Subtype);
    break;
  default:
    break; // id, type, pos x/y/z are read-only / handled below
  }
  // pos x / pos y / pos z → MoveObjectCommand
  if (fieldIndex >= 7 && fieldIndex <= 9) {
    glm::vec3 newPos = obj->position;
    try {
      const float v = std::stof(value);
      if (fieldIndex == 7)
        newPos.x = v;
      else if (fieldIndex == 8)
        newPos.y = v;
      else
        newPos.z = v;
    } catch (...) {
      return;
    }
    onInspectorMoveObject(id, newPos);
  }
}

void MapEditorApp::onInspectorMoveObject(const std::string &id,
                                         const glm::vec3 &position) {
  MapObject *obj = m_mapData.findObject(id);
  if (!obj)
    return;
  glm::vec3 snapped = snapToGrid(position);
  const glm::vec3 delta = obj->position - snapped;
  if (glm::dot(delta, delta) < 1e-6f)
    return;
  pushCommand(std::make_unique<MoveObjectCommand>(id, obj->position, snapped));
  m_dirty = true;
}

} // namespace Editor
