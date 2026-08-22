#include "Editor/Map/TrackMesh.hpp"

#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <limits>

namespace Editor {

namespace {

constexpr float kCatmullRomTension = 0.5f;

struct SampledPoint {
  glm::vec3 position;
  glm::vec3 forward;
  glm::vec3 right;
  float width;
};

glm::vec3 catmullRom(const glm::vec3 &p0, const glm::vec3 &p1,
                     const glm::vec3 &p2, const glm::vec3 &p3, float t) {
  const float t2 = t * t;
  const float t3 = t2 * t;
  const float s = kCatmullRomTension;

  const glm::vec3 e1 = (p2 - p0) * s;
  const glm::vec3 e2 = (p3 - p1) * s;

  const float c0 = 2.0f * t3 - 3.0f * t2 + 1.0f;
  const float c1 = t3 - 2.0f * t2 + t;
  const float c2 = -2.0f * t3 + 3.0f * t2;
  const float c3 = t3 - t2;

  return p1 * c0 + e1 * c1 + p2 * c2 + e2 * c3;
}

void fetchControlPoints(const std::vector<TrackPoint> &points, bool closed,
                        int i, int n, glm::vec3 &p0, glm::vec3 &p1,
                        glm::vec3 &p2, glm::vec3 &p3) {
  if (closed) {
    p0 = points[(i - 1 + n) % n].position;
    p1 = points[i].position;
    p2 = points[(i + 1) % n].position;
    p3 = points[(i + 2) % n].position;
  } else {
    p0 = points[std::max(i - 1, 0)].position;
    p1 = points[i].position;
    p2 = points[std::min(i + 1, n - 1)].position;
    p3 = points[std::min(i + 2, n - 1)].position;
  }
}

float fetchWidth(const std::vector<TrackPoint> &points, bool closed, int i,
                 int n, float t) {
  auto widthAt = [&](int idx) { return points[((idx % n) + n) % n].width; };
  if (closed) {
    const float w0 = widthAt(i);
    const float w1 = widthAt(i + 1);
    return glm::mix(w0, w1, t);
  }
  const int next = std::min(i + 1, n - 1);
  return glm::mix(points[i].width, points[next].width, t);
}

std::vector<SampledPoint> sampleSpline(const std::vector<TrackPoint> &points,
                                       bool closed, float arcLength) {
  std::vector<SampledPoint> sampled;
  if (points.size() < 2) {
    return sampled;
  }

  const int n = static_cast<int>(points.size());

  std::vector<glm::vec3> centerline;
  for (int i = 0; i < n; ++i) {
    glm::vec3 p0, p1, p2, p3;
    fetchControlPoints(points, closed, i, n, p0, p1, p2, p3);
    const float localLen = glm::length(p2 - p1);
    const int subdivisions =
        std::max(2, static_cast<int>(std::ceil(localLen / arcLength)));
    for (int s = 0; s < subdivisions; ++s) {
      const float t = static_cast<float>(s) / subdivisions;
      centerline.push_back(catmullRom(p0, p1, p2, p3, t));
    }
  }
  if (closed) {
    glm::vec3 p0, p1, p2, p3;
    fetchControlPoints(points, closed, n - 1, n, p0, p1, p2, p3);
    centerline.push_back(catmullRom(p0, p1, p2, p3, 1.0f));
  } else {
    centerline.push_back(points.back().position);
  }

  sampled.reserve(centerline.size());
  glm::vec3 up(0.0f, 1.0f, 0.0f);
  for (size_t i = 0; i < centerline.size(); ++i) {
    const glm::vec3 &here = centerline[i];
    glm::vec3 forward;
    if (i + 1 < centerline.size()) {
      forward = glm::normalize(centerline[i + 1] - here);
    } else if (i > 0) {
      forward = glm::normalize(here - centerline[i - 1]);
    } else {
      forward = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    if (glm::length(right) < 0.001f) {
      right = glm::vec3(1.0f, 0.0f, 0.0f);
    }

    float width = points.back().width;
    if (n > 0) {
      const float totalLen = static_cast<float>(centerline.size() - 1);
      const float globalT =
          (totalLen > 0.0f) ? (static_cast<float>(i) / totalLen) : 0.0f;
      const float segmentT = globalT * (n - (closed ? 0 : 1));
      const int segIndex =
          std::min(static_cast<int>(std::floor(segmentT)), n - 1);
      const float frac = std::min(std::max(segmentT - segIndex, 0.0f), 1.0f);
      width = fetchWidth(points, closed, segIndex, n, frac);
    }

    sampled.push_back({here, forward, right, width});
  }

  return sampled;
}

} // namespace

glm::vec3 evaluateTrackPosition(const std::vector<TrackPoint> &points,
                                bool closed, float normalizedT) {
  if (points.empty())
    return glm::vec3(0.0f);
  if (points.size() == 1)
    return points[0].position;
  const float t = glm::clamp(normalizedT, 0.0f, 1.0f);
  const float scaledT = t * (points.size() - 1);
  const int idx = std::min(static_cast<int>(std::floor(scaledT)),
                           static_cast<int>(points.size()) - 1);
  const float frac = scaledT - idx;
  const int next = closed
                       ? (idx + 1) % static_cast<int>(points.size())
                       : std::min(idx + 1, static_cast<int>(points.size()) - 1);
  return glm::mix(points[idx].position, points[next].position, frac);
}

TrackMesh generateTrackMesh(const std::vector<TrackPoint> &points, bool closed,
                            float arcLength, float extensionFactor) {
  TrackMesh mesh;
  if (points.size() < 2) {
    return mesh;
  }

  auto sampled = sampleSpline(points, closed, std::max(arcLength, 0.5f));
  if (sampled.size() < 2) {
    return mesh;
  }

  const float shoulderWidth = std::max(extensionFactor, 1.0f);

  mesh.vertices.reserve(sampled.size() * 8);
  mesh.indices.reserve((sampled.size() - 1) * 24);

  glm::vec3 minB(std::numeric_limits<float>::max());
  glm::vec3 maxB(-std::numeric_limits<float>::max());

  auto pushQuad = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    mesh.indices.push_back(a);
    mesh.indices.push_back(b);
    mesh.indices.push_back(c);
    mesh.indices.push_back(a);
    mesh.indices.push_back(c);
    mesh.indices.push_back(d);
  };

