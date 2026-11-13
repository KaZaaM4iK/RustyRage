#include "Editor/Shared/EditorUI.hpp"
#include "Core/Logging.hpp"

#include <algorithm>
#include <cstdio>

namespace Editor {

EditorUI::EditorUI()
    : m_renderer(nullptr), m_font(nullptr), m_fontSmall(nullptr),
      m_windowWidth(1600), m_windowHeight(900), m_toolbarHeight(72),
      m_sidePanelWidth(230), m_workspaceMode(EditorWorkspaceMode::TrackAndArena),
      m_currentTool(EditorTool::Select), m_spawnRow(1),
      m_statusMessage("Готов"), m_currentFilePath("maps/untitled.rrmap"),
      m_mapName("New Map"), m_objectCount(0), m_structureCount(0) {}

EditorUI::~EditorUI() { shutdown(); }

bool EditorUI::initialize(SDL_Renderer *renderer, int windowWidth,
                          int windowHeight) {
  m_renderer = renderer;
  m_windowWidth = windowWidth;
  m_windowHeight = windowHeight;

  if (TTF_Init() < 0) {
    LOG_ERRORF("TTF_Init failed: %s", SDL_GetError());
    return false;
  }

  const char *fontPaths[] = {"assets/fonts/DejaVuSans.ttf",
                             "../assets/fonts/DejaVuSans.ttf",
                             "../../assets/fonts/DejaVuSans.ttf"};

  for (const char *path : fontPaths) {
    m_font = TTF_OpenFont(path, 18);
    m_fontSmall = TTF_OpenFont(path, 14);
    if (m_font && m_fontSmall) {
      break;
    }
  }

  if (!m_font || !m_fontSmall) {
    LOG_WARNING("Editor UI running without fonts");
  }

  rebuildButtons();
  return true;
}

void EditorUI::shutdown() {
  if (m_fontSmall) {
    TTF_CloseFont(m_fontSmall);
    m_fontSmall = nullptr;
  }
  if (m_font) {
    TTF_CloseFont(m_font);
    m_font = nullptr;
  }
  TTF_Quit();
}

void EditorUI::onWindowResize(int windowWidth, int windowHeight) {
  m_windowWidth = windowWidth;
  m_windowHeight = windowHeight;
  rebuildButtons();
}

void EditorUI::update(float /*deltaTime*/) {}

void EditorUI::setMousePosition(const glm::vec2 &position) {
  m_mousePosition = position;
}

void EditorUI::render() { renderPanel(); }

void EditorUI::setWorkspaceMode(EditorWorkspaceMode mode) {
  m_workspaceMode = mode;
  rebuildButtons();
}

void EditorUI::setCurrentTool(EditorTool tool) {
  m_currentTool = tool;
  rebuildButtons();
}

void EditorUI::setStatusMessage(const std::string &message) {
  m_statusMessage = message;
}

void EditorUI::setCurrentFilePath(const std::string &path) {
  m_currentFilePath = path;
}

void EditorUI::setMapName(const std::string &name) { m_mapName = name; }

void EditorUI::setObjectCount(int count) { m_objectCount = count; }

void EditorUI::setSelectedObjectId(const std::string &id) {
  m_selectedObjectId = id;
}

void EditorUI::setMapFileList(const std::vector<std::string> &files) {
  m_mapFileList = files;
  rebuildButtons();
}

void EditorUI::setUndoLabel(const std::string &label) { m_undoLabel = label; }

void EditorUI::setRedoLabel(const std::string &label) { m_redoLabel = label; }

void EditorUI::setGridInfo(float size, bool enabled) {
  m_gridSize = size;
  m_gridSnap = enabled;
}

void EditorUI::setDirty(bool dirty) { m_dirty = dirty; }

bool EditorUI::isMouseOverUI(const glm::vec2 &mousePos) const {
  return mousePos.x < static_cast<float>(m_sidePanelWidth) ||
         mousePos.y > static_cast<float>(m_windowHeight - m_toolbarHeight);
}

glm::ivec4 EditorUI::getViewportRect(int inspectorPanelWidth) const {
  const int rightInset = std::max(0, inspectorPanelWidth);
  return glm::ivec4(m_sidePanelWidth, 0,
                    std::max(1, m_windowWidth - m_sidePanelWidth - rightInset),
                    std::max(1, m_windowHeight - m_toolbarHeight));
}

void EditorUI::handleMouseClick(const glm::vec2 &position) {
  for (const Button &button : m_buttons) {
    if (isPointInRect(position, button.position, button.size) &&
        button.onClick) {
      button.onClick();
      return;
    }
  }
}

void EditorUI::rebuildButtons() {
  m_buttons.clear();

  const float buttonWidth = 190.0f;
  const float buttonHeight = 32.0f;
  float y = 14.0f;

  auto addButton = [&](const std::string &id, const std::string &label,
                       std::function<void()> onClick, bool highlighted, bool active = false) {
    Button button;
    button.id = id;
    button.label = label;
    button.position = glm::vec2(20.0f, y);
    button.size = glm::vec2(buttonWidth, buttonHeight);
    button.onClick = std::move(onClick);
    button.highlighted = highlighted;
    button.active = active;
    m_buttons.push_back(button);
    y += buttonHeight + 6.0f;
  };

  // Top Workspace Modes Tab Buttons
  addButton("mode_tracks", "[1] Трассы & Арена", [this]() {
    if (m_onSwitchMode) m_onSwitchMode(EditorWorkspaceMode::TrackAndArena);
    else setWorkspaceMode(EditorWorkspaceMode::TrackAndArena);
  }, m_workspaceMode == EditorWorkspaceMode::TrackAndArena, true);

  addButton("mode_base", "[2] Базы & Строения", [this]() {
    if (m_onSwitchMode) m_onSwitchMode(EditorWorkspaceMode::BaseArchitect);
    else setWorkspaceMode(EditorWorkspaceMode::BaseArchitect);
  }, m_workspaceMode == EditorWorkspaceMode::BaseArchitect, true);

  addButton("mode_ugc", "[3] UGC Логика & Правила", [this]() {
    if (m_onSwitchMode) m_onSwitchMode(EditorWorkspaceMode::UgcLogicStudio);
    else setWorkspaceMode(EditorWorkspaceMode::UgcLogicStudio);
  }, m_workspaceMode == EditorWorkspaceMode::UgcLogicStudio, true);

  y += 6.0f;
  addButton("new", "Создать Новую Карту", m_onNewMap, false);
  addButton("save", "Сохранить (Ctrl+S)", m_onSaveMap, false);
  addButton("export_ugc", "Экспорт UGC Мода", [this]() {
    if (m_onExportUgcPackage) m_onExportUgcPackage();
  }, false);

  y += 6.0f;

  if (m_workspaceMode == EditorWorkspaceMode::TrackAndArena) {
    addButton("tool_select", "Выбор (1)", [this]() { m_currentTool = EditorTool::Select; rebuildButtons(); }, m_currentTool == EditorTool::Select);
    addButton("tool_track", "Сплайн Трассы (2)", [this]() { m_currentTool = EditorTool::EditTrack; rebuildButtons(); }, m_currentTool == EditorTool::EditTrack);
    addButton("tool_spawn", "Спавн Машин (3)", [this]() { m_currentTool = EditorTool::PlaceSpawn; rebuildButtons(); }, m_currentTool == EditorTool::PlaceSpawn);
    addButton("tool_checkpoint", "Чекпоинт (4)", [this]() { m_currentTool = EditorTool::PlaceCheckpoint; rebuildButtons(); }, m_currentTool == EditorTool::PlaceCheckpoint);
    addButton("tool_powerup", "Оружие / Бонус (5)", [this]() { m_currentTool = EditorTool::PlacePowerUp; rebuildButtons(); }, m_currentTool == EditorTool::PlacePowerUp);
    addButton("tool_jump", "Трамплин / Джамппад (6)", [this]() { m_currentTool = EditorTool::PlaceJumpPad; rebuildButtons(); }, m_currentTool == EditorTool::PlaceJumpPad);
    addButton("tool_hazard", "Дробилка / Ловушка (7)", [this]() { m_currentTool = EditorTool::PlaceHazard; rebuildButtons(); }, m_currentTool == EditorTool::PlaceHazard);
    addButton("tool_delete", "Удалить (Del)", [this]() { m_currentTool = EditorTool::Delete; rebuildButtons(); }, m_currentTool == EditorTool::Delete);

    y += 4.0f;
    addButton("track_closed", "Замкнуть кольцо трассы", m_onToggleTrackClosed, false);
    addButton("race_grid", "Сетка старта 4+4", [this]() { if (m_onRaceGrid) m_onRaceGrid(); }, false);
  } else if (m_workspaceMode == EditorWorkspaceMode::BaseArchitect) {
    addButton("tool_select", "Выбор (1)", [this]() { m_currentTool = EditorTool::Select; rebuildButtons(); }, m_currentTool == EditorTool::Select);
    addButton("bld_wall", "Стена / Баррикада (2)", [this]() { m_currentTool = EditorTool::PlaceStructureWall; rebuildButtons(); }, m_currentTool == EditorTool::PlaceStructureWall);
    addButton("bld_turret", "Авто-Турель 20мм (3)", [this]() { m_currentTool = EditorTool::PlaceStructureTurret; rebuildButtons(); }, m_currentTool == EditorTool::PlaceStructureTurret);
    addButton("bld_repair", "Эстакада Ремонта (4)", [this]() { m_currentTool = EditorTool::PlaceStructureUtility; rebuildButtons(); }, m_currentTool == EditorTool::PlaceStructureUtility);
    addButton("bld_gen", "Дизель-Генератор (5)", [this]() { m_currentTool = EditorTool::PlaceStructurePower; rebuildButtons(); }, m_currentTool == EditorTool::PlaceStructurePower);
    addButton("tool_delete", "Снести Объект (Del)", [this]() { m_currentTool = EditorTool::Delete; rebuildButtons(); }, m_currentTool == EditorTool::Delete);
  } else if (m_workspaceMode == EditorWorkspaceMode::UgcLogicStudio) {
    addButton("tool_select", "Выбор Триггера (1)", [this]() { m_currentTool = EditorTool::Select; rebuildButtons(); }, m_currentTool == EditorTool::Select);
    addButton("ugc_zone", "Создать Триггер-Зону (2)", [this]() { m_currentTool = EditorTool::PlaceTriggerZone; rebuildButtons(); }, m_currentTool == EditorTool::PlaceTriggerZone);
    addButton("ugc_add_airdrop", "+ Сброс Аирдропа", [this]() {
      setStatusMessage("Добавлено UGC правило: Аирдроп по таймеру 45с");
    }, false);
    addButton("ugc_add_boss", "+ Рейд Банды Босса", [this]() {
      setStatusMessage("Добавлено UGC правило: Спавн Тяжелого Босса");
    }, false);
    addButton("tool_delete", "Удалить Правило (Del)", [this]() { m_currentTool = EditorTool::Delete; rebuildButtons(); }, m_currentTool == EditorTool::Delete);
  }

  y += 6.0f;
  const size_t maxVisibleMaps = 8;
  size_t shown = 0;
  for (const std::string &file : m_mapFileList) {
    if (shown >= maxVisibleMaps) {
      break;
    }
    ++shown;
    addButton(
        "load_" + file, "Карта: " + file,
        [this, file]() {
          if (m_onLoadMap) {
            m_onLoadMap(file);
          }
        },
        false);
  }
}

void EditorUI::renderPanel() {
  SDL_FRect sidePanel{0.0f, 0.0f, static_cast<float>(m_sidePanelWidth),
                      static_cast<float>(m_windowHeight)};
  SDL_SetRenderDrawColor(m_renderer, 24, 28, 38, 255);
  SDL_RenderFillRect(m_renderer, &sidePanel);

  SDL_FRect toolbar{0.0f, static_cast<float>(m_windowHeight - m_toolbarHeight),
                    static_cast<float>(m_windowWidth),
                    static_cast<float>(m_toolbarHeight)};
  SDL_SetRenderDrawColor(m_renderer, 18, 22, 30, 255);
  SDL_RenderFillRect(m_renderer, &toolbar);

  const glm::vec2 mousePos = m_mousePosition;
  for (const Button &button : m_buttons) {
    renderButton(button, mousePos);
  }

  if (m_font) {
    char header[160];
    std::snprintf(header, sizeof(header), "RustyRage AAA Creator Studio%s",
                  m_dirty ? "  *" : "");
    renderText(header, 20.0f, static_cast<float>(m_windowHeight - 64.0f),
               {230, 230, 245, 255}, m_font);
    
    renderText("Проект: " + m_mapName, 360.0f,
               static_cast<float>(m_windowHeight - 64.0f), {190, 210, 235, 255},
               m_fontSmall);
    renderText("Файл: " + m_currentFilePath, 360.0f,
               static_cast<float>(m_windowHeight - 42.0f), {140, 160, 180, 255},
               m_fontSmall);

    char statsBuf[180];
    std::snprintf(statsBuf, sizeof(statsBuf), "Объектов: %d | Строений: %d | Сплайнов: %d | Режим: %s",
                  m_objectCount, m_structureCount, m_trackPointCount, m_mapMode.c_str());
    renderText(statsBuf, 780.0f,
               static_cast<float>(m_windowHeight - 64.0f), {170, 215, 240, 255},
               m_fontSmall);

    renderText("Инструмент: " + toolLabel(m_currentTool), 780.0f,
               static_cast<float>(m_windowHeight - 42.0f), {200, 210, 220, 255},
               m_fontSmall);

    if (!m_selectedObjectId.empty()) {
      renderText("Выбрано: " + m_selectedObjectId, 1200.0f,
                 static_cast<float>(m_windowHeight - 64.0f),
                 {255, 225, 110, 255}, m_fontSmall);
    }
    renderText(m_statusMessage, 1200.0f,
               static_cast<float>(m_windowHeight - 42.0f), {130, 235, 140, 255},
               m_fontSmall);

    std::string undoText = "Undo: " + (m_undoLabel.empty() ? "—" : m_undoLabel);
    std::string redoText = "Redo: " + (m_redoLabel.empty() ? "—" : m_redoLabel);
    renderText(undoText, 20.0f, static_cast<float>(m_windowHeight - 24.0f),
               {210, 190, 130, 255}, m_fontSmall);
    renderText(redoText, 220.0f, static_cast<float>(m_windowHeight - 24.0f),
               {210, 190, 130, 255}, m_fontSmall);

    char gridBuf[100];
    std::snprintf(gridBuf, sizeof(gridBuf), "Сетка: %s, шаг %.2f м | 1-3 Режимы Студии | Tab Фокус",
                  m_gridSnap ? "ВКЛ" : "ВЫКЛ", m_gridSize);
    renderText(gridBuf, 440.0f, static_cast<float>(m_windowHeight - 24.0f),
               {150, 190, 210, 255}, m_fontSmall);
  }
}

void EditorUI::renderButton(const Button &button, const glm::vec2 &mousePos) {
  const bool hovered = isPointInRect(mousePos, button.position, button.size);
  if (button.highlighted) {
    SDL_SetRenderDrawColor(m_renderer, 60, 130, 210, 255);
  } else if (button.active) {
    SDL_SetRenderDrawColor(m_renderer, 45, 75, 110, 255);
  } else if (hovered) {
    SDL_SetRenderDrawColor(m_renderer, 50, 60, 75, 255);
  } else {
    SDL_SetRenderDrawColor(m_renderer, 32, 38, 48, 255);
  }

  SDL_FRect rect{button.position.x, button.position.y, button.size.x,
                 button.size.y};
  SDL_RenderFillRect(m_renderer, &rect);

  SDL_SetRenderDrawColor(m_renderer, button.highlighted ? 110 : 70, button.highlighted ? 180 : 90, button.highlighted ? 255 : 110, 255);
  SDL_RenderRect(m_renderer, &rect);

  if (m_fontSmall) {
    renderText(button.label, button.position.x + 8.0f,
               button.position.y + 7.0f, {235, 235, 245, 255}, m_fontSmall);
  }
}

void EditorUI::renderText(const std::string &text, float x, float y,
                          SDL_Color color, TTF_Font *font) {
  if (!font) {
    return;
  }
  SDL_Surface *surface =
      TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
  if (!surface) {
    return;
  }
  SDL_Texture *texture = SDL_CreateTextureFromSurface(m_renderer, surface);
  if (!texture) {
    SDL_DestroySurface(surface);
    return;
  }
  SDL_FRect dst{x, y, static_cast<float>(surface->w),
                static_cast<float>(surface->h)};
  SDL_RenderTexture(m_renderer, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
  SDL_DestroySurface(surface);
}

bool EditorUI::isPointInRect(const glm::vec2 &point, const glm::vec2 &pos,
                             const glm::vec2 &size) const {
  return point.x >= pos.x && point.x <= pos.x + size.x && point.y >= pos.y &&
         point.y <= pos.y + size.y;
}

std::string EditorUI::toolLabel(EditorTool tool) const {
  switch (tool) {
  case EditorTool::Select: return "Выбор";
  case EditorTool::EditTrack: return "Сплайн Трассы";
  case EditorTool::PlaceSpawn: return "Спавн Машин";
  case EditorTool::PlaceCheckpoint: return "Чекпоинт";
  case EditorTool::PlacePowerUp: return "Оружие / Бонус";
  case EditorTool::PlaceStructureWall: return "Стена / Баррикада";
  case EditorTool::PlaceStructureTurret: return "Авто-Турель";
  case EditorTool::PlaceStructureUtility: return "Станция Ремонта";
  case EditorTool::PlaceStructurePower: return "Генератор Энергии";
  case EditorTool::PlaceTriggerZone: return "UGC Триггер-Зона";
  case EditorTool::PlaceHazard: return "Ловушка / Дробилка";
  case EditorTool::PlaceJumpPad: return "Трамплин";
  case EditorTool::Delete: return "Удаление / Снос";
  default: return "Выбор";
  }
}

} // namespace Editor
