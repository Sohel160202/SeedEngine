#include "seed/scene/Hierarchy.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <unordered_set>

namespace seed {
namespace {

Mat4 world_matrix_recursive(const Scene& scene, EntityId entity, std::unordered_set<EntityId>& visiting) noexcept {
    if (entity == InvalidEntity || !scene.is_alive(entity) || visiting.contains(entity)) return Mat4::identity();
    visiting.insert(entity);

    const Mat4 local = local_transform_matrix(scene, entity);
    const EntityId parent = parent_entity(scene, entity);
    if (parent == InvalidEntity) {
        visiting.erase(entity);
        return local;
    }

    const Mat4 world = world_matrix_recursive(scene, parent, visiting) * local;
    visiting.erase(entity);
    return world;
}

bool apply_local_matrix(Scene& scene, EntityId entity, const Mat4& local) {
    auto* transform = scene.get_component<TransformComponent>(entity);
    if (transform == nullptr) return false;
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    if (!decompose_transform_matrix(local, position, rotation, scale)) return false;
    transform->position = position;
    transform->rotation_degrees = rotation;
    transform->scale = scale;
    return true;
}

} // namespace

EntityId parent_entity(const Scene& scene, EntityId entity) noexcept {
    if (entity == InvalidEntity || !scene.is_alive(entity)) return InvalidEntity;
    const auto* parent = scene.get_component<ParentComponent>(entity);
    if (parent == nullptr || parent->parent_persistent_id.empty()) return InvalidEntity;
    const EntityId resolved = scene.find_entity_by_persistent_id(parent->parent_persistent_id);
    return resolved != entity ? resolved : InvalidEntity;
}

bool can_parent(const Scene& scene, EntityId child, EntityId parent) noexcept {
    if (child == InvalidEntity || parent == InvalidEntity || child == parent ||
        !scene.is_alive(child) || !scene.is_alive(parent)) return false;

    EntityId current = parent;
    for (std::size_t depth = 0; depth < 256 && current != InvalidEntity; ++depth) {
        if (current == child) return false;
        current = parent_entity(scene, current);
    }
    return true;
}

bool set_parent(Scene& scene, EntityId child, EntityId parent, bool keep_world_transform) {
    if (!can_parent(scene, child, parent)) return false;
    const Mat4 old_world = keep_world_transform ? world_transform_matrix(scene, child) : Mat4::identity();

    ParentComponent relation;
    relation.parent_persistent_id = scene.entity_persistent_id(parent);
    scene.add_component<ParentComponent>(child, relation);

    if (!keep_world_transform) return true;
    Mat4 inverse_parent{};
    if (!inverse_matrix(world_transform_matrix(scene, parent), inverse_parent)) return false;
    return apply_local_matrix(scene, child, inverse_parent * old_world);
}

bool clear_parent(Scene& scene, EntityId child, bool keep_world_transform) {
    if (child == InvalidEntity || !scene.is_alive(child)) return false;
    const Mat4 old_world = keep_world_transform ? world_transform_matrix(scene, child) : Mat4::identity();
    const bool had_parent = scene.remove_component<ParentComponent>(child);
    if (!had_parent) return false;
    return !keep_world_transform || apply_local_matrix(scene, child, old_world);
}

Mat4 local_transform_matrix(const Scene& scene, EntityId entity) noexcept {
    if (entity == InvalidEntity || !scene.is_alive(entity)) return Mat4::identity();
    const auto* transform = scene.get_component<TransformComponent>(entity);
    if (transform == nullptr) return Mat4::identity();
    return transform_matrix(transform->position, transform->rotation_degrees, transform->scale);
}

Mat4 world_transform_matrix(const Scene& scene, EntityId entity) noexcept {
    std::unordered_set<EntityId> visiting;
    return world_matrix_recursive(scene, entity, visiting);
}

TransformComponent world_transform(const Scene& scene, EntityId entity) noexcept {
    TransformComponent result;
    Vec3 position{};
    Vec3 rotation{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    if (decompose_transform_matrix(world_transform_matrix(scene, entity), position, rotation, scale)) {
        result.position = position;
        result.rotation_degrees = rotation;
        result.scale = scale;
    }
    return result;
}

} // namespace seed
