#include "Editor/Shared/PropertyInspector.hpp"
#include "Editor/Map/MapData.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <glm/glm.hpp>

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace Editor {

namespace {

constexpr int kRowHeight = 26;
constexpr int kRowPaddingY = 5;
constexpr float kPanelPadding = 12.0f;

SDL_Color colorFromUint(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return SDL_Color{r, g, b, a};
}

} // namespace

PropertyInspector::PropertyInspector()
    : m_renderer(nullptr), m_font(nullptr), m_windowHeight(720),
      m_panelWidth(300) {}

PropertyInspector::~PropertyInspector() { shutdown(); }

bool PropertyInspector::initialize(SDL_Renderer *renderer,
                                   const std::string &fontPath) {
  m_renderer = renderer;
  m_font = TTF_OpenFont(fontPath.c_str(), 16);
  if (!m_font) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "PropertyInspector: failed to open font %s", fontPath.c_str());
  }
  return true;
}

void PropertyInspector::shutdown() {
  if (m_font) {
    TTF_CloseFont(m_font);
    m_font = nullptr;
  }
}

void PropertyInspector::onWindowResize(int windowHeight) {
  m_windowHeight = std::max(1, windowHeight);
}

glm::ivec4 PropertyInspector::getPanelRect(int windowWidth) const {
  return glm::ivec4(std::max(0, windowWidth - m_panelWidth), 0, m_panelWidth,
                    m_windowHeight - 72);
}

bool PropertyInspector::isMouseOverPanel(const glm::vec2 &mousePos,
                                         int windowWidth) const {
  const auto rect = getPanelRect(windowWidth);
  return mousePos.x >= rect.x && mousePos.x <= rect.x + rect.z &&
         mousePos.y >= rect.y && mousePos.y <= rect.y + rect.w;
}

void PropertyInspector::clearSelection() {
  m_haveSelection = false;
  m_selectionType = SelectionType::None;
  m_objectId.clear();
  m_rows.clear();
  m_focusedRow = -1;
  m_pointIndex = -1;
}

void PropertyInspector::setSelected(const MapObject *object) {
  if (!object) {
    clearSelection();
    return;
  }
  m_haveSelection = true;
  m_selectionType = SelectionType::MapObject;
  m_objectId = object->id;
  m_focusedRow = -1;
  rebuildRows(*object);
}

void PropertyInspector::setSelectedStructure(const StructureDef *structure) {
  if (!structure) {
    clearSelection();
    return;
  }
  m_haveSelection = true;
  m_selectionType = SelectionType::Structure;
  m_objectId = structure->id;
  m_focusedRow = -1;
  rebuildStructureRows(*structure);
}

void PropertyInspector::setSelectedTrackPoint(const TrackPoint *point, int pointIndex) {
  if (!point) {
    clearSelection();
    return;
  }
  m_haveSelection = true;
  m_selectionType = SelectionType::TrackPoint;
  m_pointIndex = pointIndex;
  m_objectId = "track_point_" + std::to_string(pointIndex);
  m_focusedRow = -1;
  rebuildTrackPointRows(*point, pointIndex);
}

void PropertyInspector::setSelectedTriggerZone(const UgcTriggerZone *zone) {
  if (!zone) {
    clearSelection();
    return;
  }
  m_haveSelection = true;
  m_selectionType = SelectionType::TriggerZone;
  m_objectId = zone->id;
  m_focusedRow = -1;
  rebuildTriggerZoneRows(*zone);
}

void PropertyInspector::rebuildRows(const MapObject &object) {
  m_rows.clear();
  auto addFloat = [&](const std::string &label, float value) {
    char buf[32]; std::snprintf(buf, sizeof(buf), "%.2f", value);
    m_rows.push_back({label, EditorKind::Float, buf, buf, false});
  };
  auto addInt = [&](const std::string &label, int value) {
    std::string s = std::to_string(value);
    m_rows.push_back({label, EditorKind::Int, s, s, false});
  };
  auto addBool = [&](const std::string &label, bool value) {
    std::string s = value ? "true" : "false";
    m_rows.push_back({label, EditorKind::Bool, s, s, false});
  };
  auto addString = [&](const std::string &label, const std::string &value) {
    m_rows.push_back({label, EditorKind::String, value, value, false});
  };

  m_positionSnapshot = object.position;
  char posBuf[64];
  std::snprintf(posBuf, sizeof(posBuf), "%.1f, %.1f, %.1f", object.position.x, object.position.y, object.position.z);
  m_positionStr = posBuf;

  addString("ID", object.id);
  addString("Тип", MapData::typeToString(object.type));
  addFloat("Радиус", object.radius);
  addInt("Ряд", object.row);
  addBool("Финиш", object.isFinishLine);
  addFloat("Прочность", object.health);
  addFloat("X", object.position.x);
  addFloat("Y", object.position.y);
  addFloat("Z", object.position.z);
}

