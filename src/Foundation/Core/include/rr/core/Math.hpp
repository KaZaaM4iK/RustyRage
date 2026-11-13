#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// GLM version guard. TASK_002 pins GLM 1.0.3 through CMake FetchContent.
static_assert(GLM_VERSION_MAJOR == 1 && GLM_VERSION_MINOR == 0 && GLM_VERSION_PATCH == 3,
              "RustyRage requires the pinned GLM 1.0.3 headers");

namespace rr::math {

using Vec2 = glm::vec2;
using Vec3 = glm::vec3;
using Vec4 = glm::vec4;
using Quat = glm::quat;
using Mat4 = glm::mat4;

inline constexpr float kPi = 3.14159265358979323846f;

} // namespace rr::math
