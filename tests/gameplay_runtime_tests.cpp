#include "seed/animation/AnimationComponents.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/scene/Scene.h"
#include "seed/simulation/SimulationComponents.h"

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

seed::PlatformEvent key_event(seed::KeyCode key, seed::ButtonState state = seed::ButtonState::Pressed) {
    seed::PlatformEvent event;
    event.type = seed::PlatformEventType::Key;
    event.key = key;
    event.button_state = state;
    return event;
}

seed::PlatformEvent interact_event() {
    return key_event(seed::KeyCode::E);
}

} // namespace

int main() {
    {
        seed::Scene scene;
        const auto player = add_player(scene);
        const auto shop = scene.create_entity("Fresh Market");
        seed::SimulationClockComponent clock;
        clock.day = 1;
        clock.hour = 23.5f;
        scene.add_component<seed::SimulationClockComponent>(shop, clock);
        seed::ShopMetricsComponent metrics;
        metrics.tracked_day = 1;
        metrics.customers_served_today = 4;
        metrics.orders_completed_today = 3;
        metrics.products_sold_today = 7;
        metrics.daily_revenue = 80.0f;
        metrics.daily_satisfaction = 60.0f;
        metrics.satisfaction_samples = 2;
        scene.add_component<seed::ShopMetricsComponent>(shop, metrics);

        seed::GameplayRuntime runtime;
        assert(runtime.start(scene, player));
        runtime.update(scene, 60.0);

        const auto* advanced = scene.get_component<seed::SimulationClockComponent>(shop);
        assert(advanced != nullptr);
        assert(advanced->day == 2);
        assert(nearly_equal(advanced->hour, 0.5f));
        const auto* reset_metrics = scene.get_component<seed::ShopMetricsComponent>(shop);
        assert(reset_metrics != nullptr);
        assert(reset_metrics->tracked_day == 2);
        assert(reset_metrics->customers_served_today == 0);
        assert(reset_metrics->orders_completed_today == 0);
        assert(reset_metrics->products_sold_today == 0);
        assert(nearly_equal(reset_metrics->daily_revenue, 0.0f));
        assert(nearly_equal(reset_metrics->daily_satisfaction, 100.0f));
        assert(reset_metrics->satisfaction_samples == 0);
    }

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
        const auto* animation = play_scene.get_component<seed::AnimationPlayerComponent>(player);
        assert(animation != nullptr);
        assert(animation->locomotion_enabled);
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

    // Customers leave when a checkout queue exceeds their patience.
    {
        seed::Scene scene;
        const auto player = add_player(scene);

        const auto shop = scene.create_entity("Fresh Market");
        scene.add_component<seed::ShopInventoryComponent>(shop);
        scene.add_component<seed::WalletComponent>(shop);

        const auto customer_entity = scene.create_entity("Impatient Customer");
        auto& customer_transform = scene.add_component<seed::TransformComponent>(customer_entity);
        customer_transform.position = {0.0f, 0.9f, -1.0f};
        seed::CustomerBehaviorComponent customer;
        customer.state = "queued";
        customer.timer_seconds = 0.0f;
        customer.queue_patience_seconds = 0.25f;
        scene.add_component<seed::CustomerBehaviorComponent>(customer_entity, customer);
        seed::CustomerOrderComponent order;
        order.customer_name = "Impatient Customer";
        order.lines.push_back({"milk", 1, 0, 5.0f});
        scene.add_component<seed::CustomerOrderComponent>(customer_entity, order);

        seed::GameplayRuntime runtime;
        assert(runtime.start(scene, player));
        runtime.update(scene, 0.5);

        const auto* updated_customer =
            scene.get_component<seed::CustomerBehaviorComponent>(customer_entity);
        assert(updated_customer != nullptr);
        assert(updated_customer->state == "leaving");
        assert(runtime.status_message().find("queue was too long") != std::string::npos);
    }

    // Seed Physics v0: gravity/grounding, jump, and wall blocking.
    {
        seed::Scene scene;

        const auto floor = scene.create_entity("Floor");
        auto& floor_transform = scene.add_component<seed::TransformComponent>(floor);
        floor_transform.position = {0.0f, -0.25f, 0.0f};
        seed::BoxColliderComponent floor_collider;
        floor_collider.half_extents = {5.0f, 0.25f, 5.0f};
        scene.add_component<seed::BoxColliderComponent>(floor, floor_collider);

        const auto wall = scene.create_entity("Wall");
        auto& wall_transform = scene.add_component<seed::TransformComponent>(wall);
        wall_transform.position = {0.0f, 1.0f, -2.0f};
        seed::BoxColliderComponent wall_collider;
        wall_collider.half_extents = {2.0f, 1.0f, 0.25f};
        scene.add_component<seed::BoxColliderComponent>(wall, wall_collider);

        const auto player = add_player(scene);
        auto* player_transform = scene.get_component<seed::TransformComponent>(player);
        assert(player_transform != nullptr);
        player_transform->position = {0.0f, 2.6f, 0.0f};
        scene.add_component<seed::CharacterBodyComponent>(player);

        seed::GameplayRuntime runtime;
        assert(runtime.start(scene, player));

        for (int i = 0; i < 50; ++i) {
            runtime.update(scene, 0.05);
        }

        player_transform = scene.get_component<seed::TransformComponent>(player);
        assert(player_transform != nullptr);
        assert(nearly_equal(player_transform->position.y, 1.65f, 0.02f));
        assert(runtime.player_grounded());

        runtime.handle_event(key_event(seed::KeyCode::Space));
        runtime.update(scene, 0.1);
        assert(player_transform->position.y > 1.9f);
        assert(!runtime.player_grounded());

        for (int i = 0; i < 50; ++i) {
            runtime.update(scene, 0.05);
        }
        assert(nearly_equal(player_transform->position.y, 1.65f, 0.02f));
        assert(runtime.player_grounded());

        runtime.handle_event(key_event(seed::KeyCode::W));
        runtime.update(scene, 1.0);
        runtime.handle_event(key_event(seed::KeyCode::W, seed::ButtonState::Released));

        // Wall front face is z=-1.75; character radius is 0.35, so the eye
        // should stop around z=-1.40 instead of passing through.
        assert(player_transform->position.z > -1.45f);
        assert(player_transform->position.z < -1.30f);
        assert(runtime.player_grounded());
    }

    std::cout << "SeedGameplayRuntimeTests passed.\n";
    return 0;
}
