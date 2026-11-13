#pragma once

#include "Editor/Building/StructureDef.hpp"
#include "Editor/Logic/UgcLogic.hpp"

#include <SDL3/SDL.h>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct SDL_Renderer;
struct TTF_Font;

namespace Editor {

struct MapObject;
struct TrackPoint;
struct MapData;

class PropertyInspector {
public:
    PropertyInspector();
    ~PropertyInspector();

    bool initialize(SDL_Renderer* renderer, const std::string& fontPath);
    void shutdown();

    void onWindowResize(int windowHeight);
    void update(float deltaTime);
    void render(int windowWidth);

    void setSelected(const MapObject* object);
    void setSelectedStructure(const StructureDef* structure);
    void setSelectedTrackPoint(const TrackPoint* point, int pointIndex);
    void setSelectedTriggerZone(const UgcTriggerZone* zone);
    void clearSelection();

    bool isMouseOverPanel(const glm::vec2& mousePos, int windowWidth) const;
    glm::ivec4 getPanelRect(int windowWidth) const;
    int panelWidth() const { return m_panelWidth; }

    void setOnFieldChanged(std::function<void(const std::string&, int, const std::string&)> callback) {
        m_onFieldChanged = std::move(callback);
    }

    void setOnMoveObject(std::function<void(const std::string&, const glm::vec3&)> callback) {
        m_onMoveObject = std::move(callback);
    }

private:
    enum class EditorKind { Float, Int, Bool, String };
    enum class SelectionType { None, MapObject, Structure, TrackPoint, TriggerZone };

    struct FieldRow {
        std::string label;
        EditorKind kind;
        std::string currentValue;
        std::string buffer;
        bool focused = false;
    };

    SDL_Renderer* m_renderer;
    TTF_Font* m_font;

    int m_windowHeight;
    int m_panelWidth;

    SelectionType m_selectionType = SelectionType::None;
    std::vector<FieldRow> m_rows;
    int m_focusedRow = -1;
    std::string m_objectId;
    int m_pointIndex = -1;
    bool m_haveSelection = false;

    glm::vec3 m_positionSnapshot{0.0f};
    glm::vec3 m_positionBuffer{0.0f};
    std::string m_positionStr;

    std::function<void(const std::string&, int, const std::string&)> m_onFieldChanged;
    std::function<void(const std::string&, const glm::vec3&)> m_onMoveObject;

    void rebuildRows(const MapObject& object);
    void rebuildStructureRows(const StructureDef& structure);
    void rebuildTrackPointRows(const TrackPoint& point, int index);
    void rebuildTriggerZoneRows(const UgcTriggerZone& zone);
    void renderRow(const FieldRow& row, float& y, float x);
    void renderText(const std::string& text, float x, float y, SDL_Color color);

    bool isPointInRect(const glm::vec2& p, const glm::vec2& pos, const glm::vec2& size) const;

public:
    void handleTextInput(const std::string& text);
    void handleKeyDown(SDL_Keycode key);
    bool handleMouseClick(const glm::vec2& mousePos, int windowWidth);
};

} // namespace Editor
