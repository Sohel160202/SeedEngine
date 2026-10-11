#include "seed/gameplay/Components.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/save/SaveGameService.h"
#include "seed/scene/Scene.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "SeedSaveGameServiceTests FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "seed_save_game_service_tests";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);

    seed::SaveGameService saves(root);
    require(saves.save_root() == root, "configured save root");
    require(seed::SaveGameService::sanitize_slot_name("") == "slot_1", "empty slot fallback");
    require(seed::SaveGameService::sanitize_slot_name("../../autosave") == "autosave", "path traversal slot sanitization");
    require(
        seed::SaveGameService::sanitize_slot_name("checkpoint: forest / boss") == "checkpoint_forest_boss",
        "human checkpoint slot normalization"
    );

    const std::filesystem::path protected_path = saves.slot_file("../../autosave");
    require(protected_path.parent_path() == root, "sanitized slot stays inside save root");
    require(protected_path.filename() == "autosave.seedsave", "sanitized slot filename");

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
    require(autosave.saved, "write autosave slot");
    require(autosave.error.empty(), "autosave write has no error");
    require(autosave.file == root / "autosave.seedsave", "autosave file path");
    require(saves.slot_exists("autosave"), "autosave slot exists");

    // A second named slot proves listing/sorting and checkpoint semantics use
    // the same engine path instead of a parallel checkpoint serializer.
    const seed::SaveSlotWriteResult checkpoint =
        saves.save_slot(scene, "checkpoint: forest / boss", 3);
    require(checkpoint.saved, "write checkpoint slot");
    require(saves.slot_exists("checkpoint_forest_boss"), "checkpoint slot exists");

    const auto slots = saves.list_slots();
    require(slots.size() == 2, "list two save slots");
    require(slots[0] == "autosave", "slot list sorted: autosave first");
    require(slots[1] == "checkpoint_forest_boss", "slot list sorted: checkpoint second");

    // Destroy runtime progress, then load through the high-level slot API.
    auto* runtime_transform = scene.get_component<seed::TransformComponent>(player);
    auto* runtime_health = scene.get_component<seed::HealthComponent>(player);
    auto* runtime_inventory = scene.get_component<seed::InventoryComponent>(player);
    require(runtime_transform != nullptr, "runtime Transform exists");
    require(runtime_health != nullptr, "runtime Health exists");
    require(runtime_inventory != nullptr, "runtime Inventory exists");
    runtime_transform->position = {50.0f, 50.0f, 50.0f};
    runtime_health->current = 1.0f;
    runtime_inventory->items.clear();

    const seed::SaveSlotLoadResult loaded = saves.load_slot(scene, "autosave");
    require(loaded.loaded, "load autosave slot");
    require(loaded.error.empty(), "autosave load has no error");
    require(loaded.apply_result.entities_applied == 1, "apply one Persistent entity from slot");
    require(runtime_transform->position.x == 2.0f, "restore slot Transform X");
    require(runtime_transform->position.y == 3.0f, "restore slot Transform Y");
    require(runtime_transform->position.z == 4.0f, "restore slot Transform Z");
    require(runtime_health->current == 77.0f, "restore slot Health");
    require(runtime_inventory->items.contains("ForestKey"), "restore slot inventory key");
    require(runtime_inventory->items.at("ForestKey") == 1, "restore slot inventory quantity");

    std::string delete_error;
    require(saves.delete_slot("checkpoint_forest_boss", &delete_error), "delete checkpoint slot");
    require(delete_error.empty(), "checkpoint delete has no error");
    require(!saves.slot_exists("checkpoint_forest_boss"), "deleted checkpoint no longer exists");
    require(saves.list_slots().size() == 1, "slot listing updates after delete");

    // Missing slots report a normal error rather than creating or guessing.
    const seed::SaveSlotLoadResult missing = saves.load_slot(scene, "missing_slot");
    require(!missing.loaded, "missing slot does not load");
    require(!missing.error.empty(), "missing slot reports error");

    std::filesystem::remove_all(root, cleanup_error);
    std::cout << "Seed Save Game slot service tests passed.\n";
    return 0;
}
