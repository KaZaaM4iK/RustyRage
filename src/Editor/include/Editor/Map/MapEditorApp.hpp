#pragma once

#include "Editor/Map/MapData.hpp"
#include "Editor/Shared/EditorCamera.hpp"
#include "Editor/Shared/EditorRenderer.hpp"
#include "Editor/Shared/EditorUI.hpp"
#include "Editor/Shared/PropertyInspector.hpp"
#include "Editor/Shared/UndoStack.hpp"

#include <memory>
#include <string>

struct SDL_Window;
struct SDL_Renderer;

namespace Core {
class InputManager;
}

namespace Editor {

class MapEditorApp {
public:
  MapEditorApp(int width, int height, const std::string &title);
  ~MapEditorApp();

  void run();

private:
  int m_width;
  int m_height;
  std::string m_title;
  bool m_running;

  SDL_Window *m_window;
  SDL_Renderer *m_renderer;

  std::unique_ptr<Core::InputManager> m_input;
  std::unique_ptr<EditorCamera> m_camera;
  std::unique_ptr<EditorRenderer> m_sceneRenderer;
  std::unique_ptr<EditorUI> m_ui;
  std::unique_ptr<PropertyInspector> m_inspector;

  MapData m_mapData;
  std::string m_currentFilePath;
  std::string m_selectedObjectId;
  uint32_t m_objectIdCounter;

  // Track-edit state
  int m_selectedTrackPoint = -1;
  bool m_trackClosedCached = false;

  bool m_dragging = false;
  glm::vec3 m_dragStartWorld{0.0f};
  glm::vec3 m_dragOriginalPos{0.0f};

  // Undo / grid
  UndoStack m_undoStack;
  float m_gridSize = 1.0f;
  bool m_gridSnapEnabled = true;
  bool m_dirty = false;

  bool m_ctrlHeld;
  bool m_rightMouseHeld;

  bool initialize();
  void shutdown();
  void processEvents();
  void update(float deltaTime);
  void render();

  void newMap();
  void saveMap();
  void loadMap(const std::string &path);
  void refreshMapList();
  void deleteSelected();
  void undo();
  void redo();
  void cycleGridSize();

  void handleViewportClick(const glm::vec2 &screenPos);
  void placeObjectAt(const glm::vec3 &position);
  void addTrackPointAt(const glm::vec3 &position);
  void deleteTrackPoint(int index);
  void adjustSelectedTrackWidth(float delta);
  void insertTrackPointAfterSelected();
  void duplicateSelectedObject();
  void nudgeSelected(const glm::vec3 &delta);
  void cycleSelection(bool reverse);
  void placeRaceStartGrid();
  void toggleTrackClosed();
  void selectTrackPointNear(const glm::vec2 &screenPos);
  void selectObjectAt(const glm::vec2 &screenPos);
  MapObject *findObjectNear(const glm::vec2 &screenPos,
                            float maxDistance = 20.0f);
  int findTrackPointNear(const glm::vec2 &screenPos,
                         float maxDistance = 18.0f) const;

  void pushCommand(std::unique_ptr<Command> cmd);
  glm::vec3 snapToGrid(const glm::vec3 &p) const;

  // PropertyInspector callbacks
  void onInspectorFieldChanged(const std::string &id, int fieldIndex,
                               const std::string &value);
  void onInspectorMoveObject(const std::string &id, const glm::vec3 &position);

  std::string mapsDirectory() const;
  void ensureMapsDirectory() const;
};

} // namespace Editor
