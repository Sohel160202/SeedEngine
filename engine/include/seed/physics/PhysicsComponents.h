#pragma once

#include "seed/math/Math.h"

namespace seed {

// Seed Physics v0 static collision shape. The box is expressed in local space;
// Transform scale is applied at runtime. Rotation is currently approximated as
// a yaw-aware world AABB, which is enough for floors, walls, sliding doors, and
// simple rotating doors during the first physics milestone.
struct BoxColliderComponent {
    Vec3 half_extents{0.75f, 0.75f, 0.75f};
    Vec3 offset{};
    bool enabled{true};
    bool solid{true};
};

// Beginner-facing first-person character body. The entity Transform represents
// the player's eye/camera position. The physical body extends down from that eye.
struct CharacterBodyComponent {
    float radius{0.35f};
    float height{1.8f};
    float eye_height{1.65f};
    float gravity{18.0f};
    float jump_speed{6.5f};
    float max_fall_speed{30.0f};
    bool enabled{true};
};

} // namespace seed
