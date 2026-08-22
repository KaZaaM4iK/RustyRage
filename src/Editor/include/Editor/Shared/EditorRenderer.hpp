#pragma once

#include <SDL3/SDL.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

struct SDL_Renderer;

namespace Editor {

class EditorCamera;
struct MapData;
struct MapObject;
struct TrackPoint;

class EditorRenderer {
public:
    explicit EditorRenderer(SDL_Renderer* renderer);

    void setViewport(int x, int y, int width, int height);
    void renderScene(const MapData& mapData, const EditorCamera& camera,
                     const std::string& selectedId,
                     int selectedTrackPoint = -1);

private:
    SDL_Renderer* m_renderer;
    int m_viewportX;
    int m_viewportY;
    int m_viewportWidth;
    int m_viewportHeight;

    void drawGrid(const EditorCamera& camera);
    void drawBoundary(const MapData& mapData, const EditorCamera& camera);
    void drawObject(const MapObject& object, const EditorCamera& camera, bool selected);
    void drawTrack(const MapData& mapData, const EditorCamera& camera, int selectedPoint);
    void drawTrackSegment(const glm::vec3& aLeft, const glm::vec3& aRight,
                          const glm::vec3& bLeft, const glm::vec3& bRight,
                          const EditorCamera& camera,
                          Uint8 r, Uint8 g, Uint8 b, Uint8 a);
    void drawLine3D(const glm::vec3& a, const glm::vec3& b, const EditorCamera& camera,
                     Uint8 red, Uint8 green, Uint8 blue, Uint8 alpha = 255);

    void drawCross3D(const glm::vec3& center, float size, const EditorCamera& camera,
                     Uint8 r, Uint8 g, Uint8 b);
    void drawCircle3D(const glm::vec3& center, float radius, const EditorCamera& camera,
                      Uint8 r, Uint8 g, Uint8 b);
    bool projectPoint(const glm::vec3& world, const EditorCamera& camera, glm::vec2& screen) const;
};

} // namespace Editor
