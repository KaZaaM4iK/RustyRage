#pragma once

#include <array>
#include <cmath>

namespace rr::render {

/// Minimal column-major 4x4 matrix used by the foundation graphics smoke test.
/// This is deliberately local to rr_render; no math library dependency is added
/// for a single camera/projection path.
struct Mat4 {
    std::array<float, 16> values{};

    [[nodiscard]] static Mat4 identity() {
        Mat4 result;
        result.values[0] = 1.0f;
        result.values[5] = 1.0f;
        result.values[10] = 1.0f;
        result.values[15] = 1.0f;
        return result;
    }

    [[nodiscard]] static Mat4 translation(float x, float y, float z) {
        Mat4 result = identity();
        result.values[12] = x;
        result.values[13] = y;
        result.values[14] = z;
        return result;
    }

    [[nodiscard]] static Mat4 rotation_x(float radians) {
        Mat4 result = identity();
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        result.values[5] = cosine;
        result.values[6] = sine;
        result.values[9] = -sine;
        result.values[10] = cosine;
        return result;
    }

    [[nodiscard]] static Mat4 rotation_y(float radians) {
        Mat4 result = identity();
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        result.values[0] = cosine;
        result.values[2] = -sine;
        result.values[8] = sine;
        result.values[10] = cosine;
        return result;
    }

    [[nodiscard]] static Mat4 perspective(float fov_y_radians,
                                          float aspect,
                                          float near_plane,
                                          float far_plane) {
        Mat4 result;
        const float focal = 1.0f / std::tan(fov_y_radians * 0.5f);
        result.values[0] = focal / aspect;
        result.values[5] = focal;
        result.values[10] = (far_plane + near_plane) / (near_plane - far_plane);
        result.values[11] = -1.0f;
        result.values[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
        return result;
    }

    [[nodiscard]] static Mat4 look_at(float eye_x,
                                      float eye_y,
                                      float eye_z,
                                      float target_x,
                                      float target_y,
                                      float target_z,
                                      float up_x,
                                      float up_y,
                                      float up_z) {
        const float forward_x = target_x - eye_x;
        const float forward_y = target_y - eye_y;
        const float forward_z = target_z - eye_z;
        const float forward_length = std::sqrt(forward_x * forward_x +
                                               forward_y * forward_y +
                                               forward_z * forward_z);
        const float fx = forward_x / forward_length;
        const float fy = forward_y / forward_length;
        const float fz = forward_z / forward_length;

        const float side_x = fy * up_z - fz * up_y;
        const float side_y = fz * up_x - fx * up_z;
        const float side_z = fx * up_y - fy * up_x;
        const float side_length = std::sqrt(side_x * side_x + side_y * side_y + side_z * side_z);
        const float sx = side_x / side_length;
        const float sy = side_y / side_length;
        const float sz = side_z / side_length;

        const float ux = sy * fz - sz * fy;
        const float uy = sz * fx - sx * fz;
        const float uz = sx * fy - sy * fx;

        Mat4 result = identity();
        result.values[0] = sx;
        result.values[1] = ux;
        result.values[2] = -fx;
        result.values[4] = sy;
        result.values[5] = uy;
        result.values[6] = -fy;
        result.values[8] = sz;
        result.values[9] = uz;
        result.values[10] = -fz;
        result.values[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);
        result.values[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);
        result.values[14] = fx * eye_x + fy * eye_y + fz * eye_z;
        return result;
    }
};

[[nodiscard]] inline Mat4 operator*(const Mat4& left, const Mat4& right) {
    Mat4 result;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int index = 0; index < 4; ++index) {
                sum += left.values[index * 4 + row] * right.values[column * 4 + index];
            }
            result.values[column * 4 + row] = sum;
        }
    }
    return result;
}

} // namespace rr::render
