#pragma once

#include "seed/render/RenderResources.h"

namespace seed {

struct CameraComponent {
    float field_of_view_degrees{60.0f};
    float near_plane{0.1f};
    float far_plane{1000.0f};
    bool enabled{true};
    bool primary{true};
};

struct MeshComponent {
    MeshHandle mesh{};
    bool visible{true};
};

struct MaterialComponent {
    ShaderHandle shader{};
};

} // namespace seed
