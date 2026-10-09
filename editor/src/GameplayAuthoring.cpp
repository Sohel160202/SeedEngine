#include "GameplayAuthoring.h"

#include "seed/gameplay/Components.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

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

void make_primary_game_camera(Scene& scene, EntityId entity) {
    scene.for_each<CameraComponent>([&](EntityId candidate, CameraComponent& camera) {
        if (!camera.editor_only) {
            camera.primary = candidate == entity;
        }
    });
}

} // namespace

bool draw_gameplay_components(Scene& scene, EntityId entity) {
    if (entity == InvalidEntity || !scene.is_alive(entity)) {
        return false;
    }

    bool changed = false;

    if (auto* player = scene.get_component<PlayerControllerComponent>(entity)) {
        ImGui::PushID("SeedPlayer");
        if (ImGui::CollapsingHeader("Player — First Person", ImGuiTreeNodeFlags_DefaultOpen)) {
            component_description("A ready-to-play first-person Seed player. Camera and Inventory are managed as dependencies.");

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

            ImGui::SeparatorText("Interaction");
            if (ImGui::DragFloat("Distance", &player->interaction_distance, 0.1f, 0.25f, 50.0f, "%.1f m")) {
                player->interaction_distance = std::max(0.25f, player->interaction_distance);
                changed = true;
            }
            if (ImGui::DragFloat("Aim Radius", &player->interaction_radius, 0.05f, 0.05f, 10.0f, "%.2f m")) {
                player->interaction_radius = std::max(0.05f, player->interaction_radius);
                changed = true;
            }

            if (!scene.has_component<CameraComponent>(entity)) {
                ImGui::TextWrapped("This Player is missing its game Camera.");
                if (ImGui::Button("Fix: Add First-Person Camera")) {
                    CameraComponent camera;
                    camera.primary = true;
                    camera.enabled = true;
                    camera.editor_only = false;
                    camera.field_of_view_degrees = 65.0f;
                    scene.add_component<CameraComponent>(entity, camera);
                    make_primary_game_camera(scene, entity);
                    changed = true;
                }
            }

            if (!scene.has_component<InventoryComponent>(entity)) {
                ImGui::TextWrapped("This Player is missing Inventory, so pickups cannot be stored.");
                if (ImGui::Button("Fix: Add Inventory")) {
                    scene.add_component<InventoryComponent>(entity);
                    changed = true;
                }
            }

            if (remove_component_button("Remove Player Controller")) {
                scene.remove_component<PlayerControllerComponent>(entity);
                changed = true;
            }
        }
        ImGui::PopID();
    }

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

    if (auto* pickup = scene.get_component<PickupComponent>(entity)) {
        ImGui::PushID("SeedPickup");
        if (ImGui::CollapsingHeader("Pickup", ImGuiTreeNodeFlags_DefaultOpen)) {
            component_description("Turns this object into an item the player can collect into Inventory.");

            changed |= edit_string("Item ID", pickup->item_id);

            const std::string old_display_name = pickup->display_name;
            if (edit_string("Display Name", pickup->display_name)) {
                if (auto* interactable = scene.get_component<InteractableComponent>(entity)) {
                    const std::string old_default_prompt = "Pick Up " + old_display_name;
                    if (interactable->prompt == old_default_prompt) {
                        interactable->prompt = "Pick Up " + pickup->display_name;
                    }
                }
                changed = true;
            }

            if (ImGui::DragInt("Quantity", &pickup->quantity, 1.0f, 1, 9999)) {
                pickup->quantity = std::max(1, pickup->quantity);
                changed = true;
            }
            changed |= ImGui::Checkbox("Destroy on Pickup", &pickup->destroy_on_pickup);

            if (!scene.has_component<InteractableComponent>(entity)) {
                ImGui::Spacing();
                ImGui::TextWrapped("This pickup is missing Interactable, so the player cannot collect it.");
                if (ImGui::Button("Fix: Add Interactable")) {
                    scene.add_component<InteractableComponent>(entity, "Pick Up " + pickup->display_name, true);
                    changed = true;
                }
            }

            if (remove_component_button("Remove Pickup")) {
                scene.remove_component<PickupComponent>(entity);
                changed = true;
            }
        }
        ImGui::PopID();
    }

    if (auto* inventory = scene.get_component<InventoryComponent>(entity)) {
        ImGui::PushID("SeedInventory");
        if (ImGui::CollapsingHeader("Inventory", ImGuiTreeNodeFlags_DefaultOpen)) {
            component_description("Stores item IDs and quantities. Player presets use this automatically.");

            std::vector<std::string> item_ids;
            item_ids.reserve(inventory->items.size());
            for (const auto& [item_id, quantity] : inventory->items) {
                (void)quantity;
                item_ids.push_back(item_id);
            }
            std::sort(item_ids.begin(), item_ids.end());

            std::string remove_item;
            for (const auto& item_id : item_ids) {
                auto it = inventory->items.find(item_id);
                if (it == inventory->items.end()) {
                    continue;
                }

                ImGui::PushID(item_id.c_str());
                ImGui::TextUnformatted(item_id.c_str());
                ImGui::SameLine(150.0f);
                ImGui::SetNextItemWidth(75.0f);
                if (ImGui::DragInt("##Quantity", &it->second, 1.0f, 0, 9999)) {
                    it->second = std::max(0, it->second);
                    changed = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove")) {
                    remove_item = item_id;
                }
                ImGui::PopID();
            }

            if (!remove_item.empty()) {
                inventory->items.erase(remove_item);
                changed = true;
            }

            static std::array<char, 128> new_item_id{};
            static int new_item_quantity = 1;
            ImGui::SeparatorText("Add Starter Item");
            ImGui::SetNextItemWidth(145.0f);
            ImGui::InputText("Item ID##Inventory", new_item_id.data(), new_item_id.size());
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60.0f);
            ImGui::DragInt("Qty##Inventory", &new_item_quantity, 1.0f, 1, 9999);
            ImGui::SameLine();
            if (ImGui::Button("Add##Inventory") && new_item_id[0] != '\0') {
                inventory->items[std::string{new_item_id.data()}] += std::max(1, new_item_quantity);
                new_item_id.fill('\0');
                new_item_quantity = 1;
                changed = true;
            }

            if (remove_component_button("Remove Inventory")) {
                scene.remove_component<InventoryComponent>(entity);
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

    const bool has_player = scene.has_component<PlayerControllerComponent>(entity);
    const bool has_interactable = scene.has_component<InteractableComponent>(entity);
    const bool has_pickup = scene.has_component<PickupComponent>(entity);
    const bool has_inventory = scene.has_component<InventoryComponent>(entity);
    const bool has_health = scene.has_component<HealthComponent>(entity);
    const bool has_door = scene.has_component<DoorComponent>(entity);

    ImGui::BeginDisabled(has_player);
    if (ImGui::MenuItem(has_player ? "Player — First Person [Added]" : "Player — First Person")) {
        if (!scene.has_component<TransformComponent>(entity)) {
            scene.add_component<TransformComponent>(entity);
        }

        if (!scene.has_component<CameraComponent>(entity)) {
            CameraComponent camera;
            camera.primary = true;
            camera.enabled = true;
            camera.editor_only = false;
            camera.field_of_view_degrees = 65.0f;
            scene.add_component<CameraComponent>(entity, camera);
        } else {
            auto* camera = scene.get_component<CameraComponent>(entity);
            camera->primary = true;
            camera->enabled = true;
            camera->editor_only = false;
        }
        make_primary_game_camera(scene, entity);

        if (!scene.has_component<InventoryComponent>(entity)) {
            scene.add_component<InventoryComponent>(entity);
        }
        scene.add_component<PlayerControllerComponent>(entity);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Create a playable first-person entity. Camera + Inventory are added automatically.");
    }

    ImGui::Separator();

    ImGui::BeginDisabled(has_interactable);
    if (ImGui::MenuItem(has_interactable ? "Interactable   [Added]" : "Interactable")) {
        scene.add_component<InteractableComponent>(entity, "Interact", true);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Give this object an interaction prompt.");
    }

    ImGui::BeginDisabled(has_pickup);
    if (ImGui::MenuItem(has_pickup ? "Pickup         [Added]" : "Pickup")) {
        if (!scene.has_component<TransformComponent>(entity)) {
            scene.add_component<TransformComponent>(entity);
        }
        if (!scene.has_component<InteractableComponent>(entity)) {
            scene.add_component<InteractableComponent>(entity, "Pick Up Item", true);
        }
        scene.add_component<PickupComponent>(entity);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Make this object collectible. Transform + Interactable are added automatically.");
    }

    ImGui::BeginDisabled(has_inventory);
    if (ImGui::MenuItem(has_inventory ? "Inventory      [Added]" : "Inventory")) {
        scene.add_component<InventoryComponent>(entity);
        changed = true;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Give this entity a persistent collection of item IDs and quantities.");
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
    ImGui::TextDisabled("Next: Enemy, Dialogue, Quest");
    return changed;
}

} // namespace seed::studio
