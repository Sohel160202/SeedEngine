#pragma once

#include "seed/core/Types.h"
#include "seed/math/Math.h"

namespace seed {

class Scene;

class PhysicsSystem {
public:
    struct CharacterState {
        float vertical_velocity{0.0f};
        bool grounded{false};
    };

    static void initialize_character(Scene& scene, EntityId entity, CharacterState& state);

    static void move_character(
        Scene& scene,
        EntityId entity,
        const Vec3& horizontal_delta,
        bool jump_requested,
        double delta_seconds,
        CharacterState& state
    );

    // Clips a desired camera position against Seed solid BoxColliders. `start`
    // is normally the player's camera anchor and `desired` is the orbit result.
    // The query expands solids by `probe_radius` so the camera keeps a small
    // safety volume rather than behaving like an infinitely thin ray.
    static Vec3 clip_camera_position(
        const Scene& scene,
        EntityId ignored_entity,
        const Vec3& start,
        const Vec3& desired,
        float probe_radius = 0.15f,
        float surface_padding = 0.05f
    );
};

} // namespace seed
