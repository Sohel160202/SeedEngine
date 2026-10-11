#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/physics/PhysicsSystem.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    seed::Scene scene;

    const auto player = scene.create_entity("Player");
    scene.add_component<seed::TransformComponent>(player);

    const auto wall = scene.create_entity("Camera Wall");
    auto& wall_transform = scene.add_component<seed::TransformComponent>(wall);
    wall_transform.position = {0.0f, 1.5f, 2.5f};

    seed::BoxColliderComponent collider;
    collider.half_extents = {2.0f, 2.0f, 0.25f};
    collider.enabled = true;
    collider.solid = true;
    scene.add_component<seed::BoxColliderComponent>(wall, collider);

    const seed::Vec3 anchor{0.0f, 1.5f, 0.0f};
    const seed::Vec3 desired{0.0f, 1.5f, 5.0f};
    const seed::Vec3 clipped = seed::PhysicsSystem::clip_camera_position(
        scene, player, anchor, desired, 0.15f, 0.05f
    );

    // Wall front is z=2.25. With a 0.15m probe and 0.05m padding, the camera
    // should remain roughly at or before z=2.05 rather than reaching z=5.
    assert(clipped.z > 1.8f);
    assert(clipped.z <= 2.051f);
    assert(std::fabs(clipped.x) < 0.001f);

    // Disabled collision must not affect the camera query.
    auto* wall_collider = scene.get_component<seed::BoxColliderComponent>(wall);
    assert(wall_collider != nullptr);
    wall_collider->enabled = false;
    const seed::Vec3 unobstructed = seed::PhysicsSystem::clip_camera_position(
        scene, player, anchor, desired
    );
    assert(std::fabs(unobstructed.z - desired.z) < 0.001f);

    // The caller can ignore its own entity if it happens to carry collision.
    wall_collider->enabled = true;
    const seed::Vec3 ignored = seed::PhysicsSystem::clip_camera_position(
        scene, wall, anchor, desired
    );
    assert(std::fabs(ignored.z - desired.z) < 0.001f);

    std::cout << "Seed camera collision tests passed.\n";
    return 0;
}
