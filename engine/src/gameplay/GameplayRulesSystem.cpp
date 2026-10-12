#include "seed/gameplay/GameplayRulesSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <string>
#include <cstdlib>
#include <vector>

namespace seed {
namespace {

bool has_item(const Scene& scene, EntityId entity, const std::string& item_id, int quantity) {
    if (item_id.empty()) return true;
    const auto* inventory = scene.get_component<InventoryComponent>(entity);
    if (inventory == nullptr) return false;
    const auto it = inventory->items.find(item_id);
    return it != inventory->items.end() && it->second >= std::max(1, quantity);
}

bool number_value(const std::string& text, float& result) {
    if (text.empty()) return false;
    try {
        std::size_t consumed = 0;
        result = std::stof(text, &consumed);
        return consumed == text.size();
    } catch (...) {
        return false;
    }
}

bool condition_matches(const Scene& scene, EntityId owner, const GameplayRule& rule) {
    if (rule.condition.empty() || rule.condition == "Always") return true;
    if (rule.condition == "Has Item") return has_item(scene, owner, rule.condition_value, rule.quantity);
    if (rule.condition == "Health Below") {
        float threshold = 0.0f;
        const auto* health = scene.get_component<HealthComponent>(owner);
        return health != nullptr && number_value(rule.condition_value, threshold) && health->current < threshold;
    }
    if (rule.condition == "Quest Completed") {
        bool completed = false;
        scene.for_each<QuestComponent>([&](EntityId, const QuestComponent& quest) {
            if (quest.quest_id == rule.condition_value && quest.status == QuestStatus::Completed) completed = true;
        });
        return completed;
    }
    if (rule.condition == "Variable True" || rule.condition == "Variable At Least") {
        const auto* variables = scene.get_component<GameplayVariablesComponent>(owner);
        if (variables == nullptr) return false;
        const auto separator = rule.condition_value.find('=');
        const std::string name = separator == std::string::npos
            ? rule.condition_value
            : rule.condition_value.substr(0, separator);
        const std::string expected = separator == std::string::npos
            ? std::string{}
            : rule.condition_value.substr(separator + 1);
        for (const auto& variable : variables->values) {
            if (variable.name != name) continue;
            if (rule.condition == "Variable True") return variable.boolean;
            float threshold = 0.0f;
            return number_value(expected, threshold) && variable.number >= threshold;
        }
        return false;
    }
    if (rule.condition == "Enemy Defeated") {
        bool found_enemy = false;
        bool all_defeated = true;
        scene.for_each<HealthComponent, EnemyAIComponent>([&](EntityId, const HealthComponent& health, const EnemyAIComponent& enemy) {
            if (!enemy.enabled) return;
            found_enemy = true;
            all_defeated = all_defeated && health.current <= 0.0f;
        });
        return found_enemy && all_defeated;
    }
    return false;
}

EntityId action_entity(const GameplayEvent& event) {
    return event.target_entity != InvalidEntity
        ? event.target_entity
        : event.source_entity;
}

void publish_rule_event(
    GameplayEventBus& events,
    EntityId source,
    const GameplayEvent& trigger,
    const std::string& name
) {
    if (name.empty()) return;
    events.publish({
        .source_entity = source,
        .type = GameplayEventType::Interaction,
        .name = name,
        .time_seconds = trigger.time_seconds,
        .target_entity = trigger.target_entity,
        .value = trigger.value,
    });
}

} // namespace

void GameplayRulesSystem::consume_events(Scene& scene, GameplayEventBus& events) {
    const std::vector<GameplayEvent> triggers = events.pending();

    scene.for_each<GameplayRulesComponent>(
        [&](EntityId owner, GameplayRulesComponent& rules) {
            if (!scene.is_alive(owner)) return;

            for (const GameplayEvent& trigger : triggers) {
                if (trigger.source_entity != owner) continue;

                for (GameplayRule& rule : rules.rules) {
                    if (rule.fired || rule.when_event != trigger.name) continue;
                    if (!has_item(scene, owner, rule.required_item, rule.quantity)) continue;
                    if (!condition_matches(scene, owner, rule)) continue;

                    const EntityId target = action_entity(trigger);
                    if (rule.action == "Publish Event") {
                        publish_rule_event(events, owner, trigger, rule.action_value);
                    } else if (
                        rule.action == "Start Quest" ||
                        rule.action == "Complete Quest"
                    ) {
                        const QuestStatus desired = rule.action == "Start Quest"
                            ? QuestStatus::Active
                            : QuestStatus::Completed;
                        scene.for_each<QuestComponent>(
                            [&](EntityId, QuestComponent& quest) {
                                if (quest.quest_id == rule.action_value) {
                                    quest.status = desired;
                                    publish_rule_event(
                                        events,
                                        owner,
                                        trigger,
                                        desired == QuestStatus::Active
                                            ? "QuestStarted"
                                            : "QuestCompleted"
                                    );
                                }
                            }
                        );
                    } else if (rule.action == "Give Item") {
                        const EntityId recipient = scene.is_alive(target) ? target : owner;
                        auto* inventory = scene.get_component<InventoryComponent>(recipient);
                        if (inventory != nullptr && !rule.action_value.empty()) {
                            inventory->items[rule.action_value] += std::max(1, rule.quantity);
                            publish_rule_event(events, owner, trigger, "ItemGranted");
                        }
                    } else if (rule.action == "Open Door") {
                        const auto* door = scene.get_component<DoorComponent>(target);
                        if (door != nullptr) {
                            auto* editable_door = scene.get_component<DoorComponent>(target);
                            editable_door->starts_open = true;
                            publish_rule_event(events, owner, trigger, "DoorOpened");
                        }
                    } else if (rule.action == "Show Message") {
                        publish_rule_event(events, owner, trigger, rule.action_value.empty() ? "UIMessage" : rule.action_value);
                    } else if (rule.action == "Spawn Object") {
                        publish_rule_event(events, owner, trigger, "SpawnRequested");
                    } else if (rule.action == "Complete Game") {
                        publish_rule_event(events, owner, trigger, "Victory");
                    } else if (rule.action == "Move Object") {
                        publish_rule_event(events, owner, trigger, "MoveRequested");
                    } else if (rule.action == "Apply Damage") {
                        if (scene.is_alive(target)) {
                            events.publish({
                                .source_entity = owner,
                                .type = GameplayEventType::Damage,
                                .name = "Damage",
                                .time_seconds = trigger.time_seconds,
                                .target_entity = target,
                                .value = static_cast<float>(std::max(1, rule.quantity)),
                            });
                        }
                    }

                    if (rule.once) rule.fired = true;
                }
            }
        }
    );
}

} // namespace seed
