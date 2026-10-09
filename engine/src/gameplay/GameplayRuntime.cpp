#include "seed/gameplay/GameplayRuntime.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace seed {
namespace {

void set_key_state(
    KeyCode key,
    bool down,
    bool& forward,
    bool& backward,
    bool& left,
    bool& right,
    bool& move_down,
    bool& move_up,
    bool& fast
) {
    switch (key) {
    case KeyCode::W: forward = down; break;
    case KeyCode::S: backward = down; break;
    case KeyCode::A: left = down; break;
    case KeyCode::D: right = down; break;
    case KeyCode::Q: move_down = down; break;
    case KeyCode::Space: move_up = down; break;
    case KeyCode::LeftShift:
    case KeyCode::RightShift: fast = down; break;
    default: break;
    }
}

float maximum_scale(const TransformComponent& transform) {
    return std::max({
        std::fabs(transform.scale.x),
        std::fabs(transform.scale.y),
        std::fabs(transform.scale.z),
    });
}

} // namespace

bool GameplayRuntime::start(Scene& scene, EntityId player_entity) {
    stop();

    if (player_entity == InvalidEntity || !scene.is_alive(player_entity)) {
        m_status_message = "Play Mode could not find a valid player.";
        return false;
    }

    if (!scene.has_component<TransformComponent>(player_entity)) {
        scene.add_component<TransformComponent>(player_entity);
    }
    if (!scene.has_component<PlayerControllerComponent>(player_entity)) {
        scene.add_component<PlayerControllerComponent>(player_entity);
    }
    if (!scene.has_component<InventoryComponent>(player_entity)) {
        scene.add_component<InventoryComponent>(player_entity);
    }

    m_player_entity = player_entity;
    m_active = true;
    m_status_message = "Play Mode running.";

    scene.for_each<TransformComponent, DoorComponent>(
        [&](EntityId entity, TransformComponent& transform, DoorComponent& door) {
            DoorRuntimeState state;
            state.closed_position = transform.position;
            state.closed_rotation = transform.rotation_degrees;
            state.progress = door.starts_open ? 1.0f : 0.0f;
            state.target_open = door.starts_open;
            m_doors.emplace(entity, state);
            apply_door_transform(scene, entity, state);
        }
    );

    refresh_interaction_target(scene);
    return true;
}

void GameplayRuntime::stop() {
    m_active = false;
    m_player_entity = InvalidEntity;
    m_interaction_target = InvalidEntity;
    m_forward = false;
    m_backward = false;
    m_left = false;
    m_right = false;
    m_down = false;
    m_up = false;
    m_fast = false;
    m_looking = false;
    m_has_mouse_position = false;
    m_interact_requested = false;
    m_pending_look_x = 0.0f;
    m_pending_look_y = 0.0f;
    m_doors.clear();
    m_interaction_prompt.clear();
    m_status_message.clear();
}

void GameplayRuntime::handle_event(const PlatformEvent& event) {
    if (!m_active) {
        return;
    }

    if (event.type == PlatformEventType::Key) {
        const bool down = event.button_state != ButtonState::Released;
        set_key_state(
            event.key,
            down,
            m_forward,
            m_backward,
            m_left,
            m_right,
            m_down,
            m_up,
            m_fast
        );

        if (event.key == KeyCode::E && event.button_state == ButtonState::Pressed) {
            m_interact_requested = true;
        }
    }

    if (event.type == PlatformEventType::MouseButton &&
        event.mouse_button == MouseButton::Right) {
        m_looking = event.button_state != ButtonState::Released;
        m_has_mouse_position = false;
    }

    if (event.type == PlatformEventType::MouseMove && m_looking) {
        if (m_has_mouse_position) {
            m_pending_look_x += event.x - m_last_mouse_x;
            m_pending_look_y += event.y - m_last_mouse_y;
        }
        m_last_mouse_x = event.x;
        m_last_mouse_y = event.y;
        m_has_mouse_position = true;
    }

    if (event.type == PlatformEventType::WindowFocusChanged && !event.focused) {
        m_forward = false;
        m_backward = false;
        m_left = false;
        m_right = false;
        m_down = false;
        m_up = false;
        m_fast = false;
        m_looking = false;
        m_has_mouse_position = false;
    }
}

void GameplayRuntime::update(Scene& scene, double delta_seconds) {
    if (!m_active || !scene.is_alive(m_player_entity)) {
        return;
    }

    update_player(scene, delta_seconds);
    update_doors(scene, delta_seconds);
    refresh_interaction_target(scene);

    if (m_interact_requested) {
        perform_interaction(scene);
        m_interact_requested = false;
        refresh_interaction_target(scene);
    }
}

