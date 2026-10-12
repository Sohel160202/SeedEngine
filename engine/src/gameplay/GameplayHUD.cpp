#include "seed/gameplay/GameplayHUD.h"

#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <string>

namespace seed {

GameplayHUDState GameplayHUDSystem::capture(
    const Scene& scene,
    const GameplayRuntime& runtime
) {
    GameplayHUDState state;
    state.active = runtime.active();
    state.interaction_target = runtime.interaction_target();
    state.interaction_visible =
        state.interaction_target != InvalidEntity &&
        !runtime.interaction_prompt().empty();
    state.interaction_prompt = runtime.interaction_prompt();
    state.dialogue_visible = runtime.dialogue_active();
    state.status_message = runtime.status_message();

    if (runtime.player_entity() == InvalidEntity ||
        !scene.is_alive(runtime.player_entity())) {
        return state;
    }

    if (const auto* health = scene.get_component<HealthComponent>(runtime.player_entity())) {
        state.health_visible = true;
        state.health_maximum = std::max(0.0f, health->maximum);
        state.health_current = std::clamp(
            health->current,
            0.0f,
            state.health_maximum
        );
    }

    if (const auto* inventory = scene.get_component<InventoryComponent>(runtime.player_entity())) {
        state.inventory = inventory->items;
    }

    scene.for_each<QuestComponent>([&](EntityId, const QuestComponent& quest) {
        if (quest.status != QuestStatus::Active) return;

        std::string objective = quest.title;
        if (quest.required_quantity > 0 && !quest.required_item.empty()) {
            objective += " (" + quest.required_item + " " +
                std::to_string(quest.required_quantity) + ")";
        }
        state.active_objectives.push_back(std::move(objective));
    });

    return state;
}

} // namespace seed
