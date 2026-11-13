#pragma once

#include "Editor/Building/StructureDef.hpp"
#include "Editor/Logic/UgcLogic.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <functional>
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct SDL_Renderer;

namespace Editor {

enum class EditorWorkspaceMode {
  TrackAndArena,
  BaseArchitect,
  VehicleWorkshop,
  UgcLogicStudio
};

enum class EditorTool {
  Select,
  EditTrack,
  PlaceSpawn,
  PlaceCheckpoint,
  PlacePowerUp,
  PlaceStructureWall,
  PlaceStructureTurret,
  PlaceStructureUtility,
  PlaceStructurePower,
  PlaceTriggerZone,
  PlaceHazard,
  PlaceJumpPad,
  Delete
};

class EditorUI {
public:
  EditorUI();
  ~EditorUI();

  bool initialize(SDL_Renderer *renderer, int windowWidth, int windowHeight);
  void shutdown();
  void onWindowResize(int windowWidth, int windowHeight);

  void update(float deltaTime);
  void setMousePosition(const glm::vec2 &position);
  void render();

  void setWorkspaceMode(EditorWorkspaceMode mode);
  EditorWorkspaceMode getWorkspaceMode() const { return m_workspaceMode; }

  void setCurrentTool(EditorTool tool);
  EditorTool getCurrentTool() const { return m_currentTool; }
  int getSpawnRow() const { return m_spawnRow; }
  const std::string &getStatusMessage() const { return m_statusMessage; }
  const std::string &getCurrentFilePath() const { return m_currentFilePath; }

  void setStatusMessage(const std::string &message);
  void setCurrentFilePath(const std::string &path);
  void setMapName(const std::string &name);
  void setObjectCount(int count);
  void setStructureCount(int count) { m_structureCount = count; }
  void setSelectedObjectId(const std::string &id);
  void setMapFileList(const std::vector<std::string> &files);
  void setUndoLabel(const std::string &label);
  void setRedoLabel(const std::string &label);
  void setGridInfo(float size, bool enabled);
  void setDirty(bool dirty);
  void setMapMode(const std::string &mode) { m_mapMode = mode; }
  void setTrackPointCount(int count) { m_trackPointCount = count; }
  void setLapCount(int count) { m_lapCount = count; }

  bool isMouseOverUI(const glm::vec2 &mousePos) const;
  glm::ivec4 getViewportRect(int inspectorPanelWidth = 0) const;

  void setOnNewMap(std::function<void()> callback) {
    m_onNewMap = std::move(callback);
  }
  void setOnSaveMap(std::function<void()> callback) {
    m_onSaveMap = std::move(callback);
  }
  void setOnLoadMap(std::function<void(const std::string &)> callback) {
    m_onLoadMap = std::move(callback);
  }
  void setOnDeleteSelected(std::function<void()> callback) {
    m_onDeleteSelected = std::move(callback);
  }
  void setOnToggleTrackClosed(std::function<void()> callback) {
    m_onToggleTrackClosed = std::move(callback);
  }
  void setOnRaceGrid(std::function<void()> callback) {
    m_onRaceGrid = std::move(callback);
  }
  void setOnSwitchMode(std::function<void(EditorWorkspaceMode)> callback) {
    m_onSwitchMode = std::move(callback);
  }
  void setOnExportUgcPackage(std::function<void()> callback) {
    m_onExportUgcPackage = std::move(callback);
  }

  void handleMouseClick(const glm::vec2 &position);

private:
  struct Button {
    std::string id;
    std::string label;
    glm::vec2 position;
    glm::vec2 size;
    std::function<void()> onClick;
    bool highlighted = false;
    bool active = false;
  };

  SDL_Renderer *m_renderer;
  TTF_Font *m_font;
  TTF_Font *m_fontSmall;

  int m_windowWidth;
  int m_windowHeight;
  int m_toolbarHeight;
  int m_sidePanelWidth;

  EditorWorkspaceMode m_workspaceMode = EditorWorkspaceMode::TrackAndArena;
  EditorTool m_currentTool;
  int m_spawnRow;
  std::string m_statusMessage;
  std::string m_currentFilePath;
  std::string m_mapName;
  int m_objectCount;
  int m_structureCount = 0;
  std::string m_selectedObjectId;
  std::vector<std::string> m_mapFileList;
  glm::vec2 m_mousePosition{0.0f};

  std::string m_undoLabel;
  std::string m_redoLabel;
  float m_gridSize = 1.0f;
  bool m_gridSnap = true;
  bool m_dirty = false;
  std::string m_mapMode = "both";
  int m_trackPointCount = 0;
  int m_lapCount = 3;

  std::vector<Button> m_buttons;

  std::function<void()> m_onNewMap;
  std::function<void()> m_onSaveMap;
  std::function<void(const std::string &)> m_onLoadMap;
  std::function<void()> m_onDeleteSelected;
  std::function<void()> m_onToggleTrackClosed;
  std::function<void()> m_onRaceGrid;
  std::function<void(EditorWorkspaceMode)> m_onSwitchMode;
  std::function<void()> m_onExportUgcPackage;

  void rebuildButtons();
  void renderPanel();
  void renderButton(const Button &button, const glm::vec2 &mousePos);
  void renderText(const std::string &text, float x, float y, SDL_Color color,
                  TTF_Font *font);
  bool isPointInRect(const glm::vec2 &point, const glm::vec2 &pos,
                     const glm::vec2 &size) const;
  std::string toolLabel(EditorTool tool) const;
};

} // namespace Editor
