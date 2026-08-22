#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Core {

/**
 * 3D Camera for rendering
 * Supports both perspective and orthographic projections
 */
class Camera {
public:
    enum class ProjectionType {
        Perspective,
        Orthographic
    };

    Camera();
    ~Camera();

    // Position and orientation
    void setPosition(const glm::vec3& position);
    void setTarget(const glm::vec3& target);
    void setUp(const glm::vec3& up);
    
    // Movement
    void translate(const glm::vec3& translation);
    void rotate(float yaw, float pitch);
    void orbit(const glm::vec3& center, float radius, float yaw, float pitch);
    
    // Projection
    void setProjectionType(ProjectionType type);
    void setPerspective(float fov, float aspect, float near, float far);
    void setOrthographic(float left, float right, float bottom, float top, float near, float far);
    
    // Getters
    const glm::vec3& getPosition() const { return m_position; }
    const glm::vec3& getTarget() const { return m_target; }
    const glm::vec3& getUp() const { return m_up; }
    const glm::vec3& getForward() const { return m_forward; }
    const glm::vec3& getRight() const { return m_right; }
    
    const glm::mat4& getViewMatrix() const { return m_viewMatrix; }
    const glm::mat4& getProjectionMatrix() const { return m_projectionMatrix; }
    const glm::mat4& getViewProjectionMatrix() const { return m_viewProjectionMatrix; }
    
    ProjectionType getProjectionType() const { return m_projectionType; }
    
    // Update matrices
    void updateMatrices();
    
    // Screen space conversion
    glm::vec3 screenToWorld(const glm::vec2& screenPos, float depth = 1.0f) const;
    glm::vec2 worldToScreen(const glm::vec3& worldPos) const;
    
    // Ray casting
    glm::vec3 screenToRay(const glm::vec2& screenPos) const;

private:
    // Camera properties
    glm::vec3 m_position;
    glm::vec3 m_target;
    glm::vec3 m_up;
    glm::vec3 m_forward;
    glm::vec3 m_right;
    
    // Matrices
    glm::mat4 m_viewMatrix;
    glm::mat4 m_projectionMatrix;
    glm::mat4 m_viewProjectionMatrix;
    
    // Projection settings
    ProjectionType m_projectionType;
    float m_fov;
    float m_aspect;
    float m_near;
    float m_far;
    
    // Orthographic settings
    float m_left, m_rightOrtho, m_bottom, m_top;
    
    // Update vectors
    void updateVectors();
};

} // namespace Core
