#include "seed/gameplay/CombatSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <vector>

namespace seed {

void CombatSystem::consume_events(
    Scene& scene,
    const GameplayEventBus& event_bus
) {
    // Work from a snapshot because damage application publishes hit/death
    // events onto the same bus for downstream consumers.
    const std::vector<GameplayEvent> events = event_bus.pending();
    for (const auto& event : events) {
        if (event.type == GameplayEventType::Animation) {
            if (event.source_entity == InvalidEntity ||
                !scene.is_alive(event.source_entity)) {
                continue;
            }

            auto* attack = scene.get_component<AttackComponent>(event.source_entity);
            if (attack == nullptr) {
                continue;
            }

            if (event.name == attack->open_event) {
                attack->active = true;
            } else if (event.name == attack->close_event) {
                attack->active = false;
            }
            continue;
        }

        if (event.type == GameplayEventType::Damage) {
            apply_damage(
                scene,
                event.source_entity,
                event.target_entity,
                event.value,
                const_cast<GameplayEventBus&>(event_bus),
                event.time_seconds
            );
        }
    }
}

bool CombatSystem::apply_attack_damage(
    Scene& scene,
    EntityId attacker,
    EntityId target,
    GameplayEventBus& event_bus,
    float time_seconds
) {
    if (attacker == InvalidEntity || !scene.is_alive(attacker)) {
        return false;
    }

    const auto* attack = scene.get_component<AttackComponent>(attacker);
    if (attack == nullptr || !attack->active) {
        return false;
    }

    return apply_damage(
        scene,
        attacker,
        target,
        attack->damage,
        event_bus,
        time_seconds
    );
}

bool CombatSystem::apply_damage(
    Scene& scene,
    EntityId source,
    EntityId target,
    float amount,
    GameplayEventBus& event_bus,
    float time_seconds
) {
    if (target == InvalidEntity || !scene.is_alive(target) || amount <= 0.0f) {
        return false;
    }

    auto* health = scene.get_component<HealthComponent>(target);
    if (health == nullptr || health->invulnerable || health->current <= 0.0f) {
        return false;
    }

    health->maximum = std::max(0.0f, health->maximum);
    health->current = std::clamp(health->current - amount, 0.0f, health->maximum);

    event_bus.publish({
        .source_entity = source,
        .type = GameplayEventType::Combat,
        .name = "Hit",
        .time_seconds = time_seconds,
        .target_entity = target,
        .value = amount,
    });

    if (health->current <= 0.0f) {
        event_bus.publish({
            .source_entity = target,
            .type = GameplayEventType::Combat,
            .name = "Death",
            .time_seconds = time_seconds,
            .target_entity = target,
            .value = 0.0f,
        });
    }

    return true;
}

} // namespace seed
