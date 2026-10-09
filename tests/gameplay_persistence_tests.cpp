#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    const auto test_root = std::filesystem::temp_directory_path() / "seed_gameplay_persistence_test";
    const auto scene_file = test_root / "DoorTest.seedscene";
    std::filesystem::remove_all(test_root);

    seed::Scene scene;
    const auto door_entity = scene.create_entity("Vault Door");
    const std::string persistent_id = scene.entity_persistent_id(door_entity);

    scene.add_component<seed::TransformComponent>(door_entity);
    scene.add_component<seed::InteractableComponent>(door_entity, "Unlock Vault", true);

    seed::BoxColliderComponent door_collider;
    door_collider.half_extents = {0.6f, 1.1f, 0.12f};
    door_collider.offset = {0.1f, 0.0f, 0.0f};
    scene.add_component<seed::BoxColliderComponent>(door_entity, door_collider);

    seed::DoorComponent door;
    door.motion = seed::DoorMotion::Slide;
    door.open_amount = 2.75f;
    door.duration_seconds = 1.4f;
    door.required_item = "VaultKey";
    door.consume_required_item = true;
    door.starts_open = false;
    scene.add_component<seed::DoorComponent>(door_entity, door);

    seed::HealthComponent health;
    health.maximum = 250.0f;
    health.current = 175.0f;
    health.invulnerable = true;
    scene.add_component<seed::HealthComponent>(door_entity, health);

    const auto pickup_entity = scene.create_entity("Vault Key");
    scene.add_component<seed::TransformComponent>(pickup_entity);
    scene.add_component<seed::InteractableComponent>(pickup_entity, "Pick Up Vault Key", true);
    seed::PickupComponent pickup;
    pickup.item_id = "VaultKey";
    pickup.display_name = "Vault Key";
    pickup.quantity = 2;
    pickup.destroy_on_pickup = true;
    scene.add_component<seed::PickupComponent>(pickup_entity, pickup);

    const auto player_entity = scene.create_entity("Player");
    const std::string player_persistent_id = scene.entity_persistent_id(player_entity);
    auto& player_transform = scene.add_component<seed::TransformComponent>(player_entity);
    player_transform.position = {1.0f, 1.7f, 3.0f};

    seed::CameraComponent player_camera;
    player_camera.primary = true;
    player_camera.enabled = true;
    player_camera.editor_only = false;
    player_camera.field_of_view_degrees = 72.0f;
    scene.add_component<seed::CameraComponent>(player_entity, player_camera);

    seed::PlayerControllerComponent player_controller;
    player_controller.move_speed = 5.5f;
    player_controller.fast_multiplier = 3.0f;
    player_controller.look_sensitivity = 0.15f;
    player_controller.interaction_distance = 5.0f;
    player_controller.interaction_radius = 1.5f;
    player_controller.enabled = true;
    scene.add_component<seed::PlayerControllerComponent>(player_entity, player_controller);

    seed::CharacterBodyComponent character_body;
    character_body.radius = 0.4f;
    character_body.height = 1.9f;
    character_body.eye_height = 1.7f;
    character_body.gravity = 20.0f;
    character_body.jump_speed = 7.0f;
    character_body.max_fall_speed = 35.0f;
    scene.add_component<seed::CharacterBodyComponent>(player_entity, character_body);

    seed::InventoryComponent player_inventory;
    player_inventory.items["StarterCoin"] = 3;
    scene.add_component<seed::InventoryComponent>(player_entity, player_inventory);

    std::string error;
    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    assert(loaded.entity_count() == 3);

    seed::EntityId loaded_door_entity = seed::InvalidEntity;
    seed::EntityId loaded_pickup_entity = seed::InvalidEntity;
    seed::EntityId loaded_player_entity = seed::InvalidEntity;
    loaded.for_each_entity([&](seed::EntityId candidate, const std::string& name) {
        if (name == "Vault Door") {
            loaded_door_entity = candidate;
        } else if (name == "Vault Key") {
            loaded_pickup_entity = candidate;
        } else if (name == "Player") {
            loaded_player_entity = candidate;
        }
    });

    assert(loaded_door_entity != seed::InvalidEntity);
    assert(loaded.entity_persistent_id(loaded_door_entity) == persistent_id);

    const auto* loaded_interactable = loaded.get_component<seed::InteractableComponent>(loaded_door_entity);
    const auto* loaded_door = loaded.get_component<seed::DoorComponent>(loaded_door_entity);
    const auto* loaded_health = loaded.get_component<seed::HealthComponent>(loaded_door_entity);
    const auto* loaded_door_collider = loaded.get_component<seed::BoxColliderComponent>(loaded_door_entity);

    assert(loaded_interactable != nullptr);
    assert(loaded_interactable->prompt == "Unlock Vault");
    assert(loaded_interactable->enabled);

    assert(loaded_door != nullptr);
    assert(loaded_door->motion == seed::DoorMotion::Slide);
    assert(nearly_equal(loaded_door->open_amount, 2.75f));
    assert(nearly_equal(loaded_door->duration_seconds, 1.4f));
    assert(loaded_door->required_item == "VaultKey");
    assert(loaded_door->consume_required_item);
    assert(!loaded_door->starts_open);

    assert(loaded_door_collider != nullptr);
    assert(nearly_equal(loaded_door_collider->half_extents.x, 0.6f));
    assert(nearly_equal(loaded_door_collider->half_extents.y, 1.1f));
    assert(nearly_equal(loaded_door_collider->half_extents.z, 0.12f));
    assert(nearly_equal(loaded_door_collider->offset.x, 0.1f));
    assert(loaded_door_collider->enabled);
    assert(loaded_door_collider->solid);

    assert(loaded_health != nullptr);
    assert(nearly_equal(loaded_health->maximum, 250.0f));
    assert(nearly_equal(loaded_health->current, 175.0f));
    assert(loaded_health->invulnerable);

    assert(loaded_pickup_entity != seed::InvalidEntity);
    const auto* loaded_pickup = loaded.get_component<seed::PickupComponent>(loaded_pickup_entity);
    assert(loaded_pickup != nullptr);
    assert(loaded_pickup->item_id == "VaultKey");
    assert(loaded_pickup->display_name == "Vault Key");
    assert(loaded_pickup->quantity == 2);
    assert(loaded_pickup->destroy_on_pickup);

    assert(loaded_player_entity != seed::InvalidEntity);
    assert(loaded.entity_persistent_id(loaded_player_entity) == player_persistent_id);

    const auto* loaded_player_camera = loaded.get_component<seed::CameraComponent>(loaded_player_entity);
    const auto* loaded_player_controller = loaded.get_component<seed::PlayerControllerComponent>(loaded_player_entity);
    const auto* loaded_player_inventory = loaded.get_component<seed::InventoryComponent>(loaded_player_entity);
    const auto* loaded_player_transform = loaded.get_component<seed::TransformComponent>(loaded_player_entity);
    const auto* loaded_character_body = loaded.get_component<seed::CharacterBodyComponent>(loaded_player_entity);

    assert(loaded_player_camera != nullptr);
    assert(loaded_player_camera->primary);
    assert(loaded_player_camera->enabled);
    assert(!loaded_player_camera->editor_only);
    assert(nearly_equal(loaded_player_camera->field_of_view_degrees, 72.0f));

    assert(loaded_player_controller != nullptr);
    assert(nearly_equal(loaded_player_controller->move_speed, 5.5f));
    assert(nearly_equal(loaded_player_controller->fast_multiplier, 3.0f));
    assert(nearly_equal(loaded_player_controller->look_sensitivity, 0.15f));
    assert(nearly_equal(loaded_player_controller->interaction_distance, 5.0f));
    assert(nearly_equal(loaded_player_controller->interaction_radius, 1.5f));
    assert(loaded_player_controller->enabled);

    assert(loaded_character_body != nullptr);
    assert(nearly_equal(loaded_character_body->radius, 0.4f));
    assert(nearly_equal(loaded_character_body->height, 1.9f));
    assert(nearly_equal(loaded_character_body->eye_height, 1.7f));
    assert(nearly_equal(loaded_character_body->gravity, 20.0f));
    assert(nearly_equal(loaded_character_body->jump_speed, 7.0f));
    assert(nearly_equal(loaded_character_body->max_fall_speed, 35.0f));
    assert(loaded_character_body->enabled);

    assert(loaded_player_inventory != nullptr);
    assert(loaded_player_inventory->items.at("StarterCoin") == 3);
    assert(loaded_player_transform != nullptr);
    assert(nearly_equal(loaded_player_transform->position.y, 1.7f));

    std::filesystem::remove_all(test_root);
    std::cout << "SeedGameplayPersistenceTests passed.\n";
    return 0;
}
