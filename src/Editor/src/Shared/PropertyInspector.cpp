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

constexpr int kRowHeight = 28;
constexpr int kRowPaddingY = 6;
constexpr float kPanelPadding = 12.0f;

SDL_Color colorFromUint(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
  return SDL_Color{r, g, b, a};
}

} // namespace

PropertyInspector::PropertyInspector()
    : m_renderer(nullptr), m_font(nullptr), m_windowHeight(720),
      m_panelWidth(280) {}

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

void PropertyInspector::setSelected(const MapObject *object) {
  if (!object) {
    m_haveSelection = false;
    m_objectId.clear();
    m_rows.clear();
    m_focusedRow = -1;
    return;
  }
  if (m_haveSelection && m_objectId == object->id) {
    if (m_focusedRow < 0) {
      rebuildRows(*object);
    }
    return;
  }
  m_haveSelection = true;
  m_objectId = object->id;
  m_focusedRow = -1;
  rebuildRows(*object);
}

void PropertyInspector::rebuildRows(const MapObject &object) {
  m_rows.clear();

  auto addFloat = [&](const std::string &label, float value) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    FieldRow row;
    row.label = label;
    row.kind = EditorKind::Float;
    row.currentValue = buf;
    row.buffer = buf;
    m_rows.push_back(std::move(row));
  };
  auto addInt = [&](const std::string &label, int value) {
    FieldRow row;
    row.label = label;
    row.kind = EditorKind::Int;
    row.currentValue = std::to_string(value);
    row.buffer = row.currentValue;
    m_rows.push_back(std::move(row));
  };
  auto addBool = [&](const std::string &label, bool value) {
    FieldRow row;
    row.label = label;
    row.kind = EditorKind::Bool;
    row.currentValue = value ? "true" : "false";
    row.buffer = row.currentValue;
    m_rows.push_back(std::move(row));
  };
  auto addString = [&](const std::string &label, const std::string &value) {
    FieldRow row;
    row.label = label;
    row.kind = EditorKind::String;
    row.currentValue = value;
    row.buffer = value;
    m_rows.push_back(std::move(row));
  };

  m_positionSnapshot = object.position;
  char posBuf[64];
  std::snprintf(posBuf, sizeof(posBuf), "%.2f, %.2f, %.2f", object.position.x,
                object.position.y, object.position.z);
  m_positionStr = posBuf;
  m_positionBuffer = object.position;

  addString("id", object.id);
  addString("type", MapData::typeToString(object.type));
  addFloat("radius", object.radius);
  addInt("row", object.row);
  addBool("is_finish", object.isFinishLine);
  addFloat("health", object.health);
  addString("model", object.modelPath);

  addFloat("pos x", object.position.x);
  addFloat("pos y", object.position.y);
  addFloat("pos z", object.position.z);
  if (object.type == MapObjectType::PowerUpSpawn) {
    addInt("subtype", object.subtype);
  }
}

void PropertyInspector::update(float /*deltaTime*/) {
  // Cursor blink / text-input lifecycle is driven from the App loop.
}

void PropertyInspector::renderRow(const FieldRow &row, float &y, float x) {
  const SDL_Color labelColor = colorFromUint(180, 200, 220);
  const SDL_Color valueColor =
      row.focused ? colorFromUint(255, 255, 255) : colorFromUint(220, 220, 230);

  renderText(row.label, x, y, labelColor);

  const SDL_Color boxColor =
      row.focused ? colorFromUint(80, 130, 200) : colorFromUint(40, 50, 70);
  SDL_FRect valueBox{x + 90.0f, y - 2.0f, 160.0f, kRowHeight - 4.0f};
  SDL_SetRenderDrawColor(m_renderer, boxColor.r, boxColor.g, boxColor.b, 255);
  SDL_RenderFillRect(m_renderer, &valueBox);

  const std::string shown = row.focused ? row.buffer : row.currentValue;
  renderText(shown, x + 96.0f, y, valueColor);

  y += kRowHeight;
}

void PropertyInspector::render(int windowWidth) {
  if (!m_renderer)
    return;
  if (!m_haveSelection)
    return;

  SDL_SetRenderDrawColor(m_renderer, 22, 28, 38, 255);
  const auto rect = getPanelRect(windowWidth);
  SDL_FRect panelRect{static_cast<float>(rect.x), static_cast<float>(rect.y),
                      static_cast<float>(rect.z), static_cast<float>(rect.w)};
  SDL_RenderFillRect(m_renderer, &panelRect);

  SDL_SetRenderDrawColor(m_renderer, 50, 60, 80, 255);
  SDL_RenderLine(m_renderer, static_cast<float>(rect.x),
                 static_cast<float>(rect.y), static_cast<float>(rect.x),
                 static_cast<float>(rect.y + rect.w));

  float y = rect.y + kPanelPadding;
  const float x = static_cast<float>(rect.x) + kPanelPadding;

  renderText("Свойства", x, y, colorFromUint(255, 220, 120));
  y += kRowHeight;

  for (auto &row : m_rows) {
    renderRow(row, y, x);
    y += kRowPaddingY;
  }

  y += kRowPaddingY * 2;
  renderText("Позиция:", x, y, colorFromUint(200, 200, 200));
  y += kRowHeight;
  renderText(m_positionStr, x, y, colorFromUint(220, 220, 220));
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

bool PropertyInspector::isPointInRect(const glm::vec2 &p, const glm::vec2 &pos,
                                      const glm::vec2 &size) const {
  return p.x >= pos.x && p.x <= pos.x + size.x && p.y >= pos.y &&
         p.y <= pos.y + size.y;
}

void PropertyInspector::handleTextInput(const std::string &text) {
  if (!m_haveSelection || m_focusedRow < 0)
    return;
  if (m_focusedRow >= static_cast<int>(m_rows.size()))
    return;
  FieldRow &row = m_rows[m_focusedRow];
  row.buffer += text;
}

void PropertyInspector::handleKeyDown(SDL_Keycode key) {
  if (!m_haveSelection || m_focusedRow < 0)
    return;
  if (m_focusedRow >= static_cast<int>(m_rows.size()))
    return;
  FieldRow &row = m_rows[m_focusedRow];

  if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
    if (row.currentValue != row.buffer && m_onFieldChanged) {
      int fieldIndex = m_focusedRow;
      m_onFieldChanged(m_objectId, fieldIndex, row.buffer);
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

  // Header row (title) occupies one slot — start scanning rows just below.
  float y = rect.y + kPanelPadding + kRowHeight;
  const float x = static_cast<float>(rect.x) + kPanelPadding;

  for (int i = 0; i < static_cast<int>(m_rows.size()); ++i) {
    const float rowTop = y;
    const float rowBottom = rowTop + kRowHeight;
    if (mousePos.y >= rowTop && mousePos.y <= rowBottom) {
      // Click inside value box region only (past the label).
      if (mousePos.x >= x + 90.0f) {
        m_focusedRow = i;
        m_rows[i].focused = true;
        m_rows[i].buffer = m_rows[i].currentValue;
      } else {
        // Click on the label: defocus any prior row.
        if (m_focusedRow >= 0 &&
            m_focusedRow < static_cast<int>(m_rows.size())) {
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