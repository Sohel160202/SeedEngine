#include "seed/gameplay/CombatSystem.h"
#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_combat_event_test";
    const auto scene_file = root / "Combat.seedscene";
    std::filesystem::remove_all(root);

    seed::Scene scene;
    const auto attacker = scene.create_entity("Attacker", "attacker");
    scene.add_component<seed::TransformComponent>(attacker);

    seed::AttackComponent attack;
    attack.open_event = "SwordStart";
    attack.close_event = "SwordEnd";
    attack.damage = 42.0f;
    scene.add_component<seed::AttackComponent>(attacker, attack);

    seed::GameplayEventBus events;
    events.publish({
        .source_entity = attacker,
        .type = seed::GameplayEventType::Animation,
        .name = "SwordStart",
        .time_seconds = 0.2f,
    });
    seed::CombatSystem::consume_events(scene, events);
    assert(scene.get_component<seed::AttackComponent>(attacker)->active);
    assert(events.pending().size() == 1);

    events.clear();
    events.publish({
        .source_entity = attacker,
        .type = seed::GameplayEventType::Animation,
        .name = "SwordEnd",
        .time_seconds = 0.7f,
    });
    seed::CombatSystem::consume_events(scene, events);
    assert(!scene.get_component<seed::AttackComponent>(attacker)->active);

    std::string error;
    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    const auto loaded_attacker = loaded.find_entity_by_persistent_id("attacker");
    assert(loaded_attacker != seed::InvalidEntity);

    const auto* loaded_attack = loaded.get_component<seed::AttackComponent>(loaded_attacker);
    assert(loaded_attack != nullptr);
    assert(loaded_attack->open_event == "SwordStart");
    assert(loaded_attack->close_event == "SwordEnd");
    assert(loaded_attack->damage == 42.0f);
    assert(!loaded_attack->active);

    std::filesystem::remove_all(root);
    std::cout << "SeedCombatEventTests passed.\n";
    return 0;
}
