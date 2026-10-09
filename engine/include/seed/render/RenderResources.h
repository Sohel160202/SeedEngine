#pragma once

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

// Seed's first general-purpose static mesh vertex. Existing built-in geometry
// can omit texcoord and keep using vertex color; imported glTF meshes populate
// both so materials can use base-color textures.
struct VertexPositionColor {
    std::array<float, 3> position{};
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    std::array<float, 2> texcoord{};
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