void GameplayRuntime::update_player(Scene& scene, double delta_seconds) {
    auto* transform = scene.get_component<TransformComponent>(m_player_entity);
    auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (transform == nullptr || controller == nullptr || !controller->enabled) {
        return;
    }

    transform->rotation_degrees.y += m_pending_look_x * controller->look_sensitivity;
    transform->rotation_degrees.x -= m_pending_look_y * controller->look_sensitivity;
    transform->rotation_degrees.x = std::clamp(transform->rotation_degrees.x, -89.0f, 89.0f);
    m_pending_look_x = 0.0f;
    m_pending_look_y = 0.0f;

    const float speed = controller->move_speed * (m_fast ? controller->fast_multiplier : 1.0f);
    const float distance = speed * static_cast<float>(delta_seconds);
    const Vec3 forward = forward_from_euler(transform->rotation_degrees);
    const Vec3 right = right_from_euler(transform->rotation_degrees);
    const Vec3 world_up{0.0f, 1.0f, 0.0f};

    if (m_forward) transform->position += forward * distance;
    if (m_backward) transform->position += forward * -distance;
    if (m_right) transform->position += right * distance;
    if (m_left) transform->position += right * -distance;
    if (m_up) transform->position += world_up * distance;
    if (m_down) transform->position += world_up * -distance;
}

void GameplayRuntime::refresh_interaction_target(Scene& scene) {
    m_interaction_target = InvalidEntity;
    m_interaction_prompt.clear();

    const auto* player_transform = scene.get_component<TransformComponent>(m_player_entity);
    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (player_transform == nullptr || controller == nullptr || !controller->enabled) {
        return;
    }

    const Vec3 origin = player_transform->position;
    const Vec3 forward = forward_from_euler(player_transform->rotation_degrees);
    float best_projection = std::numeric_limits<float>::max();

    scene.for_each<TransformComponent, InteractableComponent>(
        [&](EntityId entity, TransformComponent& transform, InteractableComponent& interactable) {
            if (entity == m_player_entity || !interactable.enabled) {
                return;
            }

            const Vec3 to_target = transform.position - origin;
            const float projection = dot(to_target, forward);
            if (projection <= 0.0f || projection > controller->interaction_distance) {
                return;
            }

            const Vec3 closest_point = origin + forward * projection;
            const float perpendicular_distance = length(transform.position - closest_point);
            const float entity_radius = std::max(0.4f, maximum_scale(transform) * 0.65f);
            const float allowed_radius = controller->interaction_radius + entity_radius;
            if (perpendicular_distance > allowed_radius || projection >= best_projection) {
                return;
            }

            best_projection = projection;
            m_interaction_target = entity;
        }
    );

    if (m_interaction_target == InvalidEntity) {
        return;
    }

    const auto* interactable = scene.get_component<InteractableComponent>(m_interaction_target);
    if (interactable == nullptr) {
        return;
    }

    m_interaction_prompt = "[E] " + interactable->prompt;

    const auto* pickup = scene.get_component<PickupComponent>(m_interaction_target);
    if (pickup != nullptr && pickup->quantity > 1) {
        m_interaction_prompt += "  x" + std::to_string(pickup->quantity);
    }

    const auto* door = scene.get_component<DoorComponent>(m_interaction_target);
    if (door != nullptr && !door->required_item.empty() && !player_has_item(scene, door->required_item)) {
        m_interaction_prompt += "  (Requires " + door->required_item + ")";
    }
}

