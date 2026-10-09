#pragma once

#include "seed/math/Math.h"
#include "seed/render/RenderResources.h"

#include <string>

namespace seed {

struct CameraComponent {
    float field_of_view_degrees{60.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};
    bool enabled{true};
    bool primary{true};
    bool editor_only{false};
};

struct MeshComponent {
    MeshHandle mesh{};
    bool visible{true};
    std::string asset_id{"builtin:cube"};
};

struct MaterialComponent {
    ShaderHandle shader{};
    TextureHandle base_color_texture{};
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    std::string asset_id{"builtin:seed_default"};
};

struct DirectionalLightComponent {
    Vec3 color{1.0f, 0.96f, 0.88f};
    float intensity{1.0f};
    bool enabled{true};
};

struct AmbientLightComponent {
    Vec3 color{0.72f, 0.82f, 1.0f};
    float intensity{0.22f};
    bool enabled{true};
};

} // namespace seed
