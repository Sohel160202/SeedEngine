#include "seed/gameplay/GameplayRulesSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <string>
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
