#pragma once

#include "seed/core/Types.h"
#include "seed/input/InputActions.h"
#include "seed/math/Math.h"
#include "seed/physics/PhysicsSystem.h"
#include "seed/platform/IPlatform.h"

#include <string>
#include <unordered_map>

namespace seed {

class Scene;

class GameplayRuntime {
public:
    bool start(Scene& scene, EntityId player_entity);
    void stop();

    void handle_event(const PlatformEvent& event);
    void update(Scene& scene, double delta_seconds);

    InputActionMap& input_map() noexcept { return m_input_map; }
    const InputActionMap& input_map() const noexcept { return m_input_map; }

    bool active() const noexcept { return m_active; }
    EntityId player_entity() const noexcept { return m_player_entity; }
    EntityId camera_entity() const noexcept { return m_camera_entity; }
    EntityId interaction_target() const noexcept { return m_interaction_target; }
    bool player_grounded() const noexcept { return m_character_state.grounded; }

    const std::string& interaction_prompt() const noexcept { return m_interaction_prompt; }
    const std::string& status_message() const noexcept { return m_status_message; }

private:
    struct DoorRuntimeState {
        Vec3 closed_position{};
        Vec3 closed_rotation{};
        float progress{0.0f};
        bool target_open{false};
    };

    EntityId resolve_player_camera(Scene& scene) const;
    void refresh_interaction_target(Scene& scene);
    void perform_interaction(Scene& scene);
    void update_player(Scene& scene, double delta_seconds);
    void update_third_person_camera(Scene& scene);
    void update_doors(Scene& scene, double delta_seconds);
    void update_quests(Scene& scene);
    void advance_dialogue(Scene& scene, int choice_index = -1);
    void present_dialogue_line(Scene& scene);
    void apply_door_transform(Scene& scene, EntityId entity, const DoorRuntimeState& state);
    bool player_has_item(const Scene& scene, const std::string& item_id) const;
    bool add_player_item(Scene& scene, const std::string& item_id, int quantity);
    bool consume_player_item(Scene& scene, const std::string& item_id);

    bool m_active{false};
    EntityId m_player_entity{InvalidEntity};
    EntityId m_camera_entity{InvalidEntity};
    EntityId m_interaction_target{InvalidEntity};

    bool m_forward{false};
    bool m_backward{false};
    bool m_left{false};
    bool m_right{false};
    bool m_down{false};
    bool m_up{false};
    bool m_fast{false};
    bool m_looking{false};
    bool m_has_mouse_position{false};
    bool m_interact_requested{false};
    bool m_jump_requested{false};
    float m_last_mouse_x{0.0f};
    float m_last_mouse_y{0.0f};
    float m_pending_look_x{0.0f};
    float m_pending_look_y{0.0f};
    float m_third_person_yaw{0.0f};
    float m_third_person_pitch{-12.0f};

    InputActionMap m_input_map;
    PhysicsSystem::CharacterState m_character_state{};
    std::unordered_map<EntityId, DoorRuntimeState> m_doors;
    EntityId m_dialogue_entity{InvalidEntity};
    std::size_t m_dialogue_line_index{0};
    bool m_dialogue_active{false};
    int m_dialogue_choice_requested{-1};
    std::string m_interaction_prompt;
    std::string m_status_message;
};

} // namespace seed
