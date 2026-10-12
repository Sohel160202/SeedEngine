#include "seed/gameplay/CombatSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

namespace seed {

void CombatSystem::consume_events(
    Scene& scene,
    const GameplayEventBus& event_bus
) {
    for (const auto& event : event_bus.pending()) {
        if (event.type != GameplayEventType::Animation ||
            event.source_entity == InvalidEntity ||
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
    }
}

} // namespace seed
