#pragma once

#include "seed/animation/AnimationData.h"
#include "seed/render/RenderResources.h"

#include <string>

namespace seed {

struct AnimatedModelAssetResource;

// Author-facing character render component. Persistent state is asset_id and
// the ordinary visibility/shadow switches. GPU handles and runtime_asset are
// hydrated by AssetRuntime after project/scene load and are never authoritative
// scene data.
struct SkinnedMeshComponent {
    MeshHandle mesh{};
    std::string asset_id;

    bool visible{true};
    bool cast_shadows{true};
    bool receive_shadows{true};

    const AnimatedModelAssetResource* runtime_asset{nullptr};
};

// Seed-native animation playback state. Keeping playback separate from the mesh
// means future state machines, networking, ragdolls and mesh swaps can all reuse
// the same semantic animation layer.
struct AnimationPlayerComponent {
    // Empty means "use the first clip in the character asset".
    std::string clip_name;
    float time_seconds{0.0f};
    float playback_speed{1.0f};
    bool playing{true};

    // Runtime-evaluated pose. It is derived data and should not be serialized as
    // authored scene state.
    SkeletonPose pose;
};

} // namespace seed