void PropertyInspector::rebuildStructureRows(const StructureDef &structure) {
  m_rows.clear();
  auto addFloat = [&](const std::string &label, float value) {
    char buf[32]; std::snprintf(buf, sizeof(buf), "%.1f", value);
    m_rows.push_back({label, EditorKind::Float, buf, buf, false});
  };
  auto addInt = [&](const std::string &label, int value) {
    std::string s = std::to_string(value);
    m_rows.push_back({label, EditorKind::Int, s, s, false});
  };
  auto addString = [&](const std::string &label, const std::string &value) {
    m_rows.push_back({label, EditorKind::String, value, value, false});
  };

  m_positionSnapshot = structure.position;
  char posBuf[64];
  std::snprintf(posBuf, sizeof(posBuf), "%.1f, %.1f, %.1f", structure.position.x, structure.position.y, structure.position.z);
  m_positionStr = posBuf;

  addString("ID", structure.id);
  addString("Имя", structure.name);
  addString("Категория", StructureDef::categoryToString(structure.category));
  addFloat("Прочность HP", structure.maxHealth);
  addFloat("Броня %", structure.armorRating);
  addFloat("Дальность", structure.attackRange);
  addFloat("Урон/сек", structure.damagePerSecond);
  addInt("Стоимость", structure.scrapCost);
  addInt("Энергия", structure.energyDrain);
  addInt("Команда", structure.teamId);
  addString("UGC Тег", structure.triggerTag);
}

void PropertyInspector::rebuildTrackPointRows(const TrackPoint &point, int index) {
  m_rows.clear();
  auto addFloat = [&](const std::string &label, float value) {
    char buf[32]; std::snprintf(buf, sizeof(buf), "%.2f", value);
    m_rows.push_back({label, EditorKind::Float, buf, buf, false});
  };
  auto addBool = [&](const std::string &label, bool value) {
    std::string s = value ? "true" : "false";
    m_rows.push_back({label, EditorKind::Bool, s, s, false});
  };
  auto addString = [&](const std::string &label, const std::string &value) {
    m_rows.push_back({label, EditorKind::String, value, value, false});
  };

  m_positionSnapshot = point.position;
  char posBuf[64];
  std::snprintf(posBuf, sizeof(posBuf), "%.1f, %.1f, %.1f", point.position.x, point.position.y, point.position.z);
  m_positionStr = posBuf;

  addString("Точка #", std::to_string(index));
  addFloat("Ширина (м)", point.width);
  addFloat("Крен/Banking", point.banking);
  addString("Покрытие", trackSurfaceToString(point.surface));
  addBool("Финиш", point.isFinish);
  addFloat("X", point.position.x);
  addFloat("Y (Высота)", point.position.y);
  addFloat("Z", point.position.z);
}

void PropertyInspector::rebuildTriggerZoneRows(const UgcTriggerZone &zone) {
  m_rows.clear();
  auto addFloat = [&](const std::string &label, float value) {
    char buf[32]; std::snprintf(buf, sizeof(buf), "%.1f", value);
    m_rows.push_back({label, EditorKind::Float, buf, buf, false});
  };
  auto addInt = [&](const std::string &label, int value) {
    std::string s = std::to_string(value);
    m_rows.push_back({label, EditorKind::Int, s, s, false});
  };
  auto addString = [&](const std::string &label, const std::string &value) {
    m_rows.push_back({label, EditorKind::String, value, value, false});
  };

  m_positionSnapshot = zone.center;
  char posBuf[64];
  std::snprintf(posBuf, sizeof(posBuf), "%.1f, %.1f, %.1f", zone.center.x, zone.center.y, zone.center.z);
  m_positionStr = posBuf;

  addString("ID Зоны", zone.id);
  addString("Название", zone.name);
  addFloat("Размер X", zone.extents.x);
  addFloat("Размер Y", zone.extents.y);
  addFloat("Размер Z", zone.extents.z);
  addInt("Фильтр команды", zone.filterTeam);
}

void PropertyInspector::update(float /*deltaTime*/) {}

void PropertyInspector::renderRow(const FieldRow &row, float &y, float x) {
  const SDL_Color labelColor = colorFromUint(170, 195, 220);
  const SDL_Color valueColor =
      row.focused ? colorFromUint(255, 255, 255) : colorFromUint(220, 225, 235);

  renderText(row.label, x, y, labelColor);

  const SDL_Color boxColor =
      row.focused ? colorFromUint(70, 120, 200) : colorFromUint(35, 45, 60);
  SDL_FRect valueBox{x + 105.0f, y - 2.0f, 160.0f, kRowHeight - 4.0f};
  SDL_SetRenderDrawColor(m_renderer, boxColor.r, boxColor.g, boxColor.b, 255);
  SDL_RenderFillRect(m_renderer, &valueBox);

  const std::string shown = row.focused ? row.buffer : row.currentValue;
  renderText(shown, x + 110.0f, y, valueColor);

  y += kRowHeight;
}

