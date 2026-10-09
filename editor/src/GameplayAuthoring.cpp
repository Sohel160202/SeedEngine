#include "GameplayAuthoring.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

namespace seed::studio {
namespace {

template <std::size_t Size>
void copy_text(std::array<char, Size>& buffer, const std::string& value) {
    std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
}

bool edit_string(const char* label, std::string& value, float width = -1.0f) {
    std::array<char, 256> buffer{};
    copy_text(buffer, value);
    if (width > 0.0f) {
        ImGui::SetNextItemWidth(width);
    }
    if (!ImGui::InputText(label, buffer.data(), buffer.size())) {
        return false;
    }
    value = buffer.data();
    return true;
}

void component_description(const char* text) {
    ImGui::PushTextWrapPos();
    ImGui::TextDisabled("%s", text);
    ImGui::PopTextWrapPos();
}

bool remove_component_button(const char* label) {
    ImGui::Spacing();
    return ImGui::SmallButton(label);
}

} // namespace

bool draw_gameplay_components(Scene& scene, EntityId entity) {
    if (entity == InvalidEntity || !scene.is_alive(entity)) {
        return false;
    }

    bool changed = false;

    if (auto* interactable = scene.get_component<InteractableComponent>(entity)) {
        ImGui::PushID("SeedInteractable");
        if (ImGui::CollapsingHeader("Interactable", ImGuiTreeNodeFlags_DefaultOpen)) {
            component_description("Lets the player target this object and receive an interaction prompt.");
            changed |= ImGui::Checkbox("Enabled", &interactable->enabled);
            changed |= edit_string("Prompt", interactable->prompt);

            if (remove_component_button("Remove Interactable")) {
                scene.remove_component<InteractableComponent>(entity);
                changed = true;
            }
        }
        ImGui::PopID();
    }

    if (auto* health = scene.get_component<HealthComponent>(entity)) {
        ImGui::PushID("SeedHealth");
        if (ImGui::CollapsingHeader("Health", ImGuiTreeNodeFlags_DefaultOpen)) {
            component_description("Makes this object capable of receiving damage and tracking health.");

            const float old_maximum = health->maximum;
            if (ImGui::DragFloat("Maximum", &health->maximum, 1.0f, 1.0f, 100000.0f, "%.0f")) {
                health->maximum = std::max(1.0f, health->maximum);
                if (health->current > health->maximum || health->current == old_maximum) {
                    health->current = health->maximum;
                }
                changed = true;
            }

            if (ImGui::DragFloat("Current", &health->current, 1.0f, 0.0f, health->maximum, "%.0f")) {
                health->current = std::clamp(health->current, 0.0f, health->maximum);
                changed = true;
            }

            changed |= ImGui::Checkbox("Invulnerable", &health->invulnerable);
            if (ImGui::Button("Reset to Maximum")) {
                health->current = health->maximum;
                changed = true;
            }

            if (remove_component_button("Remove Health")) {
                scene.remove_component<HealthComponent>(entity);
                changed = true;
            }
        }
        ImGui::PopID();
    }

    if (auto* door = scene.get_component<DoorComponent>(entity)) {
        ImGui::PushID("SeedDoor");
        if (ImGui::CollapsingHeader("Door / Lock", ImGuiTreeNodeFlags_DefaultOpen)) {
            component_description("Turns this object into a configurable door. Seed automatically adds Interactable when a door is created.");

            int motion_index = door->motion == DoorMotion::Slide ? 1 : 0;
            if (ImGui::Combo("Opening", &motion_index, "Rotate\0Slide\0")) {
                door->motion = motion_index == 1 ? DoorMotion::Slide : DoorMotion::Rotate;
                door->open_amount = door->motion == DoorMotion::Slide ? 1.5f : 90.0f;
                changed = true;
            }

            if (door->motion == DoorMotion::Rotate) {
                changed |= ImGui::DragFloat("Open Angle", &door->open_amount, 1.0f, -360.0f, 360.0f, "%.0f deg");
            } else {
                changed |= ImGui::DragFloat("Open Distance", &door->open_amount, 0.05f, -100.0f, 100.0f, "%.2f m");
            }

            if (ImGui::DragFloat("Duration", &door->duration_seconds, 0.05f, 0.05f, 60.0f, "%.2f sec")) {
                door->duration_seconds = std::max(0.05f, door->duration_seconds);
                changed = true;
            }

            ImGui::SeparatorText("Lock");
            component_description("Leave Required Item empty for an unlocked door.");
            changed |= edit_string("Required Item", door->required_item);
            changed |= ImGui::Checkbox("Consume Item", &door->consume_required_item);
            changed |= ImGui::Checkbox("Starts Open", &door->starts_open);

            if (!scene.has_component<InteractableComponent>(entity)) {
                ImGui::Spacing();
                ImGui::TextWrapped("This door is missing Interactable, so a player cannot activate it.");
                if (ImGui::Button("Fix: Add Interactable")) {
                    scene.add_component<InteractableComponent>(entity, "Open", true);
                    changed = true;
                }
            }

            if (!scene.has_component<TransformComponent>(entity)) {
                ImGui::Spacing();
                ImGui::TextWrapped("This door is missing Transform, so it cannot move.");
                if (ImGui::Button("Fix: Add Transform")) {
                    scene.add_component<TransformComponent>(entity);
                    changed = true;
                }
            }

            if (remove_component_button("Remove Door / Lock")) {
                scene.remove_component<DoorComponent>(entity);
                changed = true;
            }
        }
        ImGui::PopID();
    }

    return changed;
}

bool draw_add_gameplay_popup(Scene& scene, EntityId entity) {
    if (entity == InvalidEntity || !scene.is_alive(entity)) {
        ImGui::TextDisabled("Select an entity first.");
        return false;
    }

    bool changed = false;
    ImGui::TextUnformatted("What should this object do?");
    ImGui::TextDisabled("Seed adds the underlying components for you.");
    ImGui::Separator();

    const bool has_interactable = scene.has_component<InteractableComponent>(entity);
    const bool has_health = scene.has_component<HealthComponent>(entity);
    const bool has_door = scene.has_component<DoorComponent>(entity);

    ImGui::BeginDisabled(has_interactable);
    if (ImGui::MenuItem(has_interactable ? "Interactable   [Added]" : "Interactable")) {
        scene.add_component<InteractableComponent>(entity, "Interact", true);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Give this object an interaction prompt.");
    }

    ImGui::BeginDisabled(has_health);
    if (ImGui::MenuItem(has_health ? "Health         [Added]" : "Health")) {
        scene.add_component<HealthComponent>(entity, 100.0f, 100.0f, false);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Make this object damageable and give it health.");
    }

    ImGui::BeginDisabled(has_door);
    if (ImGui::MenuItem(has_door ? "Door / Lock    [Added]" : "Door / Lock")) {
        if (!scene.has_component<TransformComponent>(entity)) {
            scene.add_component<TransformComponent>(entity);
        }
        if (!scene.has_component<InteractableComponent>(entity)) {
            scene.add_component<InteractableComponent>(entity, "Open", true);
        }
        scene.add_component<DoorComponent>(entity);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Add a rotating/sliding door. Interactable is added automatically.");
    }

    ImGui::Separator();
    ImGui::TextDisabled("Next: Pickup, Inventory, Player, Enemy");
    return changed;
}

} // namespace seed::studio
