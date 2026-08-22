#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct SDL_Renderer;
struct TTF_Font;

namespace Editor {

struct MapObject;
struct MapData;

// Right-side panel that shows fields of the currently selected object and
// lets the editor push change commands back through a callback. Owns its own
// font/text rendering helpers — keeps the EditorUI surface area focused on
// buttons + toolbars.
class PropertyInspector {
public:
    PropertyInspector();
    ~PropertyInspector();

    bool initialize(SDL_Renderer* renderer, const std::string& fontPath);
    void shutdown();

    // Layout: panel sits on the right edge, height = m_windowHeight.
    void onWindowResize(int windowHeight);

    // Updates internal state (handles text-edit cursor blink, etc.).
    void update(float deltaTime);

    // Must be called once per frame after rendering the scene and before the
    // toolbar.
    void render(int windowWidth);

    // Selection: nullptr hides the panel. The inspector does not own the
    // object; it just reads from it and dispatches edits through m_onFieldChanged.
    void setSelected(const MapObject* object);

    bool isMouseOverPanel(const glm::vec2& mousePos, int windowWidth) const;

    glm::ivec4 getPanelRect(int windowWidth) const;
    int panelWidth() const { return m_panelWidth; }

    // Callback: (id, fieldIndex, newValueAsString). The editor owns the
    // command pipeline and will translate fieldIndex -> SetObjectFieldCommand::Field.
    void setOnFieldChanged(std::function<void(const std::string&, int, const std::string&)> callback) {
        m_onFieldChanged = std::move(callback);
    }

    void setOnMoveObject(std::function<void(const std::string&, const glm::vec3&)> callback) {
        m_onMoveObject = std::move(callback);
    }

private:
    enum class EditorKind { Float, Int, Bool, String };

    struct FieldRow {
        std::string label;
        EditorKind kind;
        std::string currentValue;
        // For text fields we keep a small buffer that the user edits directly.
        std::string buffer;
        bool focused = false;
    };

    SDL_Renderer* m_renderer;
    TTF_Font* m_font;

    int m_windowHeight;
    int m_panelWidth;

    std::vector<FieldRow> m_rows;
    int m_focusedRow = -1;
    std::string m_objectId;
    bool m_haveSelection = false;

    glm::vec3 m_positionSnapshot;
    glm::vec3 m_positionBuffer{0.0f};
    std::string m_positionStr;

    std::function<void(const std::string&, int, const std::string&)> m_onFieldChanged;
    std::function<void(const std::string&, const glm::vec3&)> m_onMoveObject;

    void rebuildRows(const MapObject& object);
    void renderRow(const FieldRow& row, float& y, float x);
    void renderText(const std::string& text, float x, float y, SDL_Color color);

    bool isPointInRect(const glm::vec2& p, const glm::vec2& pos, const glm::vec2& size) const;

public:
    // Hot path: SDL key event from MapEditorApp.
    void handleTextInput(const std::string& text);
    void handleKeyDown(SDL_Keycode key);

    // Hot path: SDL mouse click. Returns true if the panel swallowed the event.
    bool handleMouseClick(const glm::vec2& mousePos, int windowWidth);
};

} // namespace Editor