#include "seed/diagnostics/SeedDoctor.h"

#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cctype>

namespace seed {
namespace {

bool blank(const std::string& value) {
    return value.empty() || std::all_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
}

void add_finding(
    std::vector<DoctorFinding>& findings,
    const Scene& scene,
    EntityId entity,
    std::string code,
    DoctorSeverity severity,
    std::string message,
    std::string technical_detail,
    DoctorFixKind fix = DoctorFixKind::None,
    std::string fix_label = {}
) {
    findings.push_back({
        .code = std::move(code),
        .severity = severity,
        .entity_persistent_id = scene.entity_persistent_id(entity),
        .entity_name = scene.entity_name(entity),
        .message = std::move(message),
        .technical_detail = std::move(technical_detail),
        .safe_fix = fix,
        .fix_label = std::move(fix_label),
    });
}

bool has_game_camera_child(const Scene& scene, EntityId player) {
    const std::string& player_id = scene.entity_persistent_id(player);
    bool found = false;
    scene.for_each<CameraComponent>([&](EntityId entity, const CameraComponent& camera) {
        if (found || !camera.enabled || camera.editor_only) return;
        const auto* parent = scene.get_component<ParentComponent>(entity);
        if (parent != nullptr && parent->parent_persistent_id == player_id) found = true;
    });
    return found;
}

} // namespace

std::vector<DoctorFinding> SeedDoctor::inspect(const Scene& scene) {
    std::vector<DoctorFinding> findings;

    scene.for_each<DoorComponent>([&](EntityId entity, const DoorComponent&) {
        if (!scene.has_component<InteractableComponent>(entity)) {
            add_finding(
                findings,
                scene,
                entity,
                "gameplay.door.missing_interactable",
                DoctorSeverity::Error,
                "Door cannot be interacted with because Interactable is missing.",
                "DoorComponent depends on InteractableComponent for player interaction.",
                DoctorFixKind::AddInteractable,
                "Add Interactable"
            );
        }
    });

    scene.for_each<PickupComponent>([&](EntityId entity, const PickupComponent& pickup) {
        if (blank(pickup.item_id)) {
            add_finding(
                findings,
                scene,
                entity,
                "gameplay.pickup.missing_item_id",
                DoctorSeverity::Error,
                "Pickup needs an Item ID before it can be collected reliably.",
                "PickupComponent.item_id is empty. Seed uses this ID for inventory identity.",
                DoctorFixKind::None
            );
        }
        if (pickup.quantity <= 0) {
            add_finding(
                findings,
                scene,
                entity,
                "gameplay.pickup.invalid_quantity",
                DoctorSeverity::Error,
                "Pickup quantity must be greater than zero.",
                "PickupComponent.quantity is zero or negative.",
                DoctorFixKind::None
            );
        }
    });

    scene.for_each<PlayerControllerComponent>([&](EntityId entity, const PlayerControllerComponent& player) {
        if (!scene.has_component<InventoryComponent>(entity)) {
            add_finding(
                findings,
                scene,
                entity,
                "gameplay.player.missing_inventory",
                DoctorSeverity::Warning,
                "Player has no Inventory, so item pickups cannot be retained.",
                "PlayerControllerComponent is present without InventoryComponent.",
                DoctorFixKind::AddInventory,
                "Add Inventory"
            );
        }

        if (!scene.has_component<CharacterBodyComponent>(entity)) {
            add_finding(
                findings,
                scene,
                entity,
                "physics.player.missing_character_body",
                DoctorSeverity::Info,
                "Player has no Character Body, so movement will not use gravity, jumping, or solid collision.",
                "PlayerControllerComponent is present without CharacterBodyComponent. This can be intentional for free-fly players.",
                DoctorFixKind::AddCharacterBody,
                "Add Character Body"
            );
        }

        if (player.view_mode == PlayerViewMode::ThirdPerson && !has_game_camera_child(scene, entity)) {
            add_finding(
                findings,
                scene,
                entity,
                "camera.third_person.missing_child_camera",
                DoctorSeverity::Error,
                "Third-Person Player is missing its game Camera.",
                "Third-person camera resolution expects an enabled, non-editor CameraComponent parented to the Player persistent ID.",
                DoctorFixKind::None
            );
        }
    });

    return findings;
}

DoctorFixResult SeedDoctor::apply_safe_fix(Scene& scene, const DoctorFinding& finding) {
    const EntityId entity = scene.find_entity_by_persistent_id(finding.entity_persistent_id);
    if (entity == InvalidEntity || !scene.is_alive(entity)) {
        return {.applied = false, .message = "Seed Doctor could not find the affected entity anymore."};
    }

    switch (finding.safe_fix) {
    case DoctorFixKind::AddInteractable:
        if (!scene.has_component<InteractableComponent>(entity)) {
            scene.add_component<InteractableComponent>(entity);
            return {.applied = true, .message = "Added Interactable."};
        }
        return {.applied = false, .message = "Interactable is already present."};

    case DoctorFixKind::AddInventory:
        if (!scene.has_component<InventoryComponent>(entity)) {
            scene.add_component<InventoryComponent>(entity);
            return {.applied = true, .message = "Added Inventory."};
        }
        return {.applied = false, .message = "Inventory is already present."};

    case DoctorFixKind::AddCharacterBody:
        if (!scene.has_component<CharacterBodyComponent>(entity)) {
            scene.add_component<CharacterBodyComponent>(entity);
            return {.applied = true, .message = "Added Character Body with Seed defaults."};
        }
        return {.applied = false, .message = "Character Body is already present."};

    case DoctorFixKind::None:
    default:
        return {.applied = false, .message = "This finding requires creator review and has no automatic fix."};
    }
}

} // namespace seed
