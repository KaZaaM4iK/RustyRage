#include "Editor/Shared/EditorUI.hpp"
#include "Core/Logging.hpp"

#include <algorithm>
#include <cstdio>

namespace Editor {

EditorUI::EditorUI()
    : m_renderer(nullptr), m_font(nullptr), m_fontSmall(nullptr),
      m_windowWidth(1600), m_windowHeight(900), m_toolbarHeight(72),
      m_sidePanelWidth(220), m_currentTool(EditorTool::Select), m_spawnRow(1),
      m_statusMessage("Готов"), m_currentFilePath("maps/untitled.rrmap"),
      m_mapName("New Map"), m_objectCount(0) {}

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

  const float buttonWidth = 180.0f;
  const float buttonHeight = 34.0f;
  float y = 16.0f;

  auto addButton = [&](const std::string &id, const std::string &label,
                       std::function<void()> onClick, bool highlighted) {
    Button button;
    button.id = id;
    button.label = label;
    button.position = glm::vec2(20.0f, y);
    button.size = glm::vec2(buttonWidth, buttonHeight);
    button.onClick = std::move(onClick);
    button.highlighted = highlighted;
    m_buttons.push_back(button);
    y += buttonHeight + 8.0f;
  };

  addButton("new", "Новая карта", m_onNewMap, false);
  addButton("save", "Сохранить", m_onSaveMap, false);

  y += 8.0f;
  addButton(
      "tool_select", "Выбор (1)",
      [this]() {
        m_currentTool = EditorTool::Select;
        rebuildButtons();
      },
      m_currentTool == EditorTool::Select);
  addButton(
      "tool_spawn", "Спавн (2)",
      [this]() {
        m_currentTool = EditorTool::PlaceSpawn;
        rebuildButtons();
      },
      m_currentTool == EditorTool::PlaceSpawn);
  addButton(
      "tool_checkpoint", "Чекпоинт (3)",
      [this]() {
        m_currentTool = EditorTool::PlaceCheckpoint;
        rebuildButtons();
      },
      m_currentTool == EditorTool::PlaceCheckpoint);
  addButton(
      "tool_powerup", "Пауэр-ап (4)",
      [this]() {
        m_currentTool = EditorTool::PlacePowerUp;
        rebuildButtons();
      },
      m_currentTool == EditorTool::PlacePowerUp);
  addButton(
      "tool_destructible", "Разрушаемое (5)",
      [this]() {
        m_currentTool = EditorTool::PlaceDestructible;
        rebuildButtons();
      },
      m_currentTool == EditorTool::PlaceDestructible);
  addButton(
      "tool_obstacle", "Препятствие (7)",
      [this]() {
        m_currentTool = EditorTool::PlaceObstacle;
        rebuildButtons();
      },
      m_currentTool == EditorTool::PlaceObstacle);
  addButton(
      "tool_decoration", "Декор (8)",
      [this]() {
        m_currentTool = EditorTool::PlaceDecoration;
        rebuildButtons();
      },
      m_currentTool == EditorTool::PlaceDecoration);
  addButton(
      "tool_track", "Трасса (6)",
      [this]() {
        m_currentTool = EditorTool::EditTrack;
        rebuildButtons();
      },
      m_currentTool == EditorTool::EditTrack);
  addButton(
      "tool_delete", "Удалить (Del)",
      [this]() {
        m_currentTool = EditorTool::Delete;
        rebuildButtons();
      },
      m_currentTool == EditorTool::Delete);

  y += 8.0f;
  addButton(
      "row1", "Ряд 1",
      [this]() {
        m_spawnRow = 1;
        rebuildButtons();
      },
      m_spawnRow == 1);
  addButton(
      "row2", "Ряд 2",
      [this]() {
        m_spawnRow = 2;
        rebuildButtons();
      },
      m_spawnRow == 2);
  addButton("delete_selected", "Удалить выбранное", m_onDeleteSelected, false);
  addButton("track_closed", "Замкнуть/разомкнуть трассу", m_onToggleTrackClosed,
            false);
  addButton(
      "race_grid", "Сетка старта 4+4 (Ctrl+R)",
      [this]() {
        if (m_onRaceGrid)
          m_onRaceGrid();
      },
      false);

