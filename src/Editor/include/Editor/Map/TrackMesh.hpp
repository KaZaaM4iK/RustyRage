#pragma once

#include "MapData.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

namespace Editor {

struct TrackMesh {
  std::vector<glm::vec3> vertices;
  std::vector<uint32_t> indices;
  glm::vec3 minBounds{0.0f};
  glm::vec3 maxBounds{0.0f};

  bool empty() const { return vertices.empty(); }
  void clear() {
    vertices.clear();
    indices.clear();
    minBounds = maxBounds = glm::vec3{0.0f};
  }
};

// Generates a ribbon mesh (driving surface + shoulders) along the track spline.
// Shared by editor preview and the game runtime — same mesh goes into
// Core::Physics::createMeshShape for collision.
TrackMesh generateTrackMesh(const std::vector<TrackPoint> &points, bool closed,
                            float arcLength = 4.0f,
                            float extensionFactor = 4.0f);

glm::vec3 evaluateTrackPosition(const std::vector<TrackPoint> &points,
                                bool closed, float normalizedT);

} // namespace Editor