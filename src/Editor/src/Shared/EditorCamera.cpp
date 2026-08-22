#include "Editor/Shared/EditorCamera.hpp"
#include "Core/InputManager.hpp"

#include <algorithm>
#include <cmath>

namespace Editor {

EditorCamera::EditorCamera()
    : m_position(0.0f, 40.0f, 80.0f), m_yaw(-90.0f), m_pitch(-25.0f),
      m_moveSpeed(60.0f), m_lookSpeed(0.15f), m_fov(60.0f),
      m_aspect(16.0f / 9.0f), m_near(0.1f), m_far(2000.0f),
      m_viewportWidth(1280), m_viewportHeight(720) {
  updateMatrices();
}

void EditorCamera::setAspect(float aspect) {
  m_aspect = std::max(0.1f, aspect);
  updateMatrices();
}

void EditorCamera::setViewportSize(int width, int height) {
  m_viewportWidth = std::max(1, width);
  m_viewportHeight = std::max(1, height);
  setAspect(static_cast<float>(m_viewportWidth) /
            static_cast<float>(m_viewportHeight));
}

void EditorCamera::processInput(Core::InputManager &input, float deltaTime,
                                bool allowMouseLook) {
  glm::vec3 movement(0.0f);
  if (input.isKeyPressed(SDL_SCANCODE_W))
    movement += getForward();
  if (input.isKeyPressed(SDL_SCANCODE_S))
    movement -= getForward();
  if (input.isKeyPressed(SDL_SCANCODE_A))
    movement -= getRight();
  if (input.isKeyPressed(SDL_SCANCODE_D))
    movement += getRight();
  if (input.isKeyPressed(SDL_SCANCODE_E) ||
      input.isKeyPressed(SDL_SCANCODE_SPACE))
    movement.y += 1.0f;
  if (input.isKeyPressed(SDL_SCANCODE_Q) ||
      input.isKeyPressed(SDL_SCANCODE_LCTRL))
    movement.y -= 1.0f;

  if (glm::length(movement) > 0.0f) {
    movement = glm::normalize(movement);
    float speedMultiplier =
        input.isKeyPressed(SDL_SCANCODE_LSHIFT) ? 3.0f : 1.0f;
    m_position += movement * m_moveSpeed * speedMultiplier * deltaTime;
  }

  if (allowMouseLook) {
    glm::vec2 delta = input.getMouseDelta();
    m_yaw += delta.x * m_lookSpeed;
    m_pitch -= delta.y * m_lookSpeed;
    m_pitch = std::clamp(m_pitch, -89.0f, 89.0f);
  }

  const float wheel = input.getMouseWheelDelta();
  if (wheel != 0.0f) {
    m_position += getForward() * wheel * 20.0f;
  }

  updateMatrices();
}

glm::vec2 EditorCamera::worldToScreen(const glm::vec3 &worldPos) const {
  glm::vec4 clip = m_viewProjectionMatrix * glm::vec4(worldPos, 1.0f);
  if (clip.w <= 0.001f) {
    return glm::vec2(-1.0f);
  }
  glm::vec3 ndc = glm::vec3(clip) / clip.w;
  return glm::vec2((ndc.x + 1.0f) * 0.5f * static_cast<float>(m_viewportWidth),
                   (1.0f - ndc.y) * 0.5f *
                       static_cast<float>(m_viewportHeight));
}

bool EditorCamera::screenToGround(const glm::vec2 &screenPos,
                                  glm::vec3 &hitPoint) const {
  const float x =
      (2.0f * screenPos.x) / static_cast<float>(m_viewportWidth) - 1.0f;
  const float y =
      1.0f - (2.0f * screenPos.y) / static_cast<float>(m_viewportHeight);

  glm::vec4 rayClip(x, y, -1.0f, 1.0f);
  glm::vec4 rayEye = glm::inverse(m_projectionMatrix) * rayClip;
  rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
  glm::vec3 rayWorld = glm::vec3(glm::inverse(m_viewMatrix) * rayEye);
  if (glm::length(rayWorld) < 0.0001f) {
    return false;
  }
  rayWorld = glm::normalize(rayWorld);

  if (std::abs(rayWorld.y) < 0.0001f) {
    return false;
  }

  const float t = -m_position.y / rayWorld.y;
  if (t < 0.0f) {
    return false;
  }

  hitPoint = m_position + rayWorld * t;
  return true;
}

bool EditorCamera::isInFrontOfCamera(const glm::vec3 &worldPos) const {
  glm::vec4 clip = m_viewProjectionMatrix * glm::vec4(worldPos, 1.0f);
  return clip.w > 0.001f;
}

void EditorCamera::updateMatrices() {
  const float yawRad = glm::radians(m_yaw);
  const float pitchRad = glm::radians(m_pitch);

  glm::vec3 forward;
  forward.x = std::cos(yawRad) * std::cos(pitchRad);
  forward.y = std::sin(pitchRad);
  forward.z = std::sin(yawRad) * std::cos(pitchRad);
  forward = glm::normalize(forward);

  m_viewMatrix = glm::lookAt(m_position, m_position + forward,
                             glm::vec3(0.0f, 1.0f, 0.0f));
  m_projectionMatrix =
      glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
  m_viewProjectionMatrix = m_projectionMatrix * m_viewMatrix;
}

glm::vec3 EditorCamera::getForward() const {
  const float yawRad = glm::radians(m_yaw);
  const float pitchRad = glm::radians(m_pitch);
  return glm::normalize(glm::vec3(std::cos(yawRad) * std::cos(pitchRad),
                                  std::sin(pitchRad),
                                  std::sin(yawRad) * std::cos(pitchRad)));
}

glm::vec3 EditorCamera::getRight() const {
  return glm::normalize(glm::cross(getForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
}

void EditorCamera::focusOn(const glm::vec3 &target, float distance) {
  const float yawRad = glm::radians(m_yaw);
  const float pitchRad = glm::radians(m_pitch);
  glm::vec3 forward(std::cos(yawRad) * std::cos(pitchRad), std::sin(pitchRad),
                    std::sin(yawRad) * std::cos(pitchRad));
  forward = glm::normalize(forward);
  m_position = target - forward * distance;
  updateMatrices();
}

} // namespace Editor
