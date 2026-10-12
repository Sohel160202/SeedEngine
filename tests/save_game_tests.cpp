#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/save/SaveGame.h"
#include "seed/scene/Scene.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "SeedSaveGameTests FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main() {
    seed::Scene scene;

    const seed::EntityId player = scene.create_entity("Player", "player-save-id");
    seed::TransformComponent transform;
    transform.position = {4.0f, 1.5f, -7.0f};
    transform.rotation_degrees = {0.0f, 35.0f, 0.0f};
    scene.add_component<seed::TransformComponent>(player, transform);

    seed::HealthComponent health;
    health.maximum = 100.0f;
    health.current = 42.0f;
    scene.add_component<seed::HealthComponent>(player, health);

    seed::InventoryComponent inventory;
    inventory.items["CabinKey"] = 1;
    inventory.items["Coin"] = 12;
    scene.add_component<seed::InventoryComponent>(player, inventory);

    seed::QuestComponent quest;
    quest.quest_id = "cabin_quest";
    quest.title = "Open the Cabin";
    quest.status = seed::QuestStatus::Active;
    scene.add_component<seed::QuestComponent>(player, quest);
    scene.add_component<seed::PersistentComponent>(player);

    const seed::EntityId transient = scene.create_entity("Transient", "transient-id");
    scene.add_component<seed::TransformComponent>(transient);
    scene.add_component<seed::HealthComponent>(transient);

    const seed::SaveCaptureResult captured = seed::SaveGameSystem::capture(scene, "slot_alpha", 7);
    require(captured.snapshot.format_version == seed::SeedSaveFormatVersion, "capture format version");
    require(captured.snapshot.slot == "slot_alpha", "capture slot");
    require(captured.snapshot.game_schema_version == 7, "capture game schema version");
    require(captured.snapshot.entities.size() == 1, "capture only Persistent entities");
    require(captured.snapshot.entities.front().persistent_id == "player-save-id", "capture stable entity ID");
    require(captured.snapshot.entities.front().transform.has_value(), "capture Transform");
    require(captured.snapshot.entities.front().health.has_value(), "capture Health");
    require(captured.snapshot.entities.front().inventory.has_value(), "capture Inventory");
    require(captured.snapshot.entities.front().quest.has_value(), "capture Quest");
    require(captured.snapshot.entities.front().quest->quest_id == "cabin_quest", "capture Quest ID");

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "seed_save_game_tests";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);
    std::filesystem::create_directories(root);
    const std::filesystem::path save_file = root / "slot_alpha.seedsave";
    const std::filesystem::path scene_file = root / "persistent_marker.seedscene";

    std::string error;
    require(seed::SaveGameSystem::write_file(captured.snapshot, save_file, &error), "write .seedsave file");
    require(error.empty(), "write .seedsave should not report error");

    const auto loaded = seed::SaveGameSystem::read_file(save_file, &error);
    require(loaded.has_value(), "read .seedsave file");
    require(error.empty(), "read .seedsave should not report error");
    require(loaded->slot == "slot_alpha", "round-trip slot");
    require(loaded->game_schema_version == 7, "round-trip game schema version");
    require(loaded->entities.size() == 1, "round-trip entity count");

    // Change authored balance and runtime state after the save. Restore should
    // apply progress semantically and clamp saved Health to the CURRENT authored
    // maximum rather than reviving obsolete balance data from the save file.
    auto* runtime_transform = scene.get_component<seed::TransformComponent>(player);
    auto* runtime_health = scene.get_component<seed::HealthComponent>(player);
    auto* runtime_inventory = scene.get_component<seed::InventoryComponent>(player);
    require(runtime_transform != nullptr, "runtime Transform exists");
    require(runtime_health != nullptr, "runtime Health exists");
    require(runtime_inventory != nullptr, "runtime Inventory exists");

    runtime_transform->position = {100.0f, 100.0f, 100.0f};
    runtime_health->maximum = 30.0f;
    runtime_health->current = 1.0f;
    runtime_inventory->items.clear();
    auto* runtime_quest = scene.get_component<seed::QuestComponent>(player);
    require(runtime_quest != nullptr, "runtime Quest exists");
    runtime_quest->status = seed::QuestStatus::Completed;

    seed::SaveGameSnapshot with_stale_entity = *loaded;
    seed::SaveEntityState stale;
    stale.persistent_id = "entity-that-no-longer-exists";
    stale.health = seed::SaveHealthState{.current = 5.0f};
    with_stale_entity.entities.push_back(stale);

    const seed::SaveApplyResult applied = seed::SaveGameSystem::apply(scene, with_stale_entity);
    require(applied.entities_applied == 1, "restore one matching Persistent entity");
    require(applied.entities_skipped == 1, "skip one stale entity");
    require(applied.properties_applied == 4, "restore Transform + Health + Inventory + Quest");
    require(!applied.warnings.empty(), "stale entity produces warning");

    require(near(runtime_transform->position.x, 4.0f), "restore Transform X");
    require(near(runtime_transform->position.y, 1.5f), "restore Transform Y");
    require(near(runtime_transform->position.z, -7.0f), "restore Transform Z");
    require(near(runtime_health->maximum, 30.0f), "restore does not overwrite authored Health maximum");
    require(near(runtime_health->current, 30.0f), "restore clamps Health current to authored maximum");
    require(runtime_inventory->items.contains("CabinKey"), "restore CabinKey inventory entry");
    require(runtime_inventory->items.at("CabinKey") == 1, "restore CabinKey quantity");
    require(runtime_inventory->items.contains("Coin"), "restore Coin inventory entry");
    require(runtime_inventory->items.at("Coin") == 12, "restore Coin quantity");
    require(runtime_quest->status == seed::QuestStatus::Active, "restore Quest status");

    // Persistent is an authored declaration, so it must survive .seedscene
    // save/load independently from runtime .seedsave progress.
    require(seed::SceneSerializer::save(scene, scene_file, &error), "save scene with Persistent marker");
    require(error.empty(), "scene save should not report error");

    seed::Scene reloaded_scene;
    require(seed::SceneSerializer::load(reloaded_scene, scene_file, &error), "reload scene with Persistent marker");
    require(error.empty(), "scene reload should not report error");

    const seed::EntityId reloaded_player =
        reloaded_scene.find_entity_by_persistent_id("player-save-id");
    require(reloaded_player != seed::InvalidEntity, "reloaded Player stable ID");
    const auto* persistent =
        reloaded_scene.get_component<seed::PersistentComponent>(reloaded_player);
    require(persistent != nullptr, "Persistent marker survives scene round-trip");
    require(persistent->enabled, "Persistent enabled state survives scene round-trip");

    // Removing the authored Persistent declaration prevents old save data from
    // mutating this entity. Seed reports the mismatch instead of guessing.
    require(
        reloaded_scene.remove_component<seed::PersistentComponent>(reloaded_player),
        "remove Persistent marker"
    );
    const seed::SaveApplyResult no_longer_persistent =
        seed::SaveGameSystem::apply(reloaded_scene, *loaded);
    require(no_longer_persistent.entities_applied == 0, "do not restore entity no longer marked Persistent");
    require(no_longer_persistent.entities_skipped == 1, "count non-Persistent restore as skipped");
    require(!no_longer_persistent.warnings.empty(), "non-Persistent restore produces warning");

    std::filesystem::remove_all(root, cleanup_error);
    std::cout << "Seed Save Game semantic round-trip tests passed.\n";
    return 0;
}
