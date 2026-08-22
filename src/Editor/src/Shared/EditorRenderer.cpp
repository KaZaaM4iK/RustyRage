#include "Editor/Shared/EditorRenderer.hpp"
#include "Editor/Map/MapData.hpp"
#include "Editor/Shared/EditorCamera.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Editor {

namespace {

void computeFrame(const glm::vec3 &prev, const glm::vec3 &here,
                  const glm::vec3 &next, glm::vec3 &forward, glm::vec3 &right) {
  glm::vec3 dir = next - here;
  if (glm::length(dir) < 1e-4f) {
    dir = here - prev;
  }
  forward = glm::normalize(dir);
  glm::vec3 up(0.0f, 1.0f, 0.0f);
  right = glm::normalize(glm::cross(forward, up));
  if (glm::length(right) < 1e-4f) {
    right = glm::vec3(1.0f, 0.0f, 0.0f);
  }
}

} // namespace

EditorRenderer::EditorRenderer(SDL_Renderer *renderer)
    : m_renderer(renderer), m_viewportX(0), m_viewportY(0),
      m_viewportWidth(1280), m_viewportHeight(720) {}

void EditorRenderer::setViewport(int x, int y, int width, int height) {
  m_viewportX = x;
  m_viewportY = y;
  m_viewportWidth = std::max(1, width);
  m_viewportHeight = std::max(1, height);
}

void EditorRenderer::renderScene(const MapData &mapData,
                                 const EditorCamera &camera,
                                 const std::string &selectedId,
                                 int selectedTrackPoint) {
  SDL_SetRenderDrawColor(m_renderer, 18, 22, 30, 255);
  SDL_FRect viewport{static_cast<float>(m_viewportX),
                     static_cast<float>(m_viewportY),
                     static_cast<float>(m_viewportWidth),
                     static_cast<float>(m_viewportHeight)};
  SDL_RenderFillRect(m_renderer, &viewport);

  drawGrid(camera);
  drawBoundary(mapData, camera);
  drawTrack(mapData, camera, selectedTrackPoint);

  for (const MapObject &object : mapData.objects) {
    drawObject(object, camera, object.id == selectedId);
  }
}

void EditorRenderer::drawGrid(const EditorCamera &camera) {
  const int halfSize = 20;
  const float spacing = 10.0f;
  for (int i = -halfSize; i <= halfSize; ++i) {
    const float coord = i * spacing;
    const bool major = (i % 5) == 0;
    const Uint8 shade = major ? 70 : 45;
    drawLine3D(glm::vec3(-halfSize * spacing, 0.0f, coord),
               glm::vec3(halfSize * spacing, 0.0f, coord), camera, shade, shade,
               shade + 10);
    drawLine3D(glm::vec3(coord, 0.0f, -halfSize * spacing),
               glm::vec3(coord, 0.0f, halfSize * spacing), camera, shade, shade,
               shade + 10);
  }

  drawLine3D(glm::vec3(0.0f, 0.0f, -halfSize * spacing),
             glm::vec3(0.0f, 0.0f, halfSize * spacing), camera, 90, 90, 140);
  drawLine3D(glm::vec3(-halfSize * spacing, 0.0f, 0.0f),
             glm::vec3(halfSize * spacing, 0.0f, 0.0f), camera, 140, 90, 90);
}

void EditorRenderer::drawBoundary(const MapData &mapData,
                                  const EditorCamera &camera) {
  const glm::vec3 minB = mapData.boundaryMin;
  const glm::vec3 maxB = mapData.boundaryMax;
  const glm::vec3 corners[8] = {
      {minB.x, minB.y, minB.z}, {maxB.x, minB.y, minB.z},
      {maxB.x, minB.y, maxB.z}, {minB.x, minB.y, maxB.z},
      {minB.x, maxB.y, minB.z}, {maxB.x, maxB.y, minB.z},
      {maxB.x, maxB.y, maxB.z}, {minB.x, maxB.y, maxB.z}};

  const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};

  for (const auto &edge : edges) {
    drawLine3D(corners[edge[0]], corners[edge[1]], camera, 60, 100, 130, 180);
  }
}

