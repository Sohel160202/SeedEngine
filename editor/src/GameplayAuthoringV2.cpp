#include "GameplayAuthoring.h"

// Preserve the mature gameplay authoring implementation and extend it without
// duplicating the existing Door/Pickup/Inventory/Health UI. These macros rename
// the two public entry points only inside the included implementation.
#define draw_gameplay_components draw_gameplay_components_legacy
#define draw_add_gameplay_popup draw_add_gameplay_popup_legacy
#include "GameplayAuthoring.cpp"
#undef draw_gameplay_components
#undef draw_add_gameplay_popup

#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Hierarchy.h"
#include "seed/scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <string>

namespace seed::studio {
namespace {

EntityId third_person_camera(Scene& scene, EntityId player) {
    if (player == InvalidEntity || !scene.is_alive(player)) return InvalidEntity;
    const std::string& player_id = scene.entity_persistent_id(player);
    EntityId result = InvalidEntity;
    scene.for_each<TransformComponent, CameraComponent>(
        [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
            if (result != InvalidEntity || camera.editor_only) return;
            const auto* parent = scene.get_component<ParentComponent>(entity);
            if (parent != nullptr && parent->parent_persistent_id == player_id) result = entity;
        }
    );
    return result;
}

void mark_player_root_camera(Scene& scene, EntityId player) {
    // Studio's current Play discovery identifies Player + Camera on one entity.
    // Third Person keeps this marker for discovery only; Runtime and RenderSystem
    // deliberately route actual viewing through the child orbit camera.
    CameraComponent marker;
    marker.primary = true;
    marker.enabled = true;
    marker.editor_only = false;
    marker.field_of_view_degrees = 65.0f;
    if (auto* existing = scene.get_component<CameraComponent>(player)) {
        *existing = marker;
    } else {
        scene.add_component<CameraComponent>(player, marker);
    }

    scene.for_each<CameraComponent>([&](EntityId entity, CameraComponent& camera) {
        if (!camera.editor_only) camera.primary = entity == player;
    });
}

EntityId ensure_third_person_dependencies(Scene& scene, EntityId player) {
    if (!scene.has_component<TransformComponent>(player)) scene.add_component<TransformComponent>(player);
    if (!scene.has_component<InventoryComponent>(player)) scene.add_component<InventoryComponent>(player);
    if (!scene.has_component<CharacterBodyComponent>(player)) scene.add_component<CharacterBodyComponent>(player);

    mark_player_root_camera(scene, player);

    EntityId camera_entity = third_person_camera(scene, player);
    if (camera_entity == InvalidEntity) {
        camera_entity = scene.create_entity("Third Person Camera");
        TransformComponent camera_transform;
        camera_transform.position = {0.0f, 1.55f, 4.5f};
        camera_transform.rotation_degrees = {-12.0f, 0.0f, 0.0f};
        scene.add_component<TransformComponent>(camera_entity, camera_transform);

        CameraComponent camera;
        camera.primary = false;
        camera.enabled = true;
        camera.editor_only = false;
        camera.field_of_view_degrees = 65.0f;
        scene.add_component<CameraComponent>(camera_entity, camera);
        set_parent(scene, camera_entity, player, false);
    }
    return camera_entity;
}

void convert_to_third_person(Scene& scene, EntityId entity) {
    auto* player = scene.get_component<PlayerControllerComponent>(entity);
    if (player == nullptr) {
        PlayerControllerComponent controller;
        controller.view_mode = PlayerViewMode::ThirdPerson;
        scene.add_component<PlayerControllerComponent>(entity, controller);
    } else {
        player->view_mode = PlayerViewMode::ThirdPerson;
    }
    ensure_third_person_dependencies(scene, entity);
}

void convert_to_first_person(Scene& scene, EntityId entity) {
    auto* player = scene.get_component<PlayerControllerComponent>(entity);
    if (player == nullptr) return;
    player->view_mode = PlayerViewMode::FirstPerson;

    const EntityId orbit_camera = third_person_camera(scene, entity);
    if (orbit_camera != InvalidEntity) scene.destroy_entity(orbit_camera);

    // First Person renders directly from the root game camera.
    mark_player_root_camera(scene, entity);
}

void third_person_description(const char* text) {
    ImGui::PushTextWrapPos();
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
}

bool draw_third_person_player(Scene& scene, EntityId entity) {
    auto* player = scene.get_component<PlayerControllerComponent>(entity);
    if (player == nullptr || player->view_mode != PlayerViewMode::ThirdPerson) return false;

    bool changed = false;
    ImGui::PushID("SeedThirdPersonPlayer");

    if (ImGui::CollapsingHeader("Player — Third Person", ImGuiTreeNodeFlags_DefaultOpen)) {
        third_person_description("A ready-to-play third-person Seed player. Orbit Camera, Inventory, and Character Body are managed as dependencies.");

        changed |= ImGui::Checkbox("Enabled", &player->enabled);
        if (ImGui::DragFloat("Move Speed", &player->move_speed, 0.1f, 0.1f, 100.0f, "%.1f m/s")) {
            player->move_speed = std::max(0.1f, player->move_speed);
            changed = true;
        }
        if (ImGui::DragFloat("Sprint Multiplier", &player->fast_multiplier, 0.05f, 1.0f, 10.0f, "%.2fx")) {
            player->fast_multiplier = std::max(1.0f, player->fast_multiplier);
            changed = true;
        }
        if (ImGui::DragFloat("Look Sensitivity", &player->look_sensitivity, 0.005f, 0.01f, 2.0f, "%.3f")) {
            player->look_sensitivity = std::max(0.01f, player->look_sensitivity);
            changed = true;
        }
        changed |= ImGui::Checkbox("Face Movement Direction", &player->orient_to_movement);

        ImGui::SeparatorText("Interaction");
        third_person_description("The camera aims, but interaction reach is measured from the character body.");
        if (ImGui::DragFloat("Distance", &player->interaction_distance, 0.1f, 0.25f, 50.0f, "%.1f m")) {
            player->interaction_distance = std::max(0.25f, player->interaction_distance);
            changed = true;
        }
        if (ImGui::DragFloat("Aim Radius", &player->interaction_radius, 0.05f, 0.05f, 10.0f, "%.2f m")) {
            player->interaction_radius = std::max(0.05f, player->interaction_radius);
            changed = true;
        }

        ImGui::SeparatorText("Orbit Camera");
        if (ImGui::DragFloat("Camera Distance", &player->camera_distance, 0.05f, 0.25f, 30.0f, "%.2f m")) {
            player->camera_distance = std::max(0.25f, player->camera_distance);
            changed = true;
        }
        if (ImGui::DragFloat("Camera Height", &player->camera_height, 0.03f, 0.0f, 10.0f, "%.2f m")) {
            player->camera_height = std::max(0.0f, player->camera_height);
            changed = true;
        }
        changed |= ImGui::DragFloat("Shoulder Offset", &player->camera_shoulder_offset, 0.03f, -5.0f, 5.0f, "%.2f m");

        if (ImGui::DragFloat("Minimum Pitch", &player->camera_min_pitch, 1.0f, -89.0f, 89.0f, "%.0f deg")) {
            player->camera_min_pitch = std::clamp(player->camera_min_pitch, -89.0f, 89.0f);
            if (player->camera_min_pitch > player->camera_max_pitch) player->camera_max_pitch = player->camera_min_pitch;
            changed = true;
        }
        if (ImGui::DragFloat("Maximum Pitch", &player->camera_max_pitch, 1.0f, -89.0f, 89.0f, "%.0f deg")) {
            player->camera_max_pitch = std::clamp(player->camera_max_pitch, -89.0f, 89.0f);
            if (player->camera_max_pitch < player->camera_min_pitch) player->camera_min_pitch = player->camera_max_pitch;
            changed = true;
        }

        const EntityId camera = third_person_camera(scene, entity);
        if (camera == InvalidEntity) {
            ImGui::TextWrapped("This Third-Person Player is missing its orbit camera.");
            if (ImGui::Button("Fix: Create Third-Person Camera")) {
                ensure_third_person_dependencies(scene, entity);
                changed = true;
            }
        } else {
            ImGui::TextDisabled("Camera rig: %s", scene.entity_name(camera).c_str());
        }

        if (!scene.has_component<InventoryComponent>(entity)) {
            ImGui::TextWrapped("This Player is missing Inventory, so pickups cannot be stored.");
            if (ImGui::Button("Fix: Add Inventory")) {
                scene.add_component<InventoryComponent>(entity);
                changed = true;
            }
        }
        if (!scene.has_component<CharacterBodyComponent>(entity)) {
            ImGui::TextWrapped("This Player is missing Character Physics.");
            if (ImGui::Button("Fix: Add Character Body")) {
                scene.add_component<CharacterBodyComponent>(entity);
                changed = true;
            }
        }

        ImGui::SeparatorText("View Mode");
        if (ImGui::Button("Convert to First Person")) {
            convert_to_first_person(scene, entity);
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove Player Controller")) {
            scene.remove_component<PlayerControllerComponent>(entity);
            changed = true;
        }
    }

    ImGui::PopID();
    return changed;
}

} // namespace

bool draw_gameplay_components(Scene& scene, EntityId entity) {
    if (entity == InvalidEntity || !scene.is_alive(entity)) return false;

    auto* player = scene.get_component<PlayerControllerComponent>(entity);
    const bool third_person = player != nullptr && player->view_mode == PlayerViewMode::ThirdPerson;

    // The legacy authoring panel already handles all non-player gameplay concepts.
    // For Third Person we temporarily hide PlayerController so its historical
    // "Player — First Person" panel does not render, then draw the Seed-native TP panel.
    PlayerControllerComponent third_person_copy{};
    if (third_person) {
        third_person_copy = *player;
        scene.remove_component<PlayerControllerComponent>(entity);
    }

    bool changed = draw_gameplay_components_legacy(scene, entity);

    if (third_person) {
        scene.add_component<PlayerControllerComponent>(entity, third_person_copy);
        changed |= draw_third_person_player(scene, entity);
        return changed;
    }

    player = scene.get_component<PlayerControllerComponent>(entity);
    if (player != nullptr && player->view_mode == PlayerViewMode::FirstPerson) {
        ImGui::PushID("SeedPlayerModeExtension");
        if (ImGui::Button("Convert Player to Third Person")) {
            convert_to_third_person(scene, entity);
            changed = true;
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Creates a Seed-managed orbit camera and changes movement to camera-relative third person.");
        }
        ImGui::PopID();
    }

    return changed;
}

bool draw_add_gameplay_popup(Scene& scene, EntityId entity) {
    bool changed = draw_add_gameplay_popup_legacy(scene, entity);

    if (entity == InvalidEntity || !scene.is_alive(entity)) return changed;

    const bool has_player = scene.has_component<PlayerControllerComponent>(entity);
    ImGui::Separator();
    ImGui::BeginDisabled(has_player);
    if (ImGui::MenuItem(has_player ? "Player — Third Person [Player Added]" : "Player — Third Person")) {
        convert_to_third_person(scene, entity);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Playable third-person Player. Orbit Camera + Inventory + Character Body are automatic.");
    }

    return changed;
}

} // namespace seed::studio
