#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Core {
class InputManager;
}

namespace Editor {

class EditorCamera {
public:
    EditorCamera();

    void setAspect(float aspect);
    void setViewportSize(int width, int height);

    void processInput(Core::InputManager& input, float deltaTime, bool allowMouseLook);
    void focusOn(const glm::vec3& target, float distance = 40.0f);

    const glm::vec3& getPosition() const { return m_position; }
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }
    const glm::mat4& getViewProjectionMatrix() const { return m_viewProjectionMatrix; }

    glm::vec2 worldToScreen(const glm::vec3& worldPos) const;
    bool screenToGround(const glm::vec2& screenPos, glm::vec3& hitPoint) const;
    bool isInFrontOfCamera(const glm::vec3& worldPos) const;

private:
    glm::vec3 m_position;
    float m_yaw;
    float m_pitch;
    float m_moveSpeed;
    float m_lookSpeed;
    float m_fov;
    float m_aspect;
    float m_near;
    float m_far;

    int m_viewportWidth;
    int m_viewportHeight;

    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewProjectionMatrix;

    void updateMatrices();
    glm::vec3 getForward() const;
    glm::vec3 getRight() const;
};

} // namespace Editor
