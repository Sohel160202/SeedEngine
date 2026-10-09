#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/math/Math.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    {
        seed::Scene scene;

        const auto door = scene.create_entity("Door");
        assert(door != seed::InvalidEntity);
        assert(scene.is_alive(door));
        assert(scene.entity_name(door) == "Door");

        auto& transform = scene.add_component<seed::TransformComponent>(door);
        transform.position = {1.0f, 2.0f, 3.0f};

        scene.add_component<seed::InteractableComponent>(door, "Open Door", true);
        scene.add_component<seed::DoorComponent>(
            door,
            seed::DoorMotion::Rotate,
            90.0f,
            0.5f,
            "GoldenKey",
            false,
            false
        );

        assert(scene.has_component<seed::TransformComponent>(door));
        assert(scene.has_component<seed::DoorComponent>(door));
        assert(scene.get_component<seed::TransformComponent>(door)->position.x == 1.0f);
        assert(scene.get_component<seed::DoorComponent>(door)->required_item == "GoldenKey");

        int transform_count = 0;
        scene.for_each<seed::TransformComponent>(
            [&](seed::EntityId entity, seed::TransformComponent& component) {
                assert(entity == door);
                assert(component.position.y == 2.0f);
                ++transform_count;
            }
        );
        assert(transform_count == 1);

        assert(scene.remove_component<seed::InteractableComponent>(door));
        assert(!scene.has_component<seed::InteractableComponent>(door));

        assert(scene.destroy_entity(door));
        assert(!scene.is_alive(door));
        assert(scene.entity_count() == 0);
    }

    {
        const seed::Vec3 default_forward = seed::forward_from_euler({0.0f, 0.0f, 0.0f});
        assert(nearly_equal(default_forward.x, 0.0f));
        assert(nearly_equal(default_forward.y, 0.0f));
        assert(nearly_equal(default_forward.z, -1.0f));

        const seed::Vec3 right = seed::right_from_euler({0.0f, 0.0f, 0.0f});
        assert(nearly_equal(right.x, 1.0f));
        assert(nearly_equal(right.y, 0.0f));
        assert(nearly_equal(right.z, 0.0f));

        const seed::Mat4 translation = seed::translation_matrix({2.0f, 3.0f, 4.0f});
        assert(nearly_equal(translation.values[12], 2.0f));
        assert(nearly_equal(translation.values[13], 3.0f));
        assert(nearly_equal(translation.values[14], 4.0f));
    }

    {
        seed::Scene scene;
        const auto camera = scene.create_entity("Camera");
        scene.add_component<seed::TransformComponent>(camera);
        scene.add_component<seed::CameraComponent>(camera);

        int camera_count = 0;
        scene.for_each<seed::TransformComponent, seed::CameraComponent>(
            [&](seed::EntityId entity, seed::TransformComponent&, seed::CameraComponent& component) {
                assert(entity == camera);
                assert(component.primary);
                ++camera_count;
            }
        );
        assert(camera_count == 1);
    }

    {
        seed::Engine engine({
            .application_name = "Seed Headless Test",
            .create_window = false,
        });

        assert(engine.start());
        assert(engine.is_running());
        assert(engine.tick());
        assert(engine.frame_index() == 1);
        assert(engine.delta_seconds() >= 0.0);

        engine.request_exit();
        assert(!engine.tick());
        engine.shutdown();
        assert(!engine.is_running());
    }

    std::cout << "SeedCoreTests passed.\n";
    return 0;
}
