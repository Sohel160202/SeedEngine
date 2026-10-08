#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <iostream>

int main() {
    seed::Engine engine({.application_name = "Seed Runtime"});
    engine.start();

    auto& scene = engine.scene();
    const auto player = scene.create_entity("Player");
    scene.add_component<seed::TransformComponent>(player);
    scene.add_component<seed::InventoryComponent>(player);

    const auto door = scene.create_entity("Cabin Door");
    scene.add_component<seed::TransformComponent>(door);
    scene.add_component<seed::InteractableComponent>(door, "Open Cabin Door", true);
    scene.add_component<seed::DoorComponent>(
        door,
        seed::DoorMotion::Rotate,
        90.0f,
        0.75f,
        "CabinKey",
        false,
        false
    );

    std::cout << "[SeedRuntime] Scene booted with " << scene.entity_count() << " entities.\n";
    std::cout << "[SeedRuntime] Door is a Seed entity with Seed-owned components.\n";

    engine.shutdown();
    return 0;
}
