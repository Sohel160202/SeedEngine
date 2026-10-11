#include "seed/gameplay/Components.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/save/SaveGameService.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "seed_save_game_service_tests";
    std::filesystem::remove_all(root);

    seed::SaveGameService saves(root);
    assert(saves.save_root() == root);
    assert(seed::SaveGameService::sanitize_slot_name("") == "slot_1");
    assert(seed::SaveGameService::sanitize_slot_name("../../autosave") == "autosave");
    assert(seed::SaveGameService::sanitize_slot_name("checkpoint: forest / boss") == "checkpoint_forest_boss");

    const std::filesystem::path protected_path = saves.slot_file("../../autosave");
    assert(protected_path.parent_path() == root);
    assert(protected_path.filename() == "autosave.seedsave");

    seed::Scene scene;
    const seed::EntityId player = scene.create_entity("Player", "player-slot-id");
    seed::TransformComponent transform;
    transform.position = {2.0f, 3.0f, 4.0f};
    scene.add_component<seed::TransformComponent>(player, transform);

    seed::HealthComponent health;
    health.maximum = 100.0f;
    health.current = 77.0f;
    scene.add_component<seed::HealthComponent>(player, health);

    seed::InventoryComponent inventory;
    inventory.items["ForestKey"] = 1;
    scene.add_component<seed::InventoryComponent>(player, inventory);
    scene.add_component<seed::PersistentComponent>(player);

    const seed::SaveSlotWriteResult autosave = saves.save_slot(scene, "../../autosave", 3);
    assert(autosave.saved);
    assert(autosave.error.empty());
    assert(autosave.file == root / "autosave.seedsave");
    assert(saves.slot_exists("autosave"));

    // A second named slot proves listing/sorting and checkpoint semantics use
    // the same engine path instead of a parallel checkpoint serializer.
    const seed::SaveSlotWriteResult checkpoint =
        saves.save_slot(scene, "checkpoint: forest / boss", 3);
    assert(checkpoint.saved);
    assert(saves.slot_exists("checkpoint_forest_boss"));

    const auto slots = saves.list_slots();
    assert(slots.size() == 2);
    assert(slots[0] == "autosave");
    assert(slots[1] == "checkpoint_forest_boss");

    // Destroy runtime progress, then load through the high-level slot API.
    auto* runtime_transform = scene.get_component<seed::TransformComponent>(player);
    auto* runtime_health = scene.get_component<seed::HealthComponent>(player);
    auto* runtime_inventory = scene.get_component<seed::InventoryComponent>(player);
    assert(runtime_transform && runtime_health && runtime_inventory);
    runtime_transform->position = {50.0f, 50.0f, 50.0f};
    runtime_health->current = 1.0f;
    runtime_inventory->items.clear();

    const seed::SaveSlotLoadResult loaded = saves.load_slot(scene, "autosave");
    assert(loaded.loaded);
    assert(loaded.error.empty());
    assert(loaded.apply_result.entities_applied == 1);
    assert(runtime_transform->position.x == 2.0f);
    assert(runtime_transform->position.y == 3.0f);
    assert(runtime_transform->position.z == 4.0f);
    assert(runtime_health->current == 77.0f);
    assert(runtime_inventory->items.at("ForestKey") == 1);

    std::string delete_error;
    assert(saves.delete_slot("checkpoint_forest_boss", &delete_error));
    assert(delete_error.empty());
    assert(!saves.slot_exists("checkpoint_forest_boss"));
    assert(saves.list_slots().size() == 1);

    // Missing slots report a normal error rather than creating or guessing.
    const seed::SaveSlotLoadResult missing = saves.load_slot(scene, "missing_slot");
    assert(!missing.loaded);
    assert(!missing.error.empty());

    std::filesystem::remove_all(root);
    std::cout << "Seed Save Game slot service tests passed.\n";
    return 0;
}
