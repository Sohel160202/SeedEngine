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
};

} // namespace seed
