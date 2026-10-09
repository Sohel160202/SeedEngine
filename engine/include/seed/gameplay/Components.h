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

} // namespace seed
