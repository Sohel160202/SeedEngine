#include "seed/animation/AnimationComponents.h"
#include "seed/diagnostics/SeedDoctor.h"
#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/scene/Scene.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << "SeedDoctorTests FAILED: " << message << '\n';
    std::exit(EXIT_FAILURE);
}

const seed::DoctorFinding* find_code(
    const std::vector<seed::DoctorFinding>& findings,
    const std::string& code
) {
    for (const auto& finding : findings) {
        if (finding.code == code) return &finding;
    }
    return nullptr;
}

const seed::DoctorFinding* find_code_for_entity(
    const std::vector<seed::DoctorFinding>& findings,
    const std::string& code,
    const std::string& persistent_id
) {
    for (const auto& finding : findings) {
        if (finding.code == code && finding.entity_persistent_id == persistent_id) return &finding;
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

    const seed::EntityId empty_persistent =
        scene.create_entity("Empty Persistent", "empty-persistent-1");
    scene.add_component<seed::PersistentComponent>(empty_persistent);

    const seed::EntityId missing_character =
        scene.create_entity("Broken Character", "character-missing-1");
    scene.add_component<seed::SkinnedMeshComponent>(missing_character);

    const seed::EntityId wrong_character_asset =
        scene.create_entity("Static Asset As Character", "character-wrong-asset-1");
    scene.add_component<seed::TransformComponent>(wrong_character_asset);
    scene.add_component<seed::MaterialComponent>(wrong_character_asset);
    seed::SkinnedMeshComponent wrong_mesh;
    wrong_mesh.asset_id = "model:Assets/Hero.glb";
    scene.add_component<seed::SkinnedMeshComponent>(wrong_character_asset, wrong_mesh);

    const seed::EntityId valid_authored_character =
        scene.create_entity("Authored Character", "character-valid-1");
    scene.add_component<seed::TransformComponent>(valid_authored_character);
    scene.add_component<seed::MaterialComponent>(valid_authored_character);
    seed::SkinnedMeshComponent valid_mesh;
    valid_mesh.asset_id = "character:Assets/Hero.glb";
    scene.add_component<seed::SkinnedMeshComponent>(valid_authored_character, valid_mesh);

    const seed::EntityId orphan_animation_player =
        scene.create_entity("Orphan Animation Player", "animation-orphan-1");
    scene.add_component<seed::AnimationPlayerComponent>(orphan_animation_player);

    std::vector<seed::DoctorFinding> findings = seed::SeedDoctor::inspect(scene);

    const auto* door_finding = find_code(findings, "gameplay.door.missing_interactable");
    const auto* pickup_id_finding = find_code(findings, "gameplay.pickup.missing_item_id");
    const auto* pickup_quantity_finding = find_code(findings, "gameplay.pickup.invalid_quantity");
    const auto* inventory_finding = find_code(findings, "gameplay.player.missing_inventory");
    const auto* body_finding = find_code(findings, "physics.player.missing_character_body");
    const auto* camera_finding = find_code(findings, "camera.third_person.missing_child_camera");
    const auto* persistent_finding = find_code(findings, "save.persistent.no_supported_state");

    require(door_finding != nullptr, "Door missing-Interactable finding");
    require(door_finding->severity == seed::DoctorSeverity::Error, "Door severity");
    require(door_finding->safe_fix == seed::DoctorFixKind::AddInteractable, "Door safe fix");
    require(door_finding->entity_persistent_id == "door-1", "Door finding entity identity");

    require(pickup_id_finding != nullptr, "Pickup missing-item-id finding");
    require(pickup_quantity_finding != nullptr, "Pickup invalid-quantity finding");
    require(pickup_id_finding->safe_fix == seed::DoctorFixKind::None, "Pickup ID remains creator-reviewed");
    require(pickup_quantity_finding->safe_fix == seed::DoctorFixKind::None, "Pickup quantity remains creator-reviewed");

    require(inventory_finding != nullptr, "Player missing-Inventory finding");
    require(inventory_finding->safe_fix == seed::DoctorFixKind::AddInventory, "Player Inventory safe fix");
    require(body_finding != nullptr, "Player missing-CharacterBody finding");
    require(body_finding->safe_fix == seed::DoctorFixKind::AddCharacterBody, "Player CharacterBody safe fix");
    require(camera_finding != nullptr, "Third-Person missing-camera finding");
    require(camera_finding->safe_fix == seed::DoctorFixKind::AddThirdPersonCamera, "Third-Person camera safe fix");

    require(persistent_finding != nullptr, "Persistent empty-state finding");
    require(persistent_finding->severity == seed::DoctorSeverity::Warning, "Persistent warning severity");
    require(persistent_finding->safe_fix == seed::DoctorFixKind::None, "Persistent state remains creator-reviewed");
    require(persistent_finding->entity_persistent_id == "empty-persistent-1", "Persistent finding entity identity");

    const auto* missing_asset = find_code_for_entity(
        findings,
        "animation.character.missing_asset",
        "character-missing-1"
    );
    const auto* missing_transform = find_code_for_entity(
        findings,
        "animation.character.missing_transform",
        "character-missing-1"
    );
    const auto* missing_material = find_code_for_entity(
        findings,
        "animation.character.missing_material",
        "character-missing-1"
    );
    const auto* invalid_asset = find_code_for_entity(
        findings,
        "animation.character.invalid_asset_type",
        "character-wrong-asset-1"
    );
    const auto* orphan_player = find_code_for_entity(
        findings,
        "animation.player.missing_skinned_mesh",
        "animation-orphan-1"
    );

    require(missing_asset != nullptr, "Animated Character missing-asset finding");
    require(missing_asset->severity == seed::DoctorSeverity::Error, "Animated Character missing-asset severity");
    require(missing_transform != nullptr, "Animated Character missing-Transform finding");
    require(missing_material != nullptr, "Animated Character missing-Material finding");
    require(invalid_asset != nullptr, "Animated Character invalid asset-type finding");
    require(invalid_asset->severity == seed::DoctorSeverity::Error, "Animated Character invalid asset severity");
    require(orphan_player != nullptr, "Orphan Animation Player finding");
    require(orphan_player->severity == seed::DoctorSeverity::Warning, "Orphan Animation Player severity");

    // A normal authored character intentionally has no AnimationPlayer before
    // AssetRuntime hydration. Doctor must not diagnose that as broken because
    // bind_scene() creates and initializes AnimationPlayerComponent itself.
    require(
        find_code_for_entity(findings, "animation.character.missing_asset", "character-valid-1") == nullptr,
        "Valid character has no missing-asset finding"
    );
    require(
        find_code_for_entity(findings, "animation.character.invalid_asset_type", "character-valid-1") == nullptr,
        "Valid character has no invalid-asset finding"
    );
    require(
        find_code_for_entity(findings, "animation.character.missing_transform", "character-valid-1") == nullptr,
        "Valid character has no missing-Transform finding"
    );
    require(
        find_code_for_entity(findings, "animation.character.missing_material", "character-valid-1") == nullptr,
        "Valid character has no missing-Material finding"
    );

    auto fix_result = seed::SeedDoctor::apply_safe_fix(scene, *door_finding);
    require(fix_result.applied, "Apply Door safe fix");
    require(scene.has_component<seed::InteractableComponent>(door), "Door safe fix added Interactable");

    fix_result = seed::SeedDoctor::apply_safe_fix(scene, *inventory_finding);
    require(fix_result.applied, "Apply Player Inventory fix");
    require(scene.has_component<seed::InventoryComponent>(player), "Player Inventory safe fix added component");

    fix_result = seed::SeedDoctor::apply_safe_fix(scene, *body_finding);
    require(fix_result.applied, "Apply CharacterBody fix");
    require(scene.has_component<seed::CharacterBodyComponent>(player), "CharacterBody safe fix added component");

    fix_result = seed::SeedDoctor::apply_safe_fix(scene, *camera_finding);
    require(fix_result.applied, "Apply Third-Person Camera fix");

    seed::EntityId camera = seed::InvalidEntity;
    scene.for_each<seed::CameraComponent>([&](seed::EntityId entity, const seed::CameraComponent& component) {
        if (camera != seed::InvalidEntity || !component.enabled || component.editor_only) return;
        const auto* parent = scene.get_component<seed::ParentComponent>(entity);
        if (parent != nullptr && parent->parent_persistent_id == "player-1") camera = entity;
    });
    require(camera != seed::InvalidEntity, "Third-Person Camera fix created camera");
    const auto* camera_transform = scene.get_component<seed::TransformComponent>(camera);
    require(camera_transform != nullptr, "Third-Person Camera has Transform");
    require(camera_transform->position.x == 0.25f, "Third-Person Camera shoulder offset");
    require(camera_transform->position.y == 1.7f, "Third-Person Camera height");
    require(camera_transform->position.z == 5.0f, "Third-Person Camera distance");

    // Add a supported v0 state to prove Doctor clears the persistence warning
    // without inventing any other component or guessing creator intent.
    scene.add_component<seed::HealthComponent>(empty_persistent);

    // Repair the deliberately broken animated-character authoring so the new
    // Doctor findings prove they disappear when the semantic requirements are met.
    auto* repaired_mesh = scene.get_component<seed::SkinnedMeshComponent>(missing_character);
    require(repaired_mesh != nullptr, "Broken character still has SkinnedMesh");
    repaired_mesh->asset_id = "character:Assets/RepairedHero.glb";
    scene.add_component<seed::TransformComponent>(missing_character);
    scene.add_component<seed::MaterialComponent>(missing_character);
    require(scene.remove_component<seed::AnimationPlayerComponent>(orphan_animation_player), "Remove orphan AnimationPlayer");

    findings = seed::SeedDoctor::inspect(scene);
    require(find_code(findings, "gameplay.door.missing_interactable") == nullptr, "Door finding clears after fix");
    require(find_code(findings, "gameplay.player.missing_inventory") == nullptr, "Inventory finding clears after fix");
    require(find_code(findings, "physics.player.missing_character_body") == nullptr, "CharacterBody finding clears after fix");
    require(find_code(findings, "camera.third_person.missing_child_camera") == nullptr, "Camera finding clears after fix");
    require(find_code(findings, "save.persistent.no_supported_state") == nullptr, "Persistent finding clears after supported state added");
    require(
        find_code_for_entity(findings, "animation.character.missing_asset", "character-missing-1") == nullptr,
        "Character missing-asset finding clears after repair"
    );
    require(
        find_code_for_entity(findings, "animation.character.missing_transform", "character-missing-1") == nullptr,
        "Character missing-Transform finding clears after repair"
    );
    require(
        find_code_for_entity(findings, "animation.character.missing_material", "character-missing-1") == nullptr,
        "Character missing-Material finding clears after repair"
    );
    require(
        find_code_for_entity(findings, "animation.player.missing_skinned_mesh", "animation-orphan-1") == nullptr,
        "Orphan Animation Player finding clears after component removal"
    );

    // Creator-review findings remain until the creator supplies intent.
    require(find_code(findings, "gameplay.pickup.missing_item_id") != nullptr, "Pickup ID finding remains");
    require(find_code(findings, "gameplay.pickup.invalid_quantity") != nullptr, "Pickup quantity finding remains");
    require(
        find_code_for_entity(findings, "animation.character.invalid_asset_type", "character-wrong-asset-1") != nullptr,
        "Invalid character asset remains until creator assigns correct asset"
    );

    std::cout << "Seed Doctor diagnostics + safe fix tests passed.\n";
    return 0;
}
