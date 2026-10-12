#include "seed/gameplay/GameplayRuntime.h"

#include "seed/animation/AnimationComponents.h"

#include "seed/gameplay/GameplayEvents.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/CombatSystem.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Hierarchy.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace seed {
namespace {

void set_action_state(
    InputAction action,
    bool down,
    bool& forward,
    bool& backward,
    bool& left,
    bool& right,
    bool& move_down,
    bool& move_up,
    bool& fast
) {
    switch (action) {
    case InputAction::MoveForward: forward = down; break;
    case InputAction::MoveBackward: backward = down; break;
    case InputAction::MoveLeft: left = down; break;
    case InputAction::MoveRight: right = down; break;
    case InputAction::MoveDown: move_down = down; break;
    case InputAction::MoveUp: move_up = down; break;
    case InputAction::Sprint: fast = down; break;
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

float yaw_from_direction(const Vec3& direction) {
    return degrees(std::atan2(direction.x, -direction.z));
}

void publish_locomotion(
    Scene& scene,
    EntityId entity,
    float speed,
    bool grounded,
    float vertical_velocity,
    bool sprinting
) {
    auto* animation = scene.get_component<AnimationPlayerComponent>(entity);
    if (animation == nullptr || !animation->locomotion_enabled) return;
    animation->locomotion_speed = speed;
    animation->locomotion_grounded = grounded;
    animation->locomotion_vertical_velocity = vertical_velocity;
    animation->locomotion_sprinting = sprinting;
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
    if (!scene.has_component<AttackComponent>(player_entity)) {
        scene.add_component<AttackComponent>(player_entity);
    }
    if (const auto* spawn = scene.get_component<TransformComponent>(player_entity)) {
        m_spawn_position = spawn->position;
        m_spawn_rotation = spawn->rotation_degrees;
    }
    if (!scene.has_component<PlayerControllerComponent>(player_entity)) {
        scene.add_component<PlayerControllerComponent>(player_entity);
    }
    if (!scene.has_component<InventoryComponent>(player_entity)) {
        scene.add_component<InventoryComponent>(player_entity);
    }
    if (!scene.has_component<AnimationPlayerComponent>(player_entity)) {
        AnimationPlayerComponent animation;
        animation.locomotion_enabled = true;
        scene.add_component<AnimationPlayerComponent>(player_entity, std::move(animation));
    }

    m_player_entity = player_entity;
    m_camera_entity = resolve_player_camera(scene);
    m_active = true;

    const auto* controller = scene.get_component<PlayerControllerComponent>(player_entity);
    if (controller != nullptr && controller->view_mode == PlayerViewMode::ThirdPerson) {
        if (m_camera_entity != InvalidEntity) {
            const TransformComponent camera_world = world_transform(scene, m_camera_entity);
            m_third_person_yaw = camera_world.rotation_degrees.y;
            const float min_pitch = std::min(controller->camera_min_pitch, controller->camera_max_pitch);
            const float max_pitch = std::max(controller->camera_min_pitch, controller->camera_max_pitch);
            m_third_person_pitch = std::clamp(camera_world.rotation_degrees.x, min_pitch, max_pitch);
            update_third_person_camera(scene);
            m_status_message = "Third-Person Play Mode running.";
        } else {
            m_status_message = "Third-Person Player is missing its game Camera.";
        }
    } else {
        m_status_message = "First-Person Play Mode running.";
    }

    if (scene.has_component<CharacterBodyComponent>(player_entity)) {
        PhysicsSystem::initialize_character(scene, player_entity, m_character_state);
    }

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
    m_camera_entity = InvalidEntity;
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
    m_jump_requested = false;
    m_attack_requested = false;
    m_pending_look_x = 0.0f;
    m_pending_look_y = 0.0f;
    m_third_person_yaw = 0.0f;
    m_third_person_pitch = -12.0f;
    m_footstep_elapsed = 0.0f;
    m_respawn_elapsed = 0.0f;
    m_spawn_position = {};
    m_spawn_rotation = {};
    m_character_state = {};
    m_doors.clear();
    m_enemy_attack_elapsed.clear();
    m_victory_announced = false;
    m_dialogue_entity = InvalidEntity;
    m_dialogue_line_index = 0;
    m_dialogue_active = false;
    m_dialogue_choice_requested = -1;
    m_interaction_prompt.clear();
    m_status_message.clear();
}

void GameplayRuntime::handle_event(const PlatformEvent& event) {
    if (!m_active) {
        return;
    }

    if (event.type == PlatformEventType::Key) {
        const bool down = event.button_state != ButtonState::Released;
        const auto actions = m_input_map.actions_for(event.key);
        for (const InputAction action : actions) {
            set_action_state(
                action,
                down,
                m_forward,
                m_backward,
                m_left,
                m_right,
                m_down,
                m_up,
                m_fast
            );

            if (action == InputAction::Interact && event.button_state == ButtonState::Pressed) {
                m_interact_requested = true;
            }
            if (action == InputAction::Attack && event.button_state == ButtonState::Pressed) {
                m_attack_requested = true;
            }
            if (event.button_state == ButtonState::Pressed) {
                switch (action) {
                case InputAction::DialogueChoice1: m_dialogue_choice_requested = 0; break;
                case InputAction::DialogueChoice2: m_dialogue_choice_requested = 1; break;
                case InputAction::DialogueChoice3: m_dialogue_choice_requested = 2; break;
                default: break;
                }
            }
            if (action == InputAction::Jump && event.button_state == ButtonState::Pressed) {
                m_jump_requested = true;
            }
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
        m_jump_requested = false;
        m_dialogue_choice_requested = -1;
    }
}

void GameplayRuntime::update(Scene& scene, double delta_seconds, GameplayEventBus* event_bus) {
    if (!m_active || !scene.is_alive(m_player_entity)) {
        return;
    }

    if (auto* health = scene.get_component<HealthComponent>(m_player_entity);
        health != nullptr && health->current <= 0.0f) {
        m_respawn_elapsed += static_cast<float>(std::max(0.0, delta_seconds));
        m_status_message = "You were defeated. Respawning...";
        if (m_respawn_elapsed >= 2.0f) {
            health->current = std::max(0.0f, health->maximum);
            if (auto* transform = scene.get_component<TransformComponent>(m_player_entity)) {
                transform->position = m_spawn_position;
                transform->rotation_degrees = m_spawn_rotation;
            }
            m_respawn_elapsed = 0.0f;
            m_status_message = "Respawned.";
        }
        return;
    }
    m_respawn_elapsed = 0.0f;

    update_player(scene, delta_seconds, event_bus);
    if (m_attack_requested) {
        m_attack_requested = false;
        perform_attack(scene, event_bus);
    }
    update_doors(scene, delta_seconds);
    update_quests(scene);
    refresh_interaction_target(scene);

    update_enemies(scene, delta_seconds, event_bus);

    if (m_dialogue_choice_requested >= 0) {
        const int choice_index = m_dialogue_choice_requested;
        m_dialogue_choice_requested = -1;
        advance_dialogue(scene, choice_index);
        refresh_interaction_target(scene);
    } else if (m_interact_requested) {
        perform_interaction(scene, event_bus);
        m_interact_requested = false;
        refresh_interaction_target(scene);
    }
}


void GameplayRuntime::update_enemies(Scene& scene, double delta_seconds, GameplayEventBus* event_bus) {
    const auto* player_transform = scene.get_component<TransformComponent>(m_player_entity);
    auto* player_health = scene.get_component<HealthComponent>(m_player_entity);
    if (player_transform == nullptr || player_health == nullptr) return;

    const float dt = static_cast<float>(std::max(0.0, delta_seconds));
    int enemy_count = 0;
    int living_enemies = 0;
    scene.for_each<TransformComponent, HealthComponent, EnemyAIComponent>(
        [&](EntityId entity, TransformComponent& transform, HealthComponent& health, EnemyAIComponent& enemy) {
            if (entity == m_player_entity || !enemy.enabled) return;
            ++enemy_count;
            if (health.current <= 0.0f) return;
            ++living_enemies;

            Vec3 offset = player_transform->position - transform.position;
            offset.y = 0.0f;
            const float distance = length(offset);
            if (distance <= 0.001f || distance > enemy.detection_range) return;

            if (distance > enemy.attack_range) {
                const Vec3 direction = offset * (1.0f / distance);
                transform.position += direction * (enemy.move_speed * dt);
                transform.rotation_degrees.y = yaw_from_direction(direction);
                return;
            }

            float& elapsed = m_enemy_attack_elapsed[entity];
            elapsed += dt;
            if (elapsed < std::max(0.05f, enemy.attack_cooldown)) return;
            elapsed = 0.0f;
            if (player_health->invulnerable || player_health->current <= 0.0f) return;

            player_health->current = std::max(0.0f, player_health->current - enemy.attack_damage);
            if (event_bus != nullptr) {
                GameplayEvent damage;
                damage.source_entity = entity;
                damage.target_entity = m_player_entity;
                damage.type = GameplayEventType::Damage;
                damage.name = "EnemyAttack";
                damage.value = enemy.attack_damage;
                event_bus->publish(damage);
            }
        }
    );

    if (enemy_count > 0 && living_enemies == 0 && !m_victory_announced) {
        m_victory_announced = true;
        m_status_message = "Area cleared! You win.";
        if (event_bus != nullptr) {
            GameplayEvent victory;
            victory.source_entity = m_player_entity;
            victory.type = GameplayEventType::Interaction;
            victory.name = "Victory";
            event_bus->publish(victory);
        }
    }
}

EntityId GameplayRuntime::resolve_player_camera(Scene& scene) const {
    if (m_player_entity == InvalidEntity || !scene.is_alive(m_player_entity)) {
        return InvalidEntity;
    }

    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    const bool wants_third_person =
        controller != nullptr && controller->view_mode == PlayerViewMode::ThirdPerson;

    if (wants_third_person) {
        const std::string& player_id = scene.entity_persistent_id(m_player_entity);
        EntityId child_camera = InvalidEntity;
        scene.for_each<TransformComponent, CameraComponent>(
            [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
                if (child_camera != InvalidEntity || !camera.enabled || camera.editor_only) return;
                const auto* parent = scene.get_component<ParentComponent>(entity);
                if (parent != nullptr && parent->parent_persistent_id == player_id) {
                    child_camera = entity;
                }
            }
        );
        if (child_camera != InvalidEntity) {
            return child_camera;
        }
    }

    if (const auto* camera = scene.get_component<CameraComponent>(m_player_entity);
        camera != nullptr && camera->enabled && !camera->editor_only) {
        return m_player_entity;
    }

    const std::string& player_id = scene.entity_persistent_id(m_player_entity);
    EntityId child_camera = InvalidEntity;
    scene.for_each<TransformComponent, CameraComponent>(
        [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
            if (child_camera != InvalidEntity || !camera.enabled || camera.editor_only) return;
            const auto* parent = scene.get_component<ParentComponent>(entity);
            if (parent != nullptr && parent->parent_persistent_id == player_id) {
                child_camera = entity;
            }
        }
    );
    if (child_camera != InvalidEntity) {
        return child_camera;
    }

    EntityId primary_camera = InvalidEntity;
    scene.for_each<TransformComponent, CameraComponent>(
        [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
            if (primary_camera == InvalidEntity && camera.enabled && camera.primary && !camera.editor_only) {
                primary_camera = entity;
            }
        }
    );
    return primary_camera;
}

void GameplayRuntime::update_player(Scene& scene, double delta_seconds, GameplayEventBus* event_bus) {
    auto* transform = scene.get_component<TransformComponent>(m_player_entity);
    auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (transform == nullptr || controller == nullptr || !controller->enabled) {
        m_jump_requested = false;
        return;
    }

    if (controller->view_mode == PlayerViewMode::ThirdPerson) {
        m_third_person_yaw += m_pending_look_x * controller->look_sensitivity;
        m_third_person_pitch -= m_pending_look_y * controller->look_sensitivity;
        const float min_pitch = std::min(controller->camera_min_pitch, controller->camera_max_pitch);
        const float max_pitch = std::max(controller->camera_min_pitch, controller->camera_max_pitch);
        m_third_person_pitch = std::clamp(m_third_person_pitch, min_pitch, max_pitch);
    } else {
        transform->rotation_degrees.y += m_pending_look_x * controller->look_sensitivity;
        transform->rotation_degrees.x -= m_pending_look_y * controller->look_sensitivity;
        transform->rotation_degrees.x = std::clamp(transform->rotation_degrees.x, -89.0f, 89.0f);
    }
    m_pending_look_x = 0.0f;
    m_pending_look_y = 0.0f;

    const float speed = controller->move_speed * (m_fast ? controller->fast_multiplier : 1.0f);
    const float distance = speed * static_cast<float>(delta_seconds);
    const float movement_yaw = controller->view_mode == PlayerViewMode::ThirdPerson
        ? m_third_person_yaw
        : transform->rotation_degrees.y;
    const Vec3 movement_rotation{0.0f, movement_yaw, 0.0f};
    const Vec3 movement_forward = forward_from_euler(movement_rotation);
    const Vec3 movement_right = right_from_euler(movement_rotation);

    Vec3 wish{};
    if (m_forward) wish += movement_forward;
    if (m_backward) wish += movement_forward * -1.0f;
    if (m_right) wish += movement_right;
    if (m_left) wish += movement_right * -1.0f;
    wish.y = 0.0f;
    if (length(wish) > 1.0f) wish = normalize(wish);

    if (controller->view_mode == PlayerViewMode::ThirdPerson &&
        controller->orient_to_movement && length(wish) > 0.0001f) {
        transform->rotation_degrees.x = 0.0f;
        transform->rotation_degrees.y = yaw_from_direction(wish);
        transform->rotation_degrees.z = 0.0f;
    }

    auto* character_body = scene.get_component<CharacterBodyComponent>(m_player_entity);
    if (character_body != nullptr && character_body->enabled) {
        PhysicsSystem::move_character(
            scene,
            m_player_entity,
            wish * distance,
            m_jump_requested,
            delta_seconds,
            m_character_state
        );
        m_jump_requested = false;
        const float movement_speed = length(wish) * speed;
        const bool sprinting = m_fast && length(wish) > 0.0001f;
        publish_locomotion(
            scene,
            m_player_entity,
            movement_speed,
            m_character_state.grounded,
            m_character_state.vertical_velocity,
            sprinting
        );
        publish_footstep_event(delta_seconds, movement_speed, m_character_state.grounded, sprinting, event_bus);
        if (controller->view_mode == PlayerViewMode::ThirdPerson) {
            update_third_person_camera(scene);
        }
        return;
    }

    if (length(wish) > 0.0001f) {
        transform->position += wish * distance;
    }

    if (controller->view_mode == PlayerViewMode::FirstPerson) {
        const Vec3 world_up{0.0f, 1.0f, 0.0f};
        if (m_up) transform->position += world_up * distance;
        if (m_down) transform->position += world_up * -distance;
    }

    const float movement_speed = length(wish) * speed;
    const bool sprinting = m_fast && length(wish) > 0.0001f;
    publish_locomotion(
        scene,
        m_player_entity,
        movement_speed,
        true,
        0.0f,
        sprinting
    );
    publish_footstep_event(delta_seconds, movement_speed, true, sprinting, event_bus);
    m_jump_requested = false;
    if (controller->view_mode == PlayerViewMode::ThirdPerson) {
        update_third_person_camera(scene);
    }
}

void GameplayRuntime::publish_footstep_event(
    double delta_seconds,
    float movement_speed,
    bool grounded,
    bool sprinting,
    GameplayEventBus* event_bus
) {
    if (event_bus == nullptr || !grounded || movement_speed <= 0.001f) {
        m_footstep_elapsed = 0.0f;
        return;
    }

    m_footstep_elapsed += static_cast<float>(std::max(0.0, delta_seconds));
    const float interval = sprinting ? 0.30f : 0.45f;
    while (m_footstep_elapsed >= interval) {
        m_footstep_elapsed -= interval;
        event_bus->publish({m_player_entity, GameplayEventType::Footstep, "Footstep", 0.0f});
    }
}

void GameplayRuntime::update_third_person_camera(Scene& scene) {
    if (m_camera_entity == InvalidEntity || !scene.is_alive(m_camera_entity)) {
        m_camera_entity = resolve_player_camera(scene);
    }

    auto* camera_transform = scene.get_component<TransformComponent>(m_camera_entity);
    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (camera_transform == nullptr || controller == nullptr ||
        !scene.has_component<TransformComponent>(m_player_entity)) {
        return;
    }

    const TransformComponent player_world = world_transform(scene, m_player_entity);
    const Vec3 view_rotation{m_third_person_pitch, m_third_person_yaw, 0.0f};
    const Vec3 view_forward = forward_from_euler(view_rotation);
    const Vec3 view_right = right_from_euler(view_rotation);
    const Vec3 anchor = player_world.position +
        Vec3{0.0f, std::max(0.0f, controller->camera_height), 0.0f};
    const Vec3 desired =
        anchor - view_forward * std::max(0.1f, controller->camera_distance) +
        view_right * controller->camera_shoulder_offset;
    const Vec3 clipped = PhysicsSystem::clip_camera_position(
        scene,
        m_player_entity,
        anchor,
        desired,
        0.15f,
        0.05f
    );

    const bool camera_is_child = parent_entity(scene, m_camera_entity) == m_player_entity;
    if (camera_is_child) {
        Mat4 inverse_parent{};
        if (inverse_matrix(world_transform_matrix(scene, m_player_entity), inverse_parent)) {
            const Vec4 local = inverse_parent * Vec4{clipped.x, clipped.y, clipped.z, 1.0f};
            camera_transform->position = {local.x, local.y, local.z};
        } else {
            camera_transform->position = clipped - player_world.position;
        }
        camera_transform->rotation_degrees = {
            m_third_person_pitch,
            m_third_person_yaw - player_world.rotation_degrees.y,
            0.0f
        };
    } else {
        camera_transform->position = clipped;
        camera_transform->rotation_degrees = view_rotation;
    }
}

void GameplayRuntime::refresh_interaction_target(Scene& scene) {
    m_interaction_target = InvalidEntity;
    m_interaction_prompt.clear();

    if (m_dialogue_active) {
        if (scene.is_alive(m_dialogue_entity)) {
            m_interaction_target = m_dialogue_entity;
            m_interaction_prompt = "[E] Next";
        } else {
            m_dialogue_active = false;
            m_dialogue_entity = InvalidEntity;
            m_dialogue_line_index = 0;
        }
        return;
    }

    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (controller == nullptr || !controller->enabled || !scene.has_component<TransformComponent>(m_player_entity)) {
        return;
    }

    const TransformComponent player_world = world_transform(scene, m_player_entity);
    TransformComponent view_transform = player_world;
    if (m_camera_entity != InvalidEntity && scene.is_alive(m_camera_entity) &&
        scene.has_component<TransformComponent>(m_camera_entity)) {
        view_transform = world_transform(scene, m_camera_entity);
    }

    const Vec3 origin = view_transform.position;
    const Vec3 forward = forward_from_euler(view_transform.rotation_degrees);
    const bool third_person = controller->view_mode == PlayerViewMode::ThirdPerson;
    const float ray_distance = controller->interaction_distance +
        (third_person ? std::max(0.0f, controller->camera_distance) : 0.0f);
    float best_projection = std::numeric_limits<float>::max();

    scene.for_each<TransformComponent, InteractableComponent>(
        [&](EntityId entity, TransformComponent&, InteractableComponent& interactable) {
            if (entity == m_player_entity || !interactable.enabled) {
                return;
            }

            const TransformComponent target_world = world_transform(scene, entity);
            const float entity_radius = std::max(0.4f, maximum_scale(target_world) * 0.65f);

            const float player_distance = length(target_world.position - player_world.position);
            if (player_distance > controller->interaction_distance + entity_radius) {
                return;
            }

            const Vec3 to_target = target_world.position - origin;
            const float projection = dot(to_target, forward);
            if (projection <= 0.0f || projection > ray_distance + entity_radius) {
                return;
            }

            const Vec3 closest_point = origin + forward * projection;
            const float perpendicular_distance = length(target_world.position - closest_point);
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
    if (const auto* dialogue = scene.get_component<DialogueComponent>(m_interaction_target);
        dialogue != nullptr && !dialogue->interaction_prompt.empty()) {
        m_interaction_prompt = "[E] " + dialogue->interaction_prompt;
    }

    const auto* pickup = scene.get_component<PickupComponent>(m_interaction_target);
    if (pickup != nullptr && pickup->quantity > 1) {
        m_interaction_prompt += "  x" + std::to_string(pickup->quantity);
    }

    const auto* door = scene.get_component<DoorComponent>(m_interaction_target);
    if (door != nullptr && !door->required_item.empty() && !player_has_item(scene, door->required_item)) {
        m_interaction_prompt += "  (Requires " + door->required_item + ")";
    }
}

void GameplayRuntime::perform_interaction(Scene& scene, GameplayEventBus* event_bus) {
    if (m_dialogue_active) {
        advance_dialogue(scene);
        return;
    }

    if (m_interaction_target == InvalidEntity || !scene.is_alive(m_interaction_target)) {
        m_status_message = "Nothing to interact with.";
        return;
    }

    auto* interactable = scene.get_component<InteractableComponent>(m_interaction_target);
    if (interactable == nullptr || !interactable->enabled) {
        m_status_message = "This object is not interactable.";
        return;
    }

    if (auto* dialogue = scene.get_component<DialogueComponent>(m_interaction_target)) {
        if (dialogue->lines.empty()) {
            m_status_message = "Dialogue has no lines.";
            return;
        }
        m_dialogue_entity = m_interaction_target;
        m_dialogue_line_index = 0;
        m_dialogue_active = true;
        present_dialogue_line(scene);
        if (event_bus != nullptr) {
            event_bus->publish({m_interaction_target, GameplayEventType::Interaction, "DialogueStarted", 0.0f});
        }
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

        if (event_bus != nullptr) {
            event_bus->publish({pickup_entity, GameplayEventType::Interaction, "PickupCollected", 0.0f});
        }
        m_interaction_target = InvalidEntity;
        return;
    }

    auto* door = scene.get_component<DoorComponent>(m_interaction_target);
    if (door == nullptr) {
        m_status_message = interactable->prompt + " triggered.";
        if (event_bus != nullptr) {
            event_bus->publish({m_interaction_target, GameplayEventType::Interaction, "Interact", 0.0f});
        }
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
    if (event_bus != nullptr) {
        event_bus->publish({
            m_interaction_target,
            GameplayEventType::Interaction,
            opening ? "DoorOpened" : "DoorClosed",
            0.0f
        });
    }
}

void GameplayRuntime::present_dialogue_line(Scene& scene) {
    auto* dialogue = scene.get_component<DialogueComponent>(m_dialogue_entity);
    if (dialogue == nullptr || m_dialogue_line_index >= dialogue->lines.size()) return;

    const auto& line = dialogue->lines[m_dialogue_line_index];
    m_status_message = line.speaker + ": " + line.text;
    if (line.choices.empty()) {
        m_interaction_prompt = "[E] Next";
        return;
    }

    m_interaction_prompt = "[1-3] Choose";
    const std::size_t count = std::min<std::size_t>(3, line.choices.size());
    for (std::size_t index = 0; index < count; ++index) {
        m_status_message += "\n" + std::to_string(index + 1) + ") " + line.choices[index].label;
    }
}

void GameplayRuntime::advance_dialogue(Scene& scene, int choice_index) {
    if (!m_dialogue_active || !scene.is_alive(m_dialogue_entity)) {
        m_dialogue_active = false;
        m_dialogue_entity = InvalidEntity;
        m_dialogue_line_index = 0;
        return;
    }

    auto* dialogue = scene.get_component<DialogueComponent>(m_dialogue_entity);
    if (dialogue == nullptr || dialogue->lines.empty()) {
        m_dialogue_active = false;
        m_dialogue_entity = InvalidEntity;
        m_dialogue_line_index = 0;
        m_status_message = "Dialogue ended.";
        return;
    }

    const auto& current_line = dialogue->lines[m_dialogue_line_index];
    if (!current_line.choices.empty()) {
        if (choice_index < 0 ||
            choice_index >= static_cast<int>(current_line.choices.size()) ||
            choice_index >= 3) {
            m_status_message = "Choose a response with 1, 2, or 3.";
            m_interaction_prompt = "[1-3] Choose";
            return;
        }
        const int next_line = current_line.choices[static_cast<std::size_t>(choice_index)].next_line;
        if (next_line >= 0 && next_line < static_cast<int>(dialogue->lines.size())) {
            m_dialogue_line_index = static_cast<std::size_t>(next_line);
            present_dialogue_line(scene);
            return;
        }
    } else if (choice_index >= 0) {
        m_status_message = "This dialogue line has no choices.";
        m_interaction_prompt = "[E] Next";
        return;
    } else {
        ++m_dialogue_line_index;
        if (m_dialogue_line_index < dialogue->lines.size()) {
            present_dialogue_line(scene);
            return;
        }
    }

    const std::string quest_id = dialogue->quest_id_to_start;
    if (!dialogue->repeatable) {
        if (auto* interactable = scene.get_component<InteractableComponent>(m_dialogue_entity)) {
            interactable->enabled = false;
        }
    }
    m_dialogue_active = false;
    m_dialogue_entity = InvalidEntity;
    m_dialogue_line_index = 0;

    if (!quest_id.empty()) {
        bool started = false;
        scene.for_each<QuestComponent>([&](EntityId, QuestComponent& quest) {
            if (!started && quest.quest_id == quest_id && quest.status == QuestStatus::Inactive) {
                quest.status = QuestStatus::Active;
                m_status_message = "Quest started: " + quest.title + ".";
                started = true;
            }
        });
        if (!started) {
            m_status_message = "Dialogue ended.";
        }
    } else {
        m_status_message = "Dialogue ended.";
    }
}

void GameplayRuntime::perform_attack(Scene& scene, GameplayEventBus* event_bus) {
    if (event_bus == nullptr || m_player_entity == InvalidEntity) return;

    auto* attack = scene.get_component<AttackComponent>(m_player_entity);
    const auto* player_transform = scene.get_component<TransformComponent>(m_player_entity);
    if (attack == nullptr || player_transform == nullptr) return;

    attack->active = true;
    event_bus->publish({
        .source_entity = m_player_entity,
        .type = GameplayEventType::Animation,
        .name = attack->open_event,
        .time_seconds = 0.0f,
    });

    const Vec3 forward = forward_from_euler(player_transform->rotation_degrees);
    EntityId best_target = InvalidEntity;
    float best_distance = std::numeric_limits<float>::max();
    scene.for_each<TransformComponent, HealthComponent>(
        [&](EntityId entity, TransformComponent& transform, HealthComponent& health) {
            if (entity == m_player_entity || health.current <= 0.0f || health.invulnerable) return;
            const Vec3 offset = transform.position - player_transform->position;
            const float distance = length(offset);
            if (distance > 3.0f || distance <= 0.001f) return;
            if (dot(normalize(offset), forward) < 0.15f) return;
            if (distance < best_distance) {
                best_distance = distance;
                best_target = entity;
            }
        }
    );

    if (best_target != InvalidEntity) {
        CombatSystem::apply_attack_damage(
            scene,
            m_player_entity,
            best_target,
            *event_bus,
            0.0f
        );
    }

    event_bus->publish({
        .source_entity = m_player_entity,
        .type = GameplayEventType::Animation,
        .name = attack->close_event,
        .time_seconds = 0.0f,
    });
    attack->active = false;
}

void GameplayRuntime::update_quests(Scene& scene) {
    scene.for_each<QuestComponent>([&](EntityId, QuestComponent& quest) {
        if (quest.status != QuestStatus::Active) return;
        const int required = std::max(1, quest.required_quantity);
        if (!quest.required_item.empty()) {
            const auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
            const auto it = inventory == nullptr
                ? std::unordered_map<std::string, int>::const_iterator{}
                : inventory->items.find(quest.required_item);
            if (inventory == nullptr || it == inventory->items.end() || it->second < required) {
                return;
            }
        }
        quest.status = QuestStatus::Completed;
        m_status_message = "Quest complete: " + quest.title + ".";
        if (!quest.reward_item.empty()) {
            const int reward_quantity = std::max(1, quest.reward_quantity);
            if (add_player_item(scene, quest.reward_item, reward_quantity)) {
                m_status_message += " Reward: " + quest.reward_item;
                if (reward_quantity > 1) {
                    m_status_message += " x" + std::to_string(reward_quantity);
                }
                m_status_message += ".";
            }
        }
    });
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
