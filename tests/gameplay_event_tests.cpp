#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayEvents.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <iostream>

namespace {

seed::EntityId add_player(seed::Scene& scene) {
    const auto player = scene.create_entity("Player");
    scene.add_component<seed::TransformComponent>(player);
    scene.add_component<seed::PlayerControllerComponent>(player);
    scene.add_component<seed::InventoryComponent>(player);
    return player;
}

seed::EntityId add_pickup(seed::Scene& scene) {
    const auto pickup = scene.create_entity("Key");
    auto& transform = scene.add_component<seed::TransformComponent>(pickup);
    transform.position = {0.0f, 0.0f, -1.0f};
    scene.add_component<seed::InteractableComponent>(pickup, "Pick Up Key", true);

    seed::PickupComponent item;
    item.item_id = "Key";
    item.display_name = "Key";
    scene.add_component<seed::PickupComponent>(pickup, item);
    return pickup;
}

seed::PlatformEvent key_event(seed::KeyCode key, seed::ButtonState state = seed::ButtonState::Pressed) {
    seed::PlatformEvent event;
    event.type = seed::PlatformEventType::Key;
    event.key = key;
    event.button_state = state;
    return event;
}

} // namespace

int main() {
    {
        seed::Scene scene;
        const auto player = add_player(scene);
        seed::GameplayRuntime runtime;
        seed::GameplayEventBus events;

        assert(runtime.start(scene, player));
        runtime.handle_event(key_event(seed::KeyCode::W));
        runtime.update(scene, 0.5, &events);

        bool found_footstep = false;
        for (const auto& event : events.pending()) {
            if (event.source_entity == player &&
                event.type == seed::GameplayEventType::Footstep &&
                event.name == "Footstep") {
                found_footstep = true;
            }
        }
        assert(found_footstep);
    }

    {
        seed::Scene scene;
        const auto player = add_player(scene);
        const auto pickup = add_pickup(scene);
        seed::GameplayRuntime runtime;
        seed::GameplayEventBus events;

        assert(runtime.start(scene, player));
        runtime.update(scene, 0.0, &events);
        assert(runtime.interaction_target() == pickup);

        runtime.handle_event(key_event(seed::KeyCode::E));
        runtime.update(scene, 0.0, &events);

        bool found_interaction = false;
        for (const auto& event : events.pending()) {
            if (event.source_entity == pickup &&
                event.type == seed::GameplayEventType::Interaction &&
                event.name == "PickupCollected") {
                found_interaction = true;
            }
        }
        assert(found_interaction);
    }

    std::cout << "SeedGameplayEventTests passed.\n";
    return 0;
}
