#pragma once

#include "seed/core/Types.h"
#include "seed/math/Math.h"
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

    bool active() const noexcept { return m_active; }
    EntityId player_entity() const noexcept { return m_player_entity; }
    EntityId interaction_target() const noexcept { return m_interaction_target; }

    const std::string& interaction_prompt() const noexcept { return m_interaction_prompt; }
    const std::string& status_message() const noexcept { return m_status_message; }

private:
    struct DoorRuntimeState {
        Vec3 closed_position{};
        Vec3 closed_rotation{};
        float progress{0.0f};
        bool target_open{false};
    };

    void refresh_interaction_target(Scene& scene);
    void perform_interaction(Scene& scene);
    void update_player(Scene& scene, double delta_seconds);
    void update_doors(Scene& scene, double delta_seconds);
    void apply_door_transform(Scene& scene, EntityId entity, const DoorRuntimeState& state);
    bool player_has_item(const Scene& scene, const std::string& item_id) const;
    bool add_player_item(Scene& scene, const std::string& item_id, int quantity);
    bool consume_player_item(Scene& scene, const std::string& item_id);

    bool m_active{false};
    EntityId m_player_entity{InvalidEntity};
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
    float m_last_mouse_x{0.0f};
    float m_last_mouse_y{0.0f};
    float m_pending_look_x{0.0f};
    float m_pending_look_y{0.0f};

    std::unordered_map<EntityId, DoorRuntimeState> m_doors;
    std::string m_interaction_prompt;
    std::string m_status_message;
};

} // namespace seed
