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
    bool cast_shadows{true};
    bool receive_shadows{true};
    std::string asset_id{"builtin:cube"};
};

struct MaterialComponent {
    ShaderHandle shader{};
    TextureHandle base_color_texture{};
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.65f};
    std::string asset_id{"builtin:seed_default"};

    // Old Seed scenes only stored the material asset ID. AssetRuntime uses this
    // runtime flag once to hydrate their material values from the source glTF.
    bool use_asset_defaults{false};
};

struct DirectionalLightComponent {
    Vec3 color{1.0f, 0.96f, 0.88f};
    float intensity{1.0f};
    bool enabled{true};
    bool casts_shadows{true};
    float shadow_distance{35.0f};
};

struct AmbientLightComponent {
    Vec3 color{0.72f, 0.82f, 1.0f};
    float intensity{0.22f};
    bool enabled{true};
};

struct SkyComponent {
    Vec3 zenith_color{0.08f, 0.20f, 0.42f};
    Vec3 horizon_color{0.62f, 0.76f, 0.92f};
    float intensity{1.0f};
    bool enabled{true};
};

} // namespace seed
