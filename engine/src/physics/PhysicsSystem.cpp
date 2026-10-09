#include "seed/physics/PhysicsSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace seed {
namespace {

constexpr float ContactEpsilon = 0.001f;
constexpr float GroundProbeDistance = 0.08f;

struct Aabb {
    Vec3 min{};
    Vec3 max{};
};

struct CharacterGeometry {
    float radius{0.35f};
    float height{1.8f};
    float eye_height{1.65f};
};

CharacterGeometry character_geometry(const CharacterBodyComponent& body) {
    CharacterGeometry result;
    result.radius = std::max(0.05f, body.radius);
    result.height = std::max(body.height, result.radius * 2.0f);
    result.eye_height = std::clamp(body.eye_height, result.radius, result.height - 0.05f);
    return result;
}

Aabb character_aabb(const Vec3& eye_position, const CharacterGeometry& geometry) {
    const float feet_y = eye_position.y - geometry.eye_height;
    const float top_y = feet_y + geometry.height;
    return {
        .min = {eye_position.x - geometry.radius, feet_y, eye_position.z - geometry.radius},
        .max = {eye_position.x + geometry.radius, top_y, eye_position.z + geometry.radius},
    };
}

Aabb box_aabb(const TransformComponent& transform, const BoxColliderComponent& collider) {
    const float local_x = std::fabs(transform.scale.x) * std::max(0.001f, collider.half_extents.x);
    const float local_y = std::fabs(transform.scale.y) * std::max(0.001f, collider.half_extents.y);
    const float local_z = std::fabs(transform.scale.z) * std::max(0.001f, collider.half_extents.z);

    // Physics v0 models box rotation using a yaw-aware enclosing AABB. This is
    // intentionally conservative and keeps rotating doors useful before Seed's
    // future oriented-shape/narrow-phase physics work lands.
    const float yaw = radians(transform.rotation_degrees.y);
    const float c = std::fabs(std::cos(yaw));
    const float s = std::fabs(std::sin(yaw));
    const Vec3 half{
        c * local_x + s * local_z,
        local_y,
        s * local_x + c * local_z,
    };

    const Vec3 center = transform.position + collider.offset;
    return {
        .min = {center.x - half.x, center.y - half.y, center.z - half.z},
        .max = {center.x + half.x, center.y + half.y, center.z + half.z},
    };
}

bool overlaps(float a_min, float a_max, float b_min, float b_max) {
    return a_max > b_min + ContactEpsilon && a_min < b_max - ContactEpsilon;
}

bool overlaps_xz(const Aabb& a, const Aabb& b) {
    return overlaps(a.min.x, a.max.x, b.min.x, b.max.x) &&
        overlaps(a.min.z, a.max.z, b.min.z, b.max.z);
}

bool overlaps_yz(const Aabb& a, const Aabb& b) {
    return overlaps(a.min.y, a.max.y, b.min.y, b.max.y) &&
        overlaps(a.min.z, a.max.z, b.min.z, b.max.z);
}

bool overlaps_xy(const Aabb& a, const Aabb& b) {
    return overlaps(a.min.x, a.max.x, b.min.x, b.max.x) &&
        overlaps(a.min.y, a.max.y, b.min.y, b.max.y);
}

bool grounded_on_static_box(
    const Scene& scene,
    EntityId character_entity,
    const Vec3& eye_position,
    const CharacterGeometry& geometry
) {
    const Aabb character = character_aabb(eye_position, geometry);
    bool grounded = false;

    scene.for_each<TransformComponent, BoxColliderComponent>(
        [&](EntityId entity, const TransformComponent& transform, const BoxColliderComponent& collider) {
            if (grounded || entity == character_entity || !collider.enabled || !collider.solid) {
                return;
            }

            const Aabb box = box_aabb(transform, collider);
            if (!overlaps_xz(character, box)) {
                return;
            }

            const float distance_to_top = character.min.y - box.max.y;
            if (distance_to_top >= -ContactEpsilon && distance_to_top <= GroundProbeDistance) {
                grounded = true;
            }
        }
    );

    return grounded;
}

void depenetrate_floor_spawn(
    Scene& scene,
    EntityId character_entity,
    Vec3& eye_position,
    const CharacterGeometry& geometry
) {
    float corrected_y = eye_position.y;
    Aabb character = character_aabb(eye_position, geometry);

    scene.for_each<TransformComponent, BoxColliderComponent>(
        [&](EntityId entity, const TransformComponent& transform, const BoxColliderComponent& collider) {
            if (entity == character_entity || !collider.enabled || !collider.solid) {
                return;
            }

            const Aabb box = box_aabb(transform, collider);
            if (!overlaps_xz(character, box)) {
                return;
            }

            // If the eye is approximately at/above the top of a box but the body
            // starts embedded in it, treat the surface as a floor and stand up.
            if (character.min.y < box.max.y &&
                character.max.y > box.max.y &&
                eye_position.y >= box.max.y - 0.2f) {
                corrected_y = std::max(corrected_y, box.max.y + geometry.eye_height);
            }
        }
    );

    eye_position.y = corrected_y;
}

void move_horizontal_axis(
    Scene& scene,
    EntityId character_entity,
    Vec3& eye_position,
    const CharacterGeometry& geometry,
    float delta,
    bool x_axis
) {
    if (std::fabs(delta) <= 0.000001f) {
        return;
    }

    const Vec3 start_position = eye_position;
    const Aabb start = character_aabb(start_position, geometry);
    float target = (x_axis ? start_position.x : start_position.z) + delta;

    scene.for_each<TransformComponent, BoxColliderComponent>(
        [&](EntityId entity, const TransformComponent& transform, const BoxColliderComponent& collider) {
            if (entity == character_entity || !collider.enabled || !collider.solid) {
                return;
            }

            Vec3 candidate_position = start_position;
            if (x_axis) {
                candidate_position.x = target;
            } else {
                candidate_position.z = target;
            }
            const Aabb candidate = character_aabb(candidate_position, geometry);
            const Aabb box = box_aabb(transform, collider);

            if (x_axis) {
                if (!overlaps_yz(candidate, box)) {
                    return;
                }
                if (delta > 0.0f && start.max.x <= box.min.x + ContactEpsilon && candidate.max.x > box.min.x) {
                    target = std::min(target, box.min.x - geometry.radius);
                } else if (delta < 0.0f && start.min.x >= box.max.x - ContactEpsilon && candidate.min.x < box.max.x) {
                    target = std::max(target, box.max.x + geometry.radius);
                }
            } else {
                if (!overlaps_xy(candidate, box)) {
                    return;
                }
                if (delta > 0.0f && start.max.z <= box.min.z + ContactEpsilon && candidate.max.z > box.min.z) {
                    target = std::min(target, box.min.z - geometry.radius);
                } else if (delta < 0.0f && start.min.z >= box.max.z - ContactEpsilon && candidate.min.z < box.max.z) {
                    target = std::max(target, box.max.z + geometry.radius);
                }
            }
        }
    );

    if (x_axis) {
        eye_position.x = target;
    } else {
        eye_position.z = target;
    }
}

bool move_vertical(
    Scene& scene,
    EntityId character_entity,
    Vec3& eye_position,
    const CharacterGeometry& geometry,
    float delta_y,
    bool& hit_ceiling
) {
    if (std::fabs(delta_y) <= 0.000001f) {
        hit_ceiling = false;
        return false;
    }

    const Vec3 start_position = eye_position;
    const Aabb start = character_aabb(start_position, geometry);
    float target_y = start_position.y + delta_y;
    bool landed = false;
    hit_ceiling = false;

    scene.for_each<TransformComponent, BoxColliderComponent>(
        [&](EntityId entity, const TransformComponent& transform, const BoxColliderComponent& collider) {
            if (entity == character_entity || !collider.enabled || !collider.solid) {
                return;
            }

            Vec3 candidate_position = start_position;
            candidate_position.y = target_y;
            const Aabb candidate = character_aabb(candidate_position, geometry);
            const Aabb box = box_aabb(transform, collider);
            if (!overlaps_xz(candidate, box)) {
                return;
            }

            if (delta_y < 0.0f &&
                start.min.y >= box.max.y - ContactEpsilon &&
                candidate.min.y < box.max.y) {
                target_y = std::max(target_y, box.max.y + geometry.eye_height);
                landed = true;
            } else if (delta_y > 0.0f &&
                start.max.y <= box.min.y + ContactEpsilon &&
                candidate.max.y > box.min.y) {
                const float head_above_eye = geometry.height - geometry.eye_height;
                target_y = std::min(target_y, box.min.y - head_above_eye);
                hit_ceiling = true;
            }
        }
    );

    eye_position.y = target_y;
    return landed;
}

} // namespace

