#include "seed/diagnostics/SeedDoctor.h"
#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

const seed::DoctorFinding* find_code(
    const std::vector<seed::DoctorFinding>& findings,
    const std::string& code
) {
    for (const auto& finding : findings) {
        if (finding.code == code) return &finding;
    }
    return nullptr;
}

} // namespace

int main() {
    seed::Scene scene;

    const seed::EntityId door = scene.create_entity("Cabin Door", "door-1");
    scene.add_component<seed::DoorComponent>(door);

    const seed::EntityId pickup = scene.create_entity("Broken Pickup", "pickup-1");
    seed::PickupComponent pickup_component;
    pickup_component.item_id = "";
    pickup_component.quantity = 0;
    scene.add_component<seed::PickupComponent>(pickup, pickup_component);

    const seed::EntityId player = scene.create_entity("Player", "player-1");
    seed::PlayerControllerComponent player_controller;
    player_controller.view_mode = seed::PlayerViewMode::ThirdPerson;
    player_controller.camera_distance = 5.0f;
    player_controller.camera_height = 1.7f;
    player_controller.camera_shoulder_offset = 0.25f;
    scene.add_component<seed::PlayerControllerComponent>(player, player_controller);
    scene.add_component<seed::TransformComponent>(player);

    std::vector<seed::DoctorFinding> findings = seed::SeedDoctor::inspect(scene);

    const auto* door_finding = find_code(findings, "gameplay.door.missing_interactable");
    const auto* pickup_id_finding = find_code(findings, "gameplay.pickup.missing_item_id");
    const auto* pickup_quantity_finding = find_code(findings, "gameplay.pickup.invalid_quantity");
    const auto* inventory_finding = find_code(findings, "gameplay.player.missing_inventory");
    const auto* body_finding = find_code(findings, "physics.player.missing_character_body");
    const auto* camera_finding = find_code(findings, "camera.third_person.missing_child_camera");

    assert(door_finding != nullptr);
    assert(door_finding->severity == seed::DoctorSeverity::Error);
    assert(door_finding->safe_fix == seed::DoctorFixKind::AddInteractable);
    assert(door_finding->entity_persistent_id == "door-1");

    assert(pickup_id_finding != nullptr);
    assert(pickup_quantity_finding != nullptr);
    assert(pickup_id_finding->safe_fix == seed::DoctorFixKind::None);
    assert(pickup_quantity_finding->safe_fix == seed::DoctorFixKind::None);

    assert(inventory_finding != nullptr);
    assert(inventory_finding->safe_fix == seed::DoctorFixKind::AddInventory);
    assert(body_finding != nullptr);
    assert(body_finding->safe_fix == seed::DoctorFixKind::AddCharacterBody);
    assert(camera_finding != nullptr);
    assert(camera_finding->safe_fix == seed::DoctorFixKind::AddThirdPersonCamera);

    auto fix_result = seed::SeedDoctor::apply_safe_fix(scene, *door_finding);
    assert(fix_result.applied);
    assert(scene.has_component<seed::InteractableComponent>(door));

    fix_result = seed::SeedDoctor::apply_safe_fix(scene, *inventory_finding);
    assert(fix_result.applied);
    assert(scene.has_component<seed::InventoryComponent>(player));

    fix_result = seed::SeedDoctor::apply_safe_fix(scene, *body_finding);
    assert(fix_result.applied);
    assert(scene.has_component<seed::CharacterBodyComponent>(player));

    fix_result = seed::SeedDoctor::apply_safe_fix(scene, *camera_finding);
    assert(fix_result.applied);

    seed::EntityId camera = seed::InvalidEntity;
    scene.for_each<seed::CameraComponent>([&](seed::EntityId entity, const seed::CameraComponent& component) {
        if (camera != seed::InvalidEntity || !component.enabled || component.editor_only) return;
        const auto* parent = scene.get_component<seed::ParentComponent>(entity);
        if (parent != nullptr && parent->parent_persistent_id == "player-1") camera = entity;
    });
    assert(camera != seed::InvalidEntity);
    const auto* camera_transform = scene.get_component<seed::TransformComponent>(camera);
    assert(camera_transform != nullptr);
    assert(camera_transform->position.x == 0.25f);
    assert(camera_transform->position.y == 1.7f);
    assert(camera_transform->position.z == 5.0f);

    findings = seed::SeedDoctor::inspect(scene);
    assert(find_code(findings, "gameplay.door.missing_interactable") == nullptr);
    assert(find_code(findings, "gameplay.player.missing_inventory") == nullptr);
    assert(find_code(findings, "physics.player.missing_character_body") == nullptr);
    assert(find_code(findings, "camera.third_person.missing_child_camera") == nullptr);

    // Creator-review findings remain until the creator supplies intent.
    assert(find_code(findings, "gameplay.pickup.missing_item_id") != nullptr);
    assert(find_code(findings, "gameplay.pickup.invalid_quantity") != nullptr);

    std::cout << "Seed Doctor diagnostics + safe fix tests passed.\n";
    return 0;
}
