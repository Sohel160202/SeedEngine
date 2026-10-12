#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRulesSystem.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <iostream>

int main() {
    seed::Scene scene;
    const auto player = scene.create_entity("Player", "player");
    scene.add_component<seed::InventoryComponent>(player);

    seed::GameplayRulesComponent rules;
    rules.rules.push_back({
        .when_event = "Talked",
        .action = "Start Quest",
        .action_value = "find_key",
        .once = true,
    });
    rules.rules.push_back({
        .when_event = "Talked",
        .action = "Give Item",
        .action_value = "Coin",
        .quantity = 5,
        .once = true,
    });
    scene.add_component<seed::GameplayRulesComponent>(player, rules);

    const auto quest = scene.create_entity("Find Key Quest", "quest");
    seed::QuestComponent quest_component;
    quest_component.quest_id = "find_key";
    scene.add_component<seed::QuestComponent>(quest, quest_component);

    seed::GameplayVariablesComponent variables;
    variables.values.push_back({"Score", 10.0f, false});
    scene.add_component<seed::GameplayVariablesComponent>(player, variables);
    seed::GameplayRulesComponent conditional_rules;
    conditional_rules.rules.push_back({
        .when_event = "Talked",
        .condition = "Variable At Least",
        .condition_value = "Score=10",
        .action = "Complete Game",
        .once = true,
    });
    scene.add_component<seed::GameplayRulesComponent>(player, conditional_rules);

    seed::GameplayEventBus events;
    events.publish({
        .source_entity = player,
        .type = seed::GameplayEventType::Interaction,
        .name = "Talked",
        .time_seconds = 1.0f,
    });
    seed::GameplayRulesSystem::consume_events(scene, events);

    assert(scene.get_component<seed::QuestComponent>(quest)->status == seed::QuestStatus::Active);
    assert(scene.get_component<seed::InventoryComponent>(player)->items.at("Coin") == 5);
    assert(scene.get_component<seed::GameplayRulesComponent>(player)->rules[0].fired);
    assert(scene.get_component<seed::GameplayRulesComponent>(player)->rules[1].fired);
    assert(scene.get_component<seed::GameplayRulesComponent>(player)->rules[2].fired);
    assert(events.pending().back().name == "Victory");

    const auto pending_after_first = events.pending().size();
    seed::GameplayRulesSystem::consume_events(scene, events);
    assert(events.pending().size() == pending_after_first);

    std::cout << "SeedGameplayRulesTests passed.\n";
    return 0;
}