  y += 8.0f;
  const size_t maxVisibleMaps = 12;
  size_t shown = 0;
  for (const std::string &file : m_mapFileList) {
    if (shown >= maxVisibleMaps) {
      break;
    }
    ++shown;
    addButton(
        "load_" + file, "Открыть: " + file,
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
  SDL_SetRenderDrawColor(m_renderer, 28, 32, 40, 255);
  SDL_RenderFillRect(m_renderer, &sidePanel);

  SDL_FRect toolbar{0.0f, static_cast<float>(m_windowHeight - m_toolbarHeight),
                    static_cast<float>(m_windowWidth),
                    static_cast<float>(m_toolbarHeight)};
  SDL_SetRenderDrawColor(m_renderer, 24, 28, 36, 255);
  SDL_RenderFillRect(m_renderer, &toolbar);

  const glm::vec2 mousePos = m_mousePosition;
  for (const Button &button : m_buttons) {
    renderButton(button, mousePos);
  }

  if (m_font) {
    char header[160];
    std::snprintf(header, sizeof(header), "Редактор карт RustyRage%s",
                  m_dirty ? "  *" : "");
    renderText(header, 20.0f, static_cast<float>(m_windowHeight - 62.0f),
               {220, 220, 230, 255}, m_font);
    renderText("Карта: " + m_mapName, 360.0f,
               static_cast<float>(m_windowHeight - 62.0f), {180, 200, 220, 255},
               m_fontSmall);
    renderText("Файл: " + m_currentFilePath, 360.0f,
               static_cast<float>(m_windowHeight - 40.0f), {140, 160, 180, 255},
               m_fontSmall);
    renderText("Объектов: " + std::to_string(m_objectCount), 900.0f,
               static_cast<float>(m_windowHeight - 62.0f), {180, 200, 220, 255},
               m_fontSmall);
    renderText("Режим: " + m_mapMode +
                   " | Кругов: " + std::to_string(m_lapCount) +
                   " | Точек: " + std::to_string(m_trackPointCount),
               900.0f, static_cast<float>(m_windowHeight - 18.0f),
               {160, 190, 210, 255}, m_fontSmall);
    renderText("Инструмент: " + toolLabel(m_currentTool), 900.0f,
               static_cast<float>(m_windowHeight - 40.0f), {180, 200, 220, 255},
               m_fontSmall);
    if (!m_selectedObjectId.empty()) {
      renderText("Выбрано: " + m_selectedObjectId, 1120.0f,
                 static_cast<float>(m_windowHeight - 62.0f),
                 {255, 220, 120, 255}, m_fontSmall);
    }
    renderText(m_statusMessage, 1120.0f,
               static_cast<float>(m_windowHeight - 40.0f), {140, 220, 140, 255},
               m_fontSmall);

    std::string undoText = "Undo: ";
    undoText += m_undoLabel.empty() ? "—" : m_undoLabel;
    std::string redoText = "Redo: ";
    redoText += m_redoLabel.empty() ? "—" : m_redoLabel;
    renderText(undoText, 20.0f, static_cast<float>(m_windowHeight - 30.0f),
               {200, 180, 120, 255}, m_fontSmall);
    renderText(redoText, 220.0f, static_cast<float>(m_windowHeight - 30.0f),
               {200, 180, 120, 255}, m_fontSmall);

    char gridBuf[80];
    std::snprintf(gridBuf, sizeof(gridBuf), "Сетка: %s, шаг %.2f м (Ctrl+G)",
                  m_gridSnap ? "вкл" : "выкл", m_gridSize);
    renderText(gridBuf, 420.0f, static_cast<float>(m_windowHeight - 30.0f),
               {160, 200, 220, 255}, m_fontSmall);
    renderText("L круги | M режим | Ctrl+R сетка 4+4 | P пауэр-ап | Tab | F "
               "фокус | I точка | Стрелки",
               700.0f, static_cast<float>(m_windowHeight - 30.0f),
               {120, 130, 150, 255}, m_fontSmall);
  }
}

void EditorUI::renderButton(const Button &button, const glm::vec2 &mousePos) {
  const bool hovered = isPointInRect(mousePos, button.position, button.size);
  if (button.highlighted) {
    SDL_SetRenderDrawColor(m_renderer, 70, 120, 180, 255);
  } else if (hovered) {
    SDL_SetRenderDrawColor(m_renderer, 55, 65, 80, 255);
  } else {
    SDL_SetRenderDrawColor(m_renderer, 42, 48, 58, 255);
  }

  SDL_FRect rect{button.position.x, button.position.y, button.size.x,
                 button.size.y};
  SDL_RenderFillRect(m_renderer, &rect);

  SDL_SetRenderDrawColor(m_renderer, 90, 100, 120, 255);
  SDL_RenderRect(m_renderer, &rect);

  if (m_fontSmall) {
    renderText(button.label, button.position.x + 10.0f,
               button.position.y + 8.0f, {230, 230, 240, 255}, m_fontSmall);
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
  case EditorTool::Select:
    return "Выбор";
  case EditorTool::PlaceSpawn:
    return "Спавн";
  case EditorTool::PlaceCheckpoint:
    return "Чекпоинт";
  case EditorTool::PlacePowerUp:
    return "Пауэр-ап";
  case EditorTool::PlaceDestructible:
    return "Разрушаемое";
  case EditorTool::PlaceObstacle:
    return "Препятствие";
  case EditorTool::PlaceDecoration:
    return "Декор";
  case EditorTool::EditTrack:
    return "Трасса";
  case EditorTool::Delete:
    return "Удаление";
  default:
    return "Неизвестно";
  }
}

} // namespace Editor
