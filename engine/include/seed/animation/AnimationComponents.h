#pragma once

#include "seed/animation/AnimationData.h"
#include "seed/render/RenderResources.h"

#include <string>
#include <vector>

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
struct AnimationEvent {
    // Empty clip_name means the event follows the currently selected clip.
    std::string clip_name;
    std::string name{"Event"};
    float time_seconds{0.0f};
};

struct AnimationPlayerComponent {
    // Empty means "use the first clip in the character asset".
    std::string clip_name;
    float time_seconds{0.0f};
    float playback_speed{1.0f};
    bool playing{true};
    float blend_duration_seconds{0.15f};

    // When enabled, gameplay drives conventional Idle/Walk/Run/Jump/Fall
    // clip selection. Manual clip selection remains available when disabled.
    bool locomotion_enabled{false};

    // Runtime locomotion signals published by GameplayRuntime.
    float locomotion_speed{0.0f};
    float locomotion_vertical_velocity{0.0f};
    bool locomotion_grounded{true};
    bool locomotion_sprinting{false};

    // Authored timing markers. They remain asset-independent and are saved with
    // the scene so imported clips can be swapped without changing gameplay code.
    std::vector<AnimationEvent> events;

    // Runtime events emitted during the most recent update. Gameplay can consume
    // and clear these names without requiring callbacks or external assets.
    std::vector<std::string> triggered_events;

    // Runtime crossfade state.
    std::string active_clip_name;
    float blend_elapsed_seconds{0.0f};
    SkeletonPose blend_from_pose;

    // Runtime-evaluated pose. It is derived data and should not be serialized as
    // authored scene state.
    SkeletonPose pose;
};

} // namespace seed
