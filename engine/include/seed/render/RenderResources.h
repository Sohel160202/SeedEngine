#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace seed {

struct ShaderHandle {
    std::uint32_t value{0};

    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr auto operator<=>(const ShaderHandle&) const = default;
};

struct MeshHandle {
    std::uint32_t value{0};

    constexpr explicit operator bool() const noexcept { return value != 0; }
    constexpr auto operator<=>(const MeshHandle&) const = default;
};

// First Seed vertex format. The renderer API owns this definition rather than
// exposing backend-specific vertex-array or buffer concepts to engine users.
struct VertexPositionColor {
    std::array<float, 3> position{};
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
};

struct ShaderDesc {
    std::string_view vertex_source;
    std::string_view fragment_source;
};

struct MeshDesc {
    std::span<const VertexPositionColor> vertices;
    std::span<const std::uint32_t> indices;
};

} // namespace seed
