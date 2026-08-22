#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

namespace Editor {

enum class MapObjectType {
  SpawnPoint,
  Checkpoint,
  PowerUpSpawn,
  Destructible,
  Obstacle,
  Decoration
};

struct MapObject {
  std::string id;
  MapObjectType type = MapObjectType::Decoration;
  glm::vec3 position{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  int row = 1;
  int subtype = 0;
  std::string modelPath;
  float radius = 10.0f;
  bool isFinishLine = false;
  float health = 50.0f;

  MapObject() = default;
};

// Control point of the racing track spline. The track is rendered as a
// ribbon mesh sampled from Catmull-Rom interpolation between these points.
// Point index 0 = start line; for closed tracks the last segment loops back
// to it (start == finish).
struct TrackPoint {
  glm::vec3 position{0.0f};
  float width = 12.0f;   // width of the driving surface at this point
  bool isFinish = false; // only meaningful for the last point of an open track
  float banking = 0.0f;  // reserved for future per-point tilt

  TrackPoint() = default;
  explicit TrackPoint(const glm::vec3 &p) : position(p) {}
  TrackPoint(const glm::vec3 &p, float w) : position(p), width(w) {}
};

enum class MapMode { Both, Arena, Vigilante };

// Подтип PowerUpSpawn / оружия на карте (общие для Arena и Vigilante).
enum class PowerUpKind : int {
  Random = 0, // случайный при подборе
  Missile = 1,
  Mine = 2,
  OilSlick = 3,
  Shield = 4,
  Nitro = 5
};

inline const char *powerUpKindToString(PowerUpKind kind) {
  switch (kind) {
  case PowerUpKind::Missile:
    return "missile";
  case PowerUpKind::Mine:
    return "mine";
  case PowerUpKind::OilSlick:
    return "oil";
  case PowerUpKind::Shield:
    return "shield";
  case PowerUpKind::Nitro:
    return "nitro";
  default:
    return "random";
  }
}

inline PowerUpKind powerUpKindFromInt(int value) {
  if (value < 0 || value > 5)
    return PowerUpKind::Random;
  return static_cast<PowerUpKind>(value);
}

inline const char *mapModeToString(MapMode mode) {
  switch (mode) {
  case MapMode::Arena:
    return "arena";
  case MapMode::Vigilante:
    return "vigilante";
  default:
    return "both";
  }
}

inline MapMode mapModeFromString(const std::string &value) {
  if (value == "arena")
    return MapMode::Arena;
  if (value == "vigilante")
    return MapMode::Vigilante;
  return MapMode::Both;
}

struct MapData {
  static constexpr int CURRENT_VERSION = 2;

  std::string name = "New Map";
  int version = CURRENT_VERSION;
  std::string author = "unknown";
  MapMode mode = MapMode::Both;
  int lapCount = 3; // Arena Speed: сколько кругов до финиша
  std::string terrainMesh;
  glm::vec3 boundaryMin{-500.0f, -50.0f, -500.0f};
  glm::vec3 boundaryMax{500.0f, 200.0f, 500.0f};
  std::vector<MapObject> objects;

  // Track spline — editor generates ribbon mesh from this for both preview and
  // runtime.
  std::vector<TrackPoint> trackPoints;
  bool trackClosed = false;
  float trackHeight = 0.0f;

  // Moderation / official status. "pending" — freshly authored, "approved" —
  // visible to the gametheque storefront, "rejected" — hidden from listings but
  // still editable. `isApproved` is a cached projection of the string for cheap
  // filtering; keep them in sync when one is set directly.
  bool isApproved = false;
  std::string moderationStatus = "pending";

  void createDefault(uint32_t &idCounter);
  void addObject(const MapObject &object);
  bool removeObject(const std::string &id);
  MapObject *findObject(const std::string &id);
  const MapObject *findObject(const std::string &id) const;

  // Counter-based ID generator — monotonic per type, never collides after
  // deletions. Pass an external counter so the editor can share state across
  // tools.
  std::string generateObjectId(MapObjectType type, uint32_t &counter) const;

  bool saveToFile(const std::string &path) const;
  bool loadFromFile(const std::string &path);

  static std::string typeToString(MapObjectType type);
  static MapObjectType typeFromString(const std::string &value);
};

} // namespace Editor
