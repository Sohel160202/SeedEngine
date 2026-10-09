#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace seed {

constexpr float Pi = 3.14159265358979323846f;

constexpr float radians(float degrees) noexcept {
    return degrees * (Pi / 180.0f);
}

constexpr float degrees(float radians_value) noexcept {
    return radians_value * (180.0f / Pi);
}

struct Vec2 {
    float x{0.0f};
    float y{0.0f};
};

struct Vec3 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct Vec4 {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float w{0.0f};
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec3 operator-(const Vec3& a, const Vec3& b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3 operator*(const Vec3& v, float scalar) noexcept {
    return {v.x * scalar, v.y * scalar, v.z * scalar};
}

inline Vec3& operator+=(Vec3& a, const Vec3& b) noexcept {
    a = a + b;
    return a;
}

inline float dot(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

inline float length(const Vec3& v) noexcept {
    return std::sqrt(dot(v, v));
}

inline Vec3 normalize(const Vec3& v) noexcept {
    const float magnitude = length(v);
    return magnitude > 0.000001f ? v * (1.0f / magnitude) : Vec3{};
}

struct Mat4 {
    // Column-major storage, matching the convention used by Seed's current GPU backends.
    std::array<float, 16> values{};

    static Mat4 identity() noexcept {
        Mat4 result{};
        result.values[0] = 1.0f;
        result.values[5] = 1.0f;
        result.values[10] = 1.0f;
        result.values[15] = 1.0f;
        return result;
    }

    const float* data() const noexcept { return values.data(); }
    float* data() noexcept { return values.data(); }
};

inline Mat4 operator*(const Mat4& a, const Mat4& b) noexcept {
    Mat4 result{};
    for (std::size_t column = 0; column < 4; ++column) {
        for (std::size_t row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (std::size_t i = 0; i < 4; ++i) {
                sum += a.values[i * 4 + row] * b.values[column * 4 + i];
            }
            result.values[column * 4 + row] = sum;
        }
    }
    return result;
}

inline Vec4 operator*(const Mat4& matrix, const Vec4& value) noexcept {
    return {
        matrix.values[0] * value.x + matrix.values[4] * value.y + matrix.values[8] * value.z + matrix.values[12] * value.w,
        matrix.values[1] * value.x + matrix.values[5] * value.y + matrix.values[9] * value.z + matrix.values[13] * value.w,
        matrix.values[2] * value.x + matrix.values[6] * value.y + matrix.values[10] * value.z + matrix.values[14] * value.w,
        matrix.values[3] * value.x + matrix.values[7] * value.y + matrix.values[11] * value.z + matrix.values[15] * value.w,
    };
}

inline Mat4 translation_matrix(const Vec3& position) noexcept {
    Mat4 result = Mat4::identity();
    result.values[12] = position.x;
    result.values[13] = position.y;
    result.values[14] = position.z;
    return result;
}

inline Mat4 scale_matrix(const Vec3& scale) noexcept {
    Mat4 result = Mat4::identity();
    result.values[0] = scale.x;
    result.values[5] = scale.y;
    result.values[10] = scale.z;
    return result;
}

inline Mat4 rotation_x_matrix(float degrees_value) noexcept {
    const float angle = radians(degrees_value);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result = Mat4::identity();
    result.values[5] = c;
    result.values[6] = s;
    result.values[9] = -s;
    result.values[10] = c;
    return result;
}

inline Mat4 rotation_y_matrix(float degrees_value) noexcept {
    const float angle = radians(degrees_value);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result = Mat4::identity();
    result.values[0] = c;
    result.values[2] = -s;
    result.values[8] = s;
    result.values[10] = c;
    return result;
}

inline Mat4 rotation_z_matrix(float degrees_value) noexcept {
    const float angle = radians(degrees_value);
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result = Mat4::identity();
    result.values[0] = c;
    result.values[1] = s;
    result.values[4] = -s;
    result.values[5] = c;
    return result;
}

inline Mat4 transform_matrix(const Vec3& position, const Vec3& rotation_degrees, const Vec3& scale) noexcept {
    const Mat4 rotation =
        rotation_z_matrix(rotation_degrees.z) *
        rotation_y_matrix(rotation_degrees.y) *
        rotation_x_matrix(rotation_degrees.x);
    return translation_matrix(position) * rotation * scale_matrix(scale);
}

inline bool inverse_matrix(const Mat4& matrix, Mat4& result) noexcept {
    const float* m = matrix.values.data();
    float inv[16];

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
             m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
              m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
             m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    const float determinant = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(determinant) < 0.0000001f) return false;
    const float reciprocal = 1.0f / determinant;
    for (std::size_t index = 0; index < 16; ++index) result.values[index] = inv[index] * reciprocal;
    return true;
}

inline bool decompose_transform_matrix(
    const Mat4& matrix,
    Vec3& position,
    Vec3& rotation_degrees,
    Vec3& scale
) noexcept {
    position = {matrix.values[12], matrix.values[13], matrix.values[14]};

    const Vec3 column0{matrix.values[0], matrix.values[1], matrix.values[2]};
    const Vec3 column1{matrix.values[4], matrix.values[5], matrix.values[6]};
    const Vec3 column2{matrix.values[8], matrix.values[9], matrix.values[10]};
    scale = {length(column0), length(column1), length(column2)};
    if (scale.x < 0.000001f || scale.y < 0.000001f || scale.z < 0.000001f) return false;

    const float m00 = matrix.values[0] / scale.x;
    const float m10 = matrix.values[1] / scale.x;
    const float m20 = matrix.values[2] / scale.x;
    const float m21 = matrix.values[6] / scale.y;
    const float m22 = matrix.values[10] / scale.z;

    const float y = std::asin(std::clamp(-m20, -1.0f, 1.0f));
    const float cy = std::cos(y);
    float x = 0.0f;
    float z = 0.0f;
    if (std::fabs(cy) > 0.00001f) {
        x = std::atan2(m21, m22);
        z = std::atan2(m10, m00);
    } else {
        x = std::atan2(-matrix.values[9] / scale.z, matrix.values[5] / scale.y);
    }
    rotation_degrees = {degrees(x), degrees(y), degrees(z)};
    return true;
}

inline Mat4 perspective_matrix(float fov_y_degrees, float aspect, float near_plane, float far_plane) noexcept {
    Mat4 result{};
    const float safe_aspect = std::max(aspect, 0.0001f);
    const float tangent = std::tan(radians(fov_y_degrees) * 0.5f);
    const float f = tangent > 0.0f ? 1.0f / tangent : 1.0f;

    result.values[0] = f / safe_aspect;
    result.values[5] = f;
    result.values[10] = (far_plane + near_plane) / (near_plane - far_plane);
    result.values[11] = -1.0f;
    result.values[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    return result;
}

inline Mat4 orthographic_matrix(
    float left,
    float right,
    float bottom,
    float top,
    float near_plane,
    float far_plane
) noexcept {
    Mat4 result = Mat4::identity();
    const float width = std::max(right - left, 0.0001f);
    const float height = std::max(top - bottom, 0.0001f);
    const float depth = std::max(far_plane - near_plane, 0.0001f);

    result.values[0] = 2.0f / width;
    result.values[5] = 2.0f / height;
    result.values[10] = -2.0f / depth;
    result.values[12] = -(right + left) / width;
    result.values[13] = -(top + bottom) / height;
    result.values[14] = -(far_plane + near_plane) / depth;
    return result;
}

inline Mat4 look_at_matrix(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
    const Vec3 forward = normalize(target - eye);
    const Vec3 right = normalize(cross(forward, up));
    const Vec3 camera_up = cross(right, forward);

    Mat4 result = Mat4::identity();
    result.values[0] = right.x;
    result.values[4] = right.y;
    result.values[8] = right.z;

    result.values[1] = camera_up.x;
    result.values[5] = camera_up.y;
    result.values[9] = camera_up.z;

    result.values[2] = -forward.x;
    result.values[6] = -forward.y;
    result.values[10] = -forward.z;

    result.values[12] = -dot(right, eye);
    result.values[13] = -dot(camera_up, eye);
    result.values[14] = dot(forward, eye);
    return result;
}

inline Vec3 forward_from_euler(const Vec3& rotation_degrees) noexcept {
    const float pitch = radians(rotation_degrees.x);
    const float yaw = radians(rotation_degrees.y);
    return normalize({
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        -std::cos(pitch) * std::cos(yaw),
    });
}

inline Vec3 right_from_euler(const Vec3& rotation_degrees) noexcept {
    const Vec3 forward = forward_from_euler(rotation_degrees);
    return normalize(cross(forward, {0.0f, 1.0f, 0.0f}));
}

} // namespace seed
