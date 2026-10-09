#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <iostream>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

seed::EntityId add_player(seed::Scene& scene) {
    const auto player = scene.create_entity("Player");
    auto& transform = scene.add_component<seed::TransformComponent>(player);
    transform.position = {0.0f, 0.0f, 0.0f};
    transform.rotation_degrees = {0.0f, 0.0f, 0.0f};
    scene.add_component<seed::PlayerControllerComponent>(player);
    scene.add_component<seed::InventoryComponent>(player);
    return player;
}

seed::EntityId add_door(
    seed::Scene& scene,
    std::string required_item = {},
    bool consume_required_item = false,
    float z = -2.0f
) {
    const auto door_entity = scene.create_entity("Test Door");
    auto& transform = scene.add_component<seed::TransformComponent>(door_entity);
    transform.position = {0.0f, 0.0f, z};
    transform.rotation_degrees = {0.0f, 0.0f, 0.0f};
    scene.add_component<seed::InteractableComponent>(door_entity, "Open Door", true);

    seed::DoorComponent door;
    door.motion = seed::DoorMotion::Rotate;
    door.open_amount = 90.0f;
    door.duration_seconds = 1.0f;
    door.required_item = std::move(required_item);
    door.consume_required_item = consume_required_item;
    scene.add_component<seed::DoorComponent>(door_entity, door);
    return door_entity;
}

seed::EntityId add_pickup(
    seed::Scene& scene,
    std::string item_id,
    std::string display_name,
    int quantity = 1,
    float z = -1.0f
) {
    const auto pickup_entity = scene.create_entity(display_name);
    auto& transform = scene.add_component<seed::TransformComponent>(pickup_entity);
    transform.position = {0.0f, 0.0f, z};
    scene.add_component<seed::InteractableComponent>(
        pickup_entity,
        "Pick Up " + display_name,
        true
    );

    seed::PickupComponent pickup;
    pickup.item_id = std::move(item_id);
    pickup.display_name = std::move(display_name);
    pickup.quantity = quantity;
    pickup.destroy_on_pickup = true;
    scene.add_component<seed::PickupComponent>(pickup_entity, pickup);
    return pickup_entity;
}

seed::PlatformEvent interact_event() {
    seed::PlatformEvent event;
    event.type = seed::PlatformEventType::Key;
    event.key = seed::KeyCode::E;
    event.button_state = seed::ButtonState::Pressed;
    return event;
}

} // namespace

int main() {
    {
        seed::Scene edit_scene;
        const auto edit_door = add_door(edit_scene);
        const auto* original_transform = edit_scene.get_component<seed::TransformComponent>(edit_door);
        assert(original_transform != nullptr);
        assert(nearly_equal(original_transform->rotation_degrees.y, 0.0f));

        seed::Scene play_scene = edit_scene;
        const auto player = add_player(play_scene);

        seed::GameplayRuntime runtime;
        assert(runtime.start(play_scene, player));
        runtime.update(play_scene, 0.0);
        assert(runtime.interaction_target() == edit_door);
        assert(runtime.interaction_prompt().find("Open Door") != std::string::npos);

        runtime.handle_event(interact_event());
        runtime.update(play_scene, 0.0);
        runtime.update(play_scene, 0.5);

        const auto* play_transform = play_scene.get_component<seed::TransformComponent>(edit_door);
        assert(play_transform != nullptr);
        assert(play_transform->rotation_degrees.y > 40.0f);
        assert(play_transform->rotation_degrees.y < 50.0f);

        // The editor scene is a deep clone source and must remain unchanged.
        original_transform = edit_scene.get_component<seed::TransformComponent>(edit_door);
        assert(original_transform != nullptr);
        assert(nearly_equal(original_transform->rotation_degrees.y, 0.0f));
    }

    {
        seed::Scene scene;
        const auto door = add_door(scene, "VaultKey");
        const auto player = add_player(scene);

        seed::GameplayRuntime runtime;
        assert(runtime.start(scene, player));
        runtime.update(scene, 0.0);
        assert(runtime.interaction_target() == door);
        assert(runtime.interaction_prompt().find("Requires VaultKey") != std::string::npos);

        runtime.handle_event(interact_event());
        runtime.update(scene, 0.0);
        runtime.update(scene, 1.0);

        const auto* transform = scene.get_component<seed::TransformComponent>(door);
        assert(transform != nullptr);
        assert(nearly_equal(transform->rotation_degrees.y, 0.0f));
        assert(runtime.status_message().find("requires VaultKey") != std::string::npos);

        auto* inventory = scene.get_component<seed::InventoryComponent>(player);
        assert(inventory != nullptr);
        inventory->items["VaultKey"] = 1;

        runtime.update(scene, 0.0);
        runtime.handle_event(interact_event());
        runtime.update(scene, 0.0);
        runtime.update(scene, 1.0);

        transform = scene.get_component<seed::TransformComponent>(door);
        assert(transform != nullptr);
        assert(transform->rotation_degrees.y > 89.0f);
    }

    // Complete beginner-facing loop: collect key -> inventory -> consuming locked door.
    {
        seed::Scene scene;
        const auto pickup = add_pickup(scene, "VaultKey", "Vault Key", 1, -1.0f);
        const auto door = add_door(scene, "VaultKey", true, -2.5f);
        const auto player = add_player(scene);

        seed::GameplayRuntime runtime;
        assert(runtime.start(scene, player));
        runtime.update(scene, 0.0);

        assert(runtime.interaction_target() == pickup);
        assert(runtime.interaction_prompt().find("Pick Up Vault Key") != std::string::npos);

        runtime.handle_event(interact_event());
        runtime.update(scene, 0.0);

        assert(!scene.is_alive(pickup));
        auto* inventory = scene.get_component<seed::InventoryComponent>(player);
        assert(inventory != nullptr);
        assert(inventory->items["VaultKey"] == 1);
        assert(runtime.status_message().find("Picked up Vault Key") != std::string::npos);

        runtime.update(scene, 0.0);
        assert(runtime.interaction_target() == door);
        assert(runtime.interaction_prompt().find("Requires VaultKey") == std::string::npos);

        runtime.handle_event(interact_event());
        runtime.update(scene, 0.0);
        runtime.update(scene, 1.0);

        const auto* door_transform = scene.get_component<seed::TransformComponent>(door);
        assert(door_transform != nullptr);
        assert(door_transform->rotation_degrees.y > 89.0f);
        assert(!inventory->items.contains("VaultKey"));
    }

    std::cout << "SeedGameplayRuntimeTests passed.\n";
    return 0;
}
