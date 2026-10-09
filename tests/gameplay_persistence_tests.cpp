#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    const auto test_root = std::filesystem::temp_directory_path() / "seed_gameplay_persistence_test";
    const auto scene_file = test_root / "DoorTest.seedscene";
    std::filesystem::remove_all(test_root);

    seed::Scene scene;
    const auto door_entity = scene.create_entity("Vault Door");
    const std::string persistent_id = scene.entity_persistent_id(door_entity);

    scene.add_component<seed::TransformComponent>(door_entity);
    scene.add_component<seed::InteractableComponent>(door_entity, "Unlock Vault", true);

    seed::DoorComponent door;
    door.motion = seed::DoorMotion::Slide;
    door.open_amount = 2.75f;
    door.duration_seconds = 1.4f;
    door.required_item = "VaultKey";
    door.consume_required_item = true;
    door.starts_open = false;
    scene.add_component<seed::DoorComponent>(door_entity, door);

    seed::HealthComponent health;
    health.maximum = 250.0f;
    health.current = 175.0f;
    health.invulnerable = true;
    scene.add_component<seed::HealthComponent>(door_entity, health);

    std::string error;
    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    assert(loaded.entity_count() == 1);

    seed::EntityId entity = seed::InvalidEntity;
    loaded.for_each_entity([&](seed::EntityId candidate, const std::string& name) {
        if (name == "Vault Door") {
            entity = candidate;
        }
    });

    assert(entity != seed::InvalidEntity);
    assert(loaded.entity_persistent_id(entity) == persistent_id);

    const auto* loaded_interactable = loaded.get_component<seed::InteractableComponent>(entity);
    const auto* loaded_door = loaded.get_component<seed::DoorComponent>(entity);
    const auto* loaded_health = loaded.get_component<seed::HealthComponent>(entity);

    assert(loaded_interactable != nullptr);
    assert(loaded_interactable->prompt == "Unlock Vault");
    assert(loaded_interactable->enabled);

    assert(loaded_door != nullptr);
    assert(loaded_door->motion == seed::DoorMotion::Slide);
    assert(nearly_equal(loaded_door->open_amount, 2.75f));
    assert(nearly_equal(loaded_door->duration_seconds, 1.4f));
    assert(loaded_door->required_item == "VaultKey");
    assert(loaded_door->consume_required_item);
    assert(!loaded_door->starts_open);

    assert(loaded_health != nullptr);
    assert(nearly_equal(loaded_health->maximum, 250.0f));
    assert(nearly_equal(loaded_health->current, 175.0f));
    assert(loaded_health->invulnerable);

    std::filesystem::remove_all(test_root);
    std::cout << "SeedGameplayPersistenceTests passed.\n";
    return 0;
}