void PhysicsSystem::initialize_character(Scene& scene, EntityId entity, CharacterState& state) {
    state = {};

    auto* transform = scene.get_component<TransformComponent>(entity);
    const auto* body = scene.get_component<CharacterBodyComponent>(entity);
    if (transform == nullptr || body == nullptr || !body->enabled) {
        return;
    }

    const CharacterGeometry geometry = character_geometry(*body);
    depenetrate_floor_spawn(scene, entity, transform->position, geometry);
    state.grounded = grounded_on_static_box(scene, entity, transform->position, geometry);
}

void PhysicsSystem::move_character(
    Scene& scene,
    EntityId entity,
    const Vec3& horizontal_delta,
    bool jump_requested,
    double delta_seconds,
    CharacterState& state
) {
    auto* transform = scene.get_component<TransformComponent>(entity);
    auto* body = scene.get_component<CharacterBodyComponent>(entity);
    if (transform == nullptr || body == nullptr || !body->enabled) {
        return;
    }

    const CharacterGeometry geometry = character_geometry(*body);
    const float dt = std::clamp(static_cast<float>(delta_seconds), 0.0f, 0.1f);

    // Resolve horizontal motion one axis at a time. This gives intuitive wall
    // sliding for Seed's first character controller without a full rigid-body solver.
    move_horizontal_axis(scene, entity, transform->position, geometry, horizontal_delta.x, true);
    move_horizontal_axis(scene, entity, transform->position, geometry, horizontal_delta.z, false);

    state.grounded = grounded_on_static_box(scene, entity, transform->position, geometry);

    if (jump_requested && state.grounded) {
        state.vertical_velocity = std::max(0.0f, body->jump_speed);
        state.grounded = false;
    }

    if (state.grounded && state.vertical_velocity <= 0.0f) {
        state.vertical_velocity = 0.0f;
    } else {
        const float gravity = std::max(0.0f, body->gravity);
        const float max_fall_speed = std::max(0.0f, body->max_fall_speed);
        state.vertical_velocity = std::max(
            state.vertical_velocity - gravity * dt,
            -max_fall_speed
        );
    }

    bool hit_ceiling = false;
    const bool landed = move_vertical(
        scene,
        entity,
        transform->position,
        geometry,
        state.vertical_velocity * dt,
        hit_ceiling
    );

    if (landed || hit_ceiling) {
        state.vertical_velocity = 0.0f;
    }

    state.grounded = landed || grounded_on_static_box(scene, entity, transform->position, geometry);
}

} // namespace seed
