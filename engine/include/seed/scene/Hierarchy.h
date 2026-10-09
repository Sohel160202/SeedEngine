#pragma once

#include "seed/core/Types.h"
#include "seed/math/Math.h"

namespace seed {

class Scene;
struct TransformComponent;

EntityId parent_entity(const Scene& scene, EntityId entity) noexcept;
bool can_parent(const Scene& scene, EntityId child, EntityId parent) noexcept;
bool set_parent(Scene& scene, EntityId child, EntityId parent, bool keep_world_transform = true);
bool clear_parent(Scene& scene, EntityId child, bool keep_world_transform = true);

Mat4 local_transform_matrix(const Scene& scene, EntityId entity) noexcept;
Mat4 world_transform_matrix(const Scene& scene, EntityId entity) noexcept;
TransformComponent world_transform(const Scene& scene, EntityId entity) noexcept;

} // namespace seed
