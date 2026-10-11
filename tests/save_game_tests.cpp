#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/save/SaveGame.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
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
    scene.add_component<seed::PersistentComponent>(player);

    const seed::EntityId transient = scene.create_entity("Transient", "transient-id");
    scene.add_component<seed::TransformComponent>(transient);
    scene.add_component<seed::HealthComponent>(transient);

    const seed::SaveCaptureResult captured = seed::SaveGameSystem::capture(scene, "slot_alpha", 7);
    assert(captured.snapshot.format_version == seed::SeedSaveFormatVersion);
    assert(captured.snapshot.slot == "slot_alpha");
    assert(captured.snapshot.game_schema_version == 7);
    assert(captured.snapshot.entities.size() == 1);
    assert(captured.snapshot.entities.front().persistent_id == "player-save-id");
    assert(captured.snapshot.entities.front().transform.has_value());
    assert(captured.snapshot.entities.front().health.has_value());
    assert(captured.snapshot.entities.front().inventory.has_value());

    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "seed_save_game_tests";
    std::filesystem::create_directories(root);
    const std::filesystem::path save_file = root / "slot_alpha.seedsave";
    const std::filesystem::path scene_file = root / "persistent_marker.seedscene";

    std::string error;
    assert(seed::SaveGameSystem::write_file(captured.snapshot, save_file, &error));
    assert(error.empty());

    const auto loaded = seed::SaveGameSystem::read_file(save_file, &error);
    assert(loaded.has_value());
    assert(loaded->slot == "slot_alpha");
    assert(loaded->game_schema_version == 7);
    assert(loaded->entities.size() == 1);

    // Change authored balance and runtime state after the save. Restore should
    // apply progress semantically and clamp saved Health to the CURRENT authored
    // maximum rather than reviving obsolete balance data from the save file.
    auto* runtime_transform = scene.get_component<seed::TransformComponent>(player);
    auto* runtime_health = scene.get_component<seed::HealthComponent>(player);
    auto* runtime_inventory = scene.get_component<seed::InventoryComponent>(player);
    assert(runtime_transform && runtime_health && runtime_inventory);

    runtime_transform->position = {100.0f, 100.0f, 100.0f};
    runtime_health->maximum = 30.0f;
    runtime_health->current = 1.0f;
    runtime_inventory->items.clear();

    seed::SaveGameSnapshot with_stale_entity = *loaded;
    seed::SaveEntityState stale;
    stale.persistent_id = "entity-that-no-longer-exists";
    stale.health = seed::SaveHealthState{.current = 5.0f};
    with_stale_entity.entities.push_back(stale);

    const seed::SaveApplyResult applied = seed::SaveGameSystem::apply(scene, with_stale_entity);
    assert(applied.entities_applied == 1);
    assert(applied.entities_skipped == 1);
    assert(applied.properties_applied == 3);
    assert(!applied.warnings.empty());

    assert(near(runtime_transform->position.x, 4.0f));
    assert(near(runtime_transform->position.y, 1.5f));
    assert(near(runtime_transform->position.z, -7.0f));
    assert(near(runtime_health->maximum, 30.0f));
    assert(near(runtime_health->current, 30.0f));
    assert(runtime_inventory->items.at("CabinKey") == 1);
    assert(runtime_inventory->items.at("Coin") == 12);

    // Persistent is an authored declaration, so it must survive .seedscene
    // save/load independently from runtime .seedsave progress.
    assert(seed::SceneSerializer::save(scene, scene_file, &error));
    seed::Scene reloaded_scene;
    assert(seed::SceneSerializer::load(reloaded_scene, scene_file, &error));
    const seed::EntityId reloaded_player =
        reloaded_scene.find_entity_by_persistent_id("player-save-id");
    assert(reloaded_player != seed::InvalidEntity);
    const auto* persistent =
        reloaded_scene.get_component<seed::PersistentComponent>(reloaded_player);
    assert(persistent != nullptr);
    assert(persistent->enabled);

    // Removing the authored Persistent declaration prevents old save data from
    // mutating this entity. Seed reports the mismatch instead of guessing.
    assert(reloaded_scene.remove_component<seed::PersistentComponent>(reloaded_player));
    const seed::SaveApplyResult no_longer_persistent =
        seed::SaveGameSystem::apply(reloaded_scene, *loaded);
    assert(no_longer_persistent.entities_applied == 0);
    assert(no_longer_persistent.entities_skipped == 1);
    assert(!no_longer_persistent.warnings.empty());

    std::filesystem::remove_all(root);
    std::cout << "Seed Save Game semantic round-trip tests passed.\n";
    return 0;
}
