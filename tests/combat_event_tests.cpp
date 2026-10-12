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

    const auto target = scene.create_entity("Target", "target");
    seed::HealthComponent health;
    health.maximum = 60.0f;
    health.current = 60.0f;
    scene.add_component<seed::HealthComponent>(target, health);

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

    assert(seed::CombatSystem::apply_attack_damage(
        scene,
        attacker,
        target,
        events,
        0.3f
    ));
    assert(scene.get_component<seed::HealthComponent>(target)->current == 18.0f);
    assert(events.pending().size() == 2);
    assert(events.pending().back().name == "Hit");
    assert(events.pending().back().target_entity == target);

    assert(seed::CombatSystem::apply_attack_damage(
        scene,
        attacker,
        target,
        events,
        0.4f
    ));
    assert(scene.get_component<seed::HealthComponent>(target)->current == 0.0f);
    assert(events.pending().back().name == "Death");

    assert(!seed::CombatSystem::apply_attack_damage(
        scene,
        attacker,
        target,
        events,
        0.5f
    ));

    events.clear();
    events.publish({
        .source_entity = attacker,
        .type = seed::GameplayEventType::Animation,
        .name = "SwordEnd",
        .time_seconds = 0.7f,
    });
    seed::CombatSystem::consume_events(scene, events);
    assert(!scene.get_component<seed::AttackComponent>(attacker)->active);

    seed::Scene invulnerable_scene;
    const auto invulnerable_target = invulnerable_scene.create_entity("Target", "invulnerable");
    seed::HealthComponent invulnerable_health;
    invulnerable_health.invulnerable = true;
    invulnerable_scene.add_component<seed::HealthComponent>(
        invulnerable_target,
        invulnerable_health
    );
    seed::GameplayEventBus invulnerable_events;
    invulnerable_events.publish({
        .source_entity = attacker,
        .type = seed::GameplayEventType::Damage,
        .name = "Damage",
        .time_seconds = 1.0f,
        .target_entity = invulnerable_target,
        .value = 10.0f,
    });
    seed::CombatSystem::consume_events(
        invulnerable_scene,
        invulnerable_events
    );
    assert(invulnerable_scene.get_component<seed::HealthComponent>(
        invulnerable_target
    )->current == 100.0f);
    assert(invulnerable_events.pending().size() == 1);

    std::string error;
    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    const auto loaded_attacker = loaded.find_entity_by_persistent_id("attacker");
    assert(loaded_attacker != seed::InvalidEntity);
    const auto loaded_target = loaded.find_entity_by_persistent_id("target");
    assert(loaded_target != seed::InvalidEntity);

    const auto* loaded_attack = loaded.get_component<seed::AttackComponent>(loaded_attacker);
    assert(loaded_attack != nullptr);
    assert(loaded_attack->open_event == "SwordStart");
    assert(loaded_attack->close_event == "SwordEnd");
    assert(loaded_attack->damage == 42.0f);
    assert(!loaded_attack->active);
    assert(loaded.get_component<seed::HealthComponent>(loaded_target)->current == 0.0f);

    std::filesystem::remove_all(root);
    std::cout << "SeedCombatEventTests passed.\n";
    return 0;
}
