#pragma once

#include "seed/math/Math.h"

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace seed {

struct ShaderHandle {
    std::uint32_t value{0};

    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr bool operator==(const ShaderHandle&) const = default;
};

struct MeshHandle {
    std::uint32_t value{0};

    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr bool operator==(const MeshHandle&) const = default;
};

struct TextureHandle {
    std::uint32_t value{0};

    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr bool operator==(const TextureHandle&) const = default;
};

// Seed's first general-purpose static mesh vertex. The historical name is kept
// for source compatibility while the layout grows with normals + UVs.
struct VertexPositionColor {
    std::array<float, 3> position{};
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    std::array<float, 2> texcoord{};
    std::array<float, 3> normal{0.0f, 1.0f, 0.0f};
};

struct MaterialSurface {
    float metallic{0.0f};
    float roughness{0.65f};
};

struct SceneLighting {
    Vec3 directional_direction{0.0f, -1.0f, 0.0f};
    Vec3 directional_color{1.0f, 1.0f, 1.0f};
    float directional_intensity{0.0f};

    Vec3 ambient_color{1.0f, 1.0f, 1.0f};
    float ambient_intensity{0.18f};

    Vec3 camera_position{};
    Mat4 light_view_projection{Mat4::identity()};
    bool shadows_enabled{false};
};

struct SkySettings {
    Vec3 zenith_color{0.08f, 0.20f, 0.42f};
    Vec3 horizon_color{0.62f, 0.76f, 0.92f};
    float intensity{1.0f};
    bool enabled{false};
};

struct ShaderDesc {
    std::string_view vertex_source;
    std::string_view fragment_source;
};

struct MeshDesc {
    std::span<const VertexPositionColor> vertices;
    std::span<const std::uint32_t> indices;
};

struct TextureDesc {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::span<const std::uint8_t> rgba8_pixels;
};

} // namespace seed
