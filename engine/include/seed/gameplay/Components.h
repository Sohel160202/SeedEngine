#pragma once

#include "seed/math/Math.h"

#include <string>
#include <unordered_map>

namespace seed {

struct TransformComponent {
    Vec3 position{};
    Vec3 rotation_degrees{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct InteractableComponent {
    std::string prompt{"Interact"};
    bool enabled{true};
};

enum class DoorMotion {
    Rotate,
    Slide
};

struct DoorComponent {
    DoorMotion motion{DoorMotion::Rotate};
    float open_amount{90.0f};
    float duration_seconds{0.75f};
    std::string required_item{};
    bool consume_required_item{false};
    bool starts_open{false};
};

struct HealthComponent {
    float maximum{100.0f};
    float current{100.0f};
    bool invulnerable{false};
};

struct InventoryComponent {
    std::unordered_map<std::string, int> items;
};

// Beginner-facing collectible item definition. A Pickup is intentionally data-only;
// GameplayRuntime owns the actual collect/inventory behavior.
struct PickupComponent {
    std::string item_id{"Item"};
    std::string display_name{"Item"};
    int quantity{1};
    bool destroy_on_pickup{true};
};

enum class PlayerViewMode {
    FirstPerson,
    ThirdPerson
};

// Seed-native playable controller shared by first- and third-person player presets.
// The beginner sees one Player concept while Seed owns the camera/movement details.
struct PlayerControllerComponent {
    PlayerViewMode view_mode{PlayerViewMode::FirstPerson};

    float move_speed{4.0f};
    float fast_multiplier{2.5f};
    float look_sensitivity{0.12f};
    float interaction_distance{4.0f};
    float interaction_radius{1.25f};

    // Third-person camera rig settings. They are ignored in First Person mode.
    float camera_distance{4.5f};
    float camera_height{1.55f};
    float camera_shoulder_offset{0.0f};
    float camera_min_pitch{-60.0f};
    float camera_max_pitch{70.0f};
    bool orient_to_movement{true};

    bool enabled{true};
};

} // namespace seed
