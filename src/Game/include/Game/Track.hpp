#pragma once

#include "Editor/Map/MapData.hpp"

#include <string>

namespace Game {

// Minimal in-game wrapper around an editor-authored map. Holds the loaded
// MapData in memory so the game can read the track spline and object layout
// without re-parsing the JSON each frame. Physics, vehicles, and rendering
// are intentionally out of scope — this is just the data layer.
class Track {
public:
  Track();
  explicit Track(const std::string &mapPath);

  // Reloads from disk. Returns true on success.
  bool load(const std::string &mapPath);

  bool isLoaded() const { return m_loaded; }
  const std::string &getMapPath() const { return m_mapPath; }

  const Editor::MapData &data() const { return m_data; }
  Editor::MapData &data() { return m_data; }

  size_t objectCount() const { return m_data.objects.size(); }
  size_t trackPointCount() const { return m_data.trackPoints.size(); }
  bool isClosedLoop() const { return m_data.trackClosed; }

private:
  Editor::MapData m_data;
  std::string m_mapPath;
  bool m_loaded;
};

} // namespace Game