  for (size_t i = 0; i < sampled.size(); ++i) {
    const SampledPoint &s = sampled[i];
    const float half = s.width * 0.5f;
    const float ext = s.width * 0.5f + shoulderWidth;

    const glm::vec3 vL0 = s.position - s.right * half; // driving surface L
    const glm::vec3 vR0 = s.position + s.right * half; // driving surface R
    const glm::vec3 vL1 = s.position - s.right * ext;  // shoulder L
    const glm::vec3 vR1 = s.position + s.right * ext;  // shoulder R

    mesh.vertices.push_back(vL0);
    mesh.vertices.push_back(vR0);
    mesh.vertices.push_back(vL1);
    mesh.vertices.push_back(vR1);

    for (const auto &v : {vL0, vR0, vL1, vR1}) {
      minB = glm::min(minB, v);
      maxB = glm::max(maxB, v);
    }
  }

  for (size_t i = 0; i + 1 < sampled.size(); ++i) {
    const uint32_t base = static_cast<uint32_t>(i) * 4;
    const uint32_t next = base + 4;

    // Driving surface: L0,R0 (this) -> L0,R0 (next) — strip 0..3
    pushQuad(base + 0, next + 0, next + 1, base + 1);
    // Left shoulder: L0,L1 (this) -> L0,L1 (next) — strip 0..2
    pushQuad(base + 2, next + 2, next + 0, base + 0);
    // Right shoulder: R0,R1 (this) -> R0,R1 (next) — strip 1..3
    pushQuad(base + 1, next + 1, next + 3, base + 3);
  }

  mesh.minBounds = minB;
  mesh.maxBounds = maxB;
  return mesh;
}

} // namespace Editor