#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <iostream>

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

        assert(scene.remove_component<seed::InteractableComponent>(door));
        assert(!scene.has_component<seed::InteractableComponent>(door));

        assert(scene.destroy_entity(door));
        assert(!scene.is_alive(door));
        assert(scene.entity_count() == 0);
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
