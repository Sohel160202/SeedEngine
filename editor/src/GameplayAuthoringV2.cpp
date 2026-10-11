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
    // Studio's existing authored-player discovery identifies Player + Camera on
    // one entity. The root camera remains a lightweight marker; Seed Runtime and
    // RenderSystem deliberately route Third Person through the child camera.
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
    if (!scene.has_component<TransformComponent>(player)) {
        scene.add_component<TransformComponent>(player);
    }
    if (!scene.has_component<InventoryComponent>(player)) {
        scene.add_component<InventoryComponent>(player);
    }
    if (!scene.has_component<CharacterBodyComponent>(player)) {
        scene.add_component<CharacterBodyComponent>(player);
    }

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

    // First Person renders directly from the root game camera.
    mark_player_root_camera(scene, entity);
}

bool draw_third_person_settings(Scene& scene, EntityId entity) {
    auto* player = scene.get_component<PlayerControllerComponent>(entity);
    if (player == nullptr || player->view_mode != PlayerViewMode::ThirdPerson) return false;

    bool changed = false;
    ImGui::PushID("SeedThirdPersonExtension");
    if (ImGui::CollapsingHeader("Third-Person Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushTextWrapPos();
        ImGui::TextDisabled("Orbit camera managed by Seed. Movement is camera-relative and the character can face travel direction automatically.");
        ImGui::PopTextWrapPos();

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
        changed |= ImGui::Checkbox("Face Movement Direction", &player->orient_to_movement);

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

        ImGui::SeparatorText("View Mode");
        if (ImGui::Button("Convert to First Person")) {
            convert_to_first_person(scene, entity);
            changed = true;
        }
    }
    ImGui::PopID();
    return changed;
}

} // namespace

bool draw_gameplay_components(Scene& scene, EntityId entity) {
    bool changed = draw_gameplay_components_legacy(scene, entity);

    auto* player = scene.get_component<PlayerControllerComponent>(entity);
    if (player != nullptr) {
        ImGui::PushID("SeedPlayerModeExtension");
        if (player->view_mode == PlayerViewMode::FirstPerson) {
            if (ImGui::Button("Convert Player to Third Person")) {
                convert_to_third_person(scene, entity);
                changed = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Creates a Seed-managed orbit camera and changes movement to camera-relative third person.");
            }
        }
        ImGui::PopID();
    }

    changed |= draw_third_person_settings(scene, entity);
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
