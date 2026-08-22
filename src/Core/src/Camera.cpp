#include "Core/Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace Core {

Camera::Camera() 
    : m_position(0.0f, 0.0f, 0.0f),
      m_target(0.0f, 0.0f, -1.0f),
      m_up(0.0f, 1.0f, 0.0f),
      m_forward(0.0f, 0.0f, -1.0f),
      m_right(1.0f, 0.0f, 0.0f),
      m_projectionType(ProjectionType::Perspective),
      m_fov(45.0f),
      m_aspect(16.0f / 9.0f),
      m_near(0.1f),
      m_far(1000.0f),
      m_left(-1.0f),
      m_rightOrtho(1.0f),
      m_bottom(-1.0f),
      m_top(1.0f) {
    
    updateMatrices();
}

Camera::~Camera() {
}

void Camera::setPosition(const glm::vec3& position) {
    m_position = position;
    updateVectors();
}

void Camera::setTarget(const glm::vec3& target) {
    m_target = target;
    updateVectors();
}

void Camera::setUp(const glm::vec3& up) {
    m_up = up;
    updateVectors();
}

void Camera::translate(const glm::vec3& translation) {
    m_position += translation;
    m_target += translation;
    updateVectors();
}

void Camera::rotate(float yaw, float pitch) {
    // TODO: Implement camera rotation
    updateVectors();
}

void Camera::orbit(const glm::vec3& center, float radius, float yaw, float pitch) {
    // TODO: Implement camera orbiting
    updateVectors();
}

void Camera::setProjectionType(ProjectionType type) {
    m_projectionType = type;
    updateMatrices();
}

void Camera::setPerspective(float fov, float aspect, float near, float far) {
    m_projectionType = ProjectionType::Perspective;
    m_fov = fov;
    m_aspect = aspect;
    m_near = near;
    m_far = far;
    updateMatrices();
}

void Camera::setOrthographic(float left, float right, float bottom, float top, float near, float far) {
    m_projectionType = ProjectionType::Orthographic;
    m_left = left;
    m_rightOrtho = right;
    m_bottom = bottom;
    m_top = top;
    m_near = near;
    m_far = far;
    updateMatrices();
}

void Camera::updateMatrices() {
    // Update view matrix
    m_viewMatrix = glm::lookAt(m_position, m_target, m_up);
    
    // Update projection matrix
    if (m_projectionType == ProjectionType::Perspective) {
        m_projectionMatrix = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
    } else {
        m_projectionMatrix = glm::ortho(m_left, m_rightOrtho, m_bottom, m_top, m_near, m_far);
    }
    
    // Update view-projection matrix
    m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
}

glm::vec3 Camera::screenToWorld(const glm::vec2& screenPos, float depth) const {
    // TODO: Implement screen to world conversion
    return glm::vec3(0.0f);
}

glm::vec2 Camera::worldToScreen(const glm::vec3& worldPos) const {
    // TODO: Implement world to screen conversion
    return glm::vec2(0.0f);
}

glm::vec3 Camera::screenToRay(const glm::vec2& screenPos) const {
    // TODO: Implement screen to ray conversion
    return glm::vec3(0.0f);
}

void Camera::updateVectors() {
    m_forward = glm::normalize(m_target - m_position);
    m_right = glm::normalize(glm::cross(m_forward, m_up));
    m_up = glm::normalize(glm::cross(m_right, m_forward));
}

} // namespace Core