void GameplayRuntime::perform_interaction(Scene& scene) {
    if (m_interaction_target == InvalidEntity || !scene.is_alive(m_interaction_target)) {
        m_status_message = "Nothing to interact with.";
        return;
    }

    auto* interactable = scene.get_component<InteractableComponent>(m_interaction_target);
    if (interactable == nullptr || !interactable->enabled) {
        m_status_message = "This object is not interactable.";
        return;
    }

    if (auto* pickup = scene.get_component<PickupComponent>(m_interaction_target)) {
        const std::string item_id = pickup->item_id;
        const std::string display_name = pickup->display_name.empty() ? item_id : pickup->display_name;
        const int quantity = std::max(1, pickup->quantity);
        const bool destroy_on_pickup = pickup->destroy_on_pickup;
        const EntityId pickup_entity = m_interaction_target;

        if (item_id.empty()) {
            m_status_message = "Pickup needs an Item ID.";
            return;
        }
        if (!add_player_item(scene, item_id, quantity)) {
            m_status_message = "Could not add " + display_name + " to Inventory.";
            return;
        }

        m_status_message = "Picked up " + display_name;
        if (quantity > 1) {
            m_status_message += " x" + std::to_string(quantity);
        }
        m_status_message += ".";

        if (destroy_on_pickup) {
            m_doors.erase(pickup_entity);
            scene.destroy_entity(pickup_entity);
        } else {
            interactable = scene.get_component<InteractableComponent>(pickup_entity);
            if (interactable != nullptr) {
                interactable->enabled = false;
            }
        }

        m_interaction_target = InvalidEntity;
        return;
    }

    auto* door = scene.get_component<DoorComponent>(m_interaction_target);
    if (door == nullptr) {
        m_status_message = interactable->prompt + " triggered.";
        return;
    }

    auto state_it = m_doors.find(m_interaction_target);
    if (state_it == m_doors.end()) {
        auto* transform = scene.get_component<TransformComponent>(m_interaction_target);
        if (transform == nullptr) {
            m_status_message = "Door cannot move because it has no Transform.";
            return;
        }
        DoorRuntimeState state;
        state.closed_position = transform->position;
        state.closed_rotation = transform->rotation_degrees;
        state.progress = door->starts_open ? 1.0f : 0.0f;
        state.target_open = door->starts_open;
        state_it = m_doors.emplace(m_interaction_target, state).first;
    }

    auto& state = state_it->second;
    const bool opening = !state.target_open;

    if (opening && !door->required_item.empty()) {
        if (!player_has_item(scene, door->required_item)) {
            m_status_message = "Locked — requires " + door->required_item + ".";
            return;
        }
        if (door->consume_required_item && !consume_player_item(scene, door->required_item)) {
            m_status_message = "Locked — requires " + door->required_item + ".";
            return;
        }
    }

    state.target_open = opening;
    m_status_message = opening ? "Opening " : "Closing ";
    m_status_message += scene.entity_name(m_interaction_target) + ".";
}

void GameplayRuntime::update_doors(Scene& scene, double delta_seconds) {
    for (auto& [entity, state] : m_doors) {
        if (!scene.is_alive(entity)) {
            continue;
        }

        const auto* door = scene.get_component<DoorComponent>(entity);
        if (door == nullptr) {
            continue;
        }

        const float duration = std::max(door->duration_seconds, 0.05f);
        const float step = static_cast<float>(delta_seconds) / duration;
        if (state.target_open) {
            state.progress = std::min(1.0f, state.progress + step);
        } else {
            state.progress = std::max(0.0f, state.progress - step);
        }
        apply_door_transform(scene, entity, state);
    }
}

void GameplayRuntime::apply_door_transform(
    Scene& scene,
    EntityId entity,
    const DoorRuntimeState& state
) {
    auto* transform = scene.get_component<TransformComponent>(entity);
    const auto* door = scene.get_component<DoorComponent>(entity);
    if (transform == nullptr || door == nullptr) {
        return;
    }

    transform->position = state.closed_position;
    transform->rotation_degrees = state.closed_rotation;

    if (door->motion == DoorMotion::Rotate) {
        transform->rotation_degrees.y += door->open_amount * state.progress;
    } else {
        const Vec3 slide_direction = right_from_euler(state.closed_rotation);
        transform->position += slide_direction * (door->open_amount * state.progress);
    }
}

bool GameplayRuntime::player_has_item(const Scene& scene, const std::string& item_id) const {
    if (item_id.empty()) {
        return true;
    }

    const auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
    if (inventory == nullptr) {
        return false;
    }

    const auto it = inventory->items.find(item_id);
    return it != inventory->items.end() && it->second > 0;
}

bool GameplayRuntime::add_player_item(Scene& scene, const std::string& item_id, int quantity) {
    if (item_id.empty() || quantity <= 0 || !scene.is_alive(m_player_entity)) {
        return false;
    }

    auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
    if (inventory == nullptr) {
        inventory = &scene.add_component<InventoryComponent>(m_player_entity);
    }

    inventory->items[item_id] += quantity;
    return true;
}

bool GameplayRuntime::consume_player_item(Scene& scene, const std::string& item_id) {
    auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
    if (inventory == nullptr) {
        return false;
    }

    const auto it = inventory->items.find(item_id);
    if (it == inventory->items.end() || it->second <= 0) {
        return false;
    }

    --it->second;
    if (it->second <= 0) {
        inventory->items.erase(it);
    }
    return true;
}

} // namespace seed