void EditorRenderer::drawObject(const MapObject &object,
                                const EditorCamera &camera, bool selected) {
  Uint8 r = 180, g = 180, b = 180;
  switch (object.type) {
  case MapObjectType::SpawnPoint:
    if (object.row <= 1) {
      r = 80;
      g = 255;
      b = 120; // ряд 1 — сильные / впереди
    } else {
      r = 80;
      g = 160;
      b = 255; // ряд 2 — сзади
    }
    break;
  case MapObjectType::Checkpoint:
    r = object.isFinishLine ? 80 : 80;
    g = object.isFinishLine ? 220 : 140;
    b = object.isFinishLine ? 220 : 255;
    break;
  case MapObjectType::PowerUpSpawn:
    switch (powerUpKindFromInt(object.subtype)) {
    case PowerUpKind::Missile:
      r = 255;
      g = 80;
      b = 80;
      break;
    case PowerUpKind::Mine:
      r = 180;
      g = 60;
      b = 200;
      break;
    case PowerUpKind::OilSlick:
      r = 80;
      g = 80;
      b = 80;
      break;
    case PowerUpKind::Shield:
      r = 80;
      g = 180;
      b = 255;
      break;
    case PowerUpKind::Nitro:
      r = 80;
      g = 255;
      b = 120;
      break;
    default:
      r = 255;
      g = 160;
      b = 40;
      break;
    }
    break;
  case MapObjectType::Destructible:
    r = 220;
    g = 70;
    b = 70;
    break;
  case MapObjectType::Obstacle:
    r = 160;
    g = 120;
    b = 80;
    break;
  case MapObjectType::Decoration:
    r = 150;
    g = 150;
    b = 170;
    break;
  }

  const float markerSize = selected ? 4.0f : 3.0f;
  drawCross3D(object.position, markerSize, camera, r, g, b);

  if (object.type == MapObjectType::Checkpoint) {
    drawCircle3D(object.position, object.radius, camera, r, g, b);
  }

  if (selected) {
    drawCircle3D(object.position, markerSize + 2.0f, camera, 255, 255, 255);
  }
}

void EditorRenderer::drawTrackSegment(const glm::vec3 &aLeft,
                                      const glm::vec3 &aRight,
                                      const glm::vec3 &bLeft,
                                      const glm::vec3 &bRight,
                                      const EditorCamera &camera, Uint8 r,
                                      Uint8 g, Uint8 bChannel, Uint8 aChannel) {
  drawLine3D((aLeft + aRight) * 0.5f, (bLeft + bRight) * 0.5f, camera, r, g,
             bChannel, aChannel);
  drawLine3D(aLeft, bLeft, camera, r, g, bChannel, aChannel);
  drawLine3D(aRight, bRight, camera, r, g, bChannel, aChannel);
}