void PropertyInspector::render(int windowWidth) {
  if (!m_renderer || !m_haveSelection)
    return;

  SDL_SetRenderDrawColor(m_renderer, 20, 24, 34, 255);
  const auto rect = getPanelRect(windowWidth);
  SDL_FRect panelRect{static_cast<float>(rect.x), static_cast<float>(rect.y),
                      static_cast<float>(rect.z), static_cast<float>(rect.w)};
  SDL_RenderFillRect(m_renderer, &panelRect);

  SDL_SetRenderDrawColor(m_renderer, 45, 55, 75, 255);
  SDL_RenderLine(m_renderer, static_cast<float>(rect.x),
                 static_cast<float>(rect.y), static_cast<float>(rect.x),
                 static_cast<float>(rect.y + rect.w));

  float y = rect.y + kPanelPadding;
  const float x = static_cast<float>(rect.x) + kPanelPadding;

  std::string title = "Инспектор Параметров";
  if (m_selectionType == SelectionType::Structure) title = "Параметры Строения";
  else if (m_selectionType == SelectionType::TrackPoint) title = "Узел Трассы";
  else if (m_selectionType == SelectionType::TriggerZone) title = "UGC Триггер-Зона";

  renderText(title, x, y, colorFromUint(255, 220, 110));
  y += kRowHeight + 4.0f;

  for (auto &row : m_rows) {
    renderRow(row, y, x);
    y += kRowPaddingY;
  }

  y += kRowPaddingY * 2;
  renderText("Координаты (X, Y, Z):", x, y, colorFromUint(180, 200, 220));
  y += kRowHeight;
  renderText(m_positionStr, x, y, colorFromUint(230, 230, 240));
}

void PropertyInspector::renderText(const std::string &text, float x, float y,
                                   SDL_Color color) {
  if (!m_font || text.empty())
    return;
  SDL_Surface *surface =
      TTF_RenderText_Blended(m_font, text.c_str(), text.size(), color);
  if (!surface)
    return;
  SDL_Texture *texture = SDL_CreateTextureFromSurface(m_renderer, surface);
  if (texture) {
    SDL_FRect dst{x, y, static_cast<float>(surface->w),
                  static_cast<float>(surface->h)};
    SDL_RenderTexture(m_renderer, texture, nullptr, &dst);
    SDL_DestroyTexture(texture);
  }
  SDL_DestroySurface(surface);
}

void PropertyInspector::handleTextInput(const std::string &text) {
  if (!m_haveSelection || m_focusedRow < 0 || m_focusedRow >= static_cast<int>(m_rows.size()))
    return;
  m_rows[m_focusedRow].buffer += text;
}

void PropertyInspector::handleKeyDown(SDL_Keycode key) {
  if (!m_haveSelection || m_focusedRow < 0 || m_focusedRow >= static_cast<int>(m_rows.size()))
    return;
  FieldRow &row = m_rows[m_focusedRow];

  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    if (row.currentValue != row.buffer && m_onFieldChanged) {
      m_onFieldChanged(m_objectId, m_focusedRow, row.buffer);
      row.currentValue = row.buffer;
    }
    row.focused = false;
    m_focusedRow = -1;
    return;
  }

  if (key == SDLK_BACKSPACE) {
    if (!row.buffer.empty())
      row.buffer.pop_back();
    return;
  }

  if (key == SDLK_ESCAPE) {
    row.buffer = row.currentValue;
    row.focused = false;
    m_focusedRow = -1;
  }
}

bool PropertyInspector::handleMouseClick(const glm::vec2 &mousePos,
                                         int windowWidth) {
  if (!m_haveSelection || !m_renderer)
    return false;
  const auto rect = getPanelRect(windowWidth);
  if (mousePos.x < rect.x || mousePos.x > rect.x + rect.z ||
      mousePos.y < rect.y || mousePos.y > rect.y + rect.w) {
    return false;
  }

  float y = rect.y + kPanelPadding + kRowHeight + 4.0f;
  const float x = static_cast<float>(rect.x) + kPanelPadding;

  for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
    const float rowTop = y;
    const float rowBottom = rowTop + kRowHeight;
    if (mousePos.y >= rowTop && mousePos.y <= rowBottom) {
      if (mousePos.x >= x + 105.0f) {
        m_focusedRow = i;
        m_rows[i].focused = true;
        m_rows[i].buffer = m_rows[i].currentValue;
      } else {
        if (m_focusedRow >= 0 && m_focusedRow < static_cast<int>(m_rows.size())) {
          m_rows[m_focusedRow].focused = false;
        }
        m_focusedRow = -1;
      }
      return true;
    }
    y = rowBottom + kRowPaddingY;
  }
  return true;
}

} // namespace Editor
