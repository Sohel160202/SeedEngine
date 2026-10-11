#include "seed/physics/PhysicsSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/scene/Hierarchy.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace seed {
namespace {

struct Aabb {
    Vec3 min{};
    Vec3 max{};
};

Aabb box_aabb(const TransformComponent& transform, const BoxColliderComponent& collider) {
    const float local_x = std::fabs(transform.scale.x) * std::max(0.001f, collider.half_extents.x);
    const float local_y = std::fabs(transform.scale.y) * std::max(0.001f, collider.half_extents.y);
    const float local_z = std::fabs(transform.scale.z) * std::max(0.001f, collider.half_extents.z);

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

bool segment_aabb_entry(
    const Vec3& start,
    const Vec3& delta,
    const Aabb& box,
    float& entry_t
) {
    float t_min = 0.0f;
    float t_max = 1.0f;

    const auto slab = [&](float start_axis, float delta_axis, float min_axis, float max_axis) -> bool {
        if (std::fabs(delta_axis) <= 0.000001f) {
            return start_axis >= min_axis && start_axis <= max_axis;
        }

        const float inverse = 1.0f / delta_axis;
        float t1 = (min_axis - start_axis) * inverse;
        float t2 = (max_axis - start_axis) * inverse;
        if (t1 > t2) std::swap(t1, t2);
        t_min = std::max(t_min, t1);
        t_max = std::min(t_max, t2);
        return t_min <= t_max;
    };

    if (!slab(start.x, delta.x, box.min.x, box.max.x)) return false;
    if (!slab(start.y, delta.y, box.min.y, box.max.y)) return false;
    if (!slab(start.z, delta.z, box.min.z, box.max.z)) return false;

    entry_t = std::clamp(t_min, 0.0f, 1.0f);
    return t_max >= 0.0f && t_min <= 1.0f;
}

} // namespace

Vec3 PhysicsSystem::clip_camera_position(
    const Scene& scene,
    EntityId ignored_entity,
    const Vec3& start,
    const Vec3& desired,
    float probe_radius,
    float surface_padding
) {
    const Vec3 delta = desired - start;
    const float segment_length = length(delta);
    if (segment_length <= 0.000001f) return desired;

    const float radius = std::max(0.0f, probe_radius);
    float closest_t = 1.0f;

    scene.for_each<TransformComponent, BoxColliderComponent>(
        [&](EntityId entity, const TransformComponent&, const BoxColliderComponent& collider) {
            if (entity == ignored_entity || !collider.enabled || !collider.solid) return;

            const TransformComponent world = world_transform(scene, entity);
            Aabb box = box_aabb(world, collider);
            box.min.x -= radius; box.min.y -= radius; box.min.z -= radius;
            box.max.x += radius; box.max.y += radius; box.max.z += radius;

            float entry_t = 1.0f;
            if (segment_aabb_entry(start, delta, box, entry_t)) {
                closest_t = std::min(closest_t, entry_t);
            }
        }
    );

    if (closest_t >= 1.0f) return desired;

    const float padding_t = std::max(0.0f, surface_padding) / segment_length;
    const float safe_t = std::max(0.0f, closest_t - padding_t);
    return start + delta * safe_t;
}

} // namespace seed