void EditorRenderer::drawTrack(const MapData &mapData,
                               const EditorCamera &camera, int selectedPoint) {
  if (mapData.trackPoints.size() < 2) {
    return;
  }

  const int n = static_cast<int>(mapData.trackPoints.size());
  auto frame = [&](int i) {
    glm::vec3 prev =
        (i > 0) ? mapData.trackPoints[i - 1].position
                : mapData.trackPoints[mapData.trackClosed ? n - 1 : i].position;
    glm::vec3 here = mapData.trackPoints[i].position;
    glm::vec3 next =
        (i + 1 < n) ? mapData.trackPoints[i + 1].position
                    : mapData.trackPoints[mapData.trackClosed ? 0 : i].position;
    glm::vec3 forward, right;
    computeFrame(prev, here, next, forward, right);
    return std::pair<glm::vec3, glm::vec3>(forward, right);
  };

  const int segCount = mapData.trackClosed ? n : (n - 1);
  for (int i = 0; i < segCount; ++i) {
    const TrackPoint &a = mapData.trackPoints[i];
    const TrackPoint &b = mapData.trackPoints[(i + 1) % n];
    const auto fa = frame(i);
    const auto fb = frame((i + 1) % n);

    const float ah = a.width * 0.5f;
    const float bh = b.width * 0.5f;
    drawTrackSegment(a.position - fa.second * ah, a.position + fa.second * ah,
                     b.position - fb.second * bh, b.position + fb.second * bh,
                     camera, 90, 200, 90, 200);

    const float extA = ah + 4.0f;
    const float extB = bh + 4.0f;
    drawLine3D(a.position - fa.second * extA, b.position - fb.second * extB,
               camera, 110, 110, 110, 160);
    drawLine3D(a.position + fa.second * extA, b.position + fb.second * extB,
               camera, 110, 110, 110, 160);
  }

  for (int i = 0; i < n; ++i) {
    const TrackPoint &p = mapData.trackPoints[i];
    const bool isSelected = (i == selectedPoint);
    const Uint8 r = isSelected ? 255 : (i == 0 ? 255 : 180);
    const Uint8 g = isSelected ? 255 : (i == 0 ? 220 : 200);
    const Uint8 b = isSelected ? 255 : (i == 0 ? 120 : 80);
    const float size = isSelected ? 3.0f : 2.0f;
    drawLine3D(p.position + glm::vec3(0, size, 0),
               p.position + glm::vec3(size, 0, 0), camera, r, g, b);
    drawLine3D(p.position + glm::vec3(size, 0, 0),
               p.position + glm::vec3(0, -size, 0), camera, r, g, b);
    drawLine3D(p.position + glm::vec3(0, -size, 0),
               p.position + glm::vec3(-size, 0, 0), camera, r, g, b);
    drawLine3D(p.position + glm::vec3(-size, 0, 0),
               p.position + glm::vec3(0, size, 0), camera, r, g, b);
    if (isSelected) {
      const auto f = frame(i);
      const float half = p.width * 0.5f;
      drawLine3D(p.position - f.second * half, p.position + f.second * half,
                 camera, 255, 255, 80);
      drawCircle3D(p.position, 1.5f, camera, 255, 255, 80);
    }
    if (p.isFinish) {
      drawCircle3D(p.position, 2.5f, camera, 80, 220, 220);
    }
  }
}

void EditorRenderer::drawLine3D(const glm::vec3 &a, const glm::vec3 &b,
                                const EditorCamera &camera, Uint8 r, Uint8 g,
                                Uint8 bChannel, Uint8 aChannel) {
  glm::vec2 screenA;
  glm::vec2 screenB;
  if (!projectPoint(a, camera, screenA) || !projectPoint(b, camera, screenB)) {
    return;
  }

  SDL_SetRenderDrawColor(m_renderer, r, g, bChannel, aChannel);
  SDL_FPoint points[2] = {{m_viewportX + screenA.x, m_viewportY + screenA.y},
                          {m_viewportX + screenB.x, m_viewportY + screenB.y}};
  SDL_RenderLines(m_renderer, points, 2);
}

void EditorRenderer::drawCross3D(const glm::vec3 &center, float size,
                                 const EditorCamera &camera, Uint8 r, Uint8 g,
                                 Uint8 b) {
  drawLine3D(center + glm::vec3(-size, 0.0f, 0.0f),
             center + glm::vec3(size, 0.0f, 0.0f), camera, r, g, b);
  drawLine3D(center + glm::vec3(0.0f, -size, 0.0f),
             center + glm::vec3(0.0f, size, 0.0f), camera, r, g, b);
  drawLine3D(center + glm::vec3(0.0f, 0.0f, -size),
             center + glm::vec3(0.0f, 0.0f, size), camera, r, g, b);
}

void EditorRenderer::drawCircle3D(const glm::vec3 &center, float radius,
                                  const EditorCamera &camera, Uint8 r, Uint8 g,
                                  Uint8 b) {
  const int segments = 24;
  glm::vec3 previous = center + glm::vec3(radius, 0.0f, 0.0f);
  for (int i = 1; i <= segments; ++i) {
    const float angle = (static_cast<float>(i) / segments) * 6.2831853f;
    glm::vec3 point = center + glm::vec3(std::cos(angle) * radius, 0.0f,
                                         std::sin(angle) * radius);
    drawLine3D(previous, point, camera, r, g, b);
    previous = point;
  }
}

bool EditorRenderer::projectPoint(const glm::vec3 &world,
                                  const EditorCamera &camera,
                                  glm::vec2 &screen) const {
  if (!camera.isInFrontOfCamera(world)) {
    return false;
  }
  screen = camera.worldToScreen(world);
  return screen.x >= 0.0f && screen.y >= 0.0f &&
         screen.x <= static_cast<float>(m_viewportWidth) &&
         screen.y <= static_cast<float>(m_viewportHeight);
}

} // namespace Editor
