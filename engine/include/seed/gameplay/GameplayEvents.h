#pragma once

#include "seed/core/Types.h"

#include <string>
#include <utility>
#include <vector>

namespace seed {

enum class GameplayEventType {
    BeginPlay,
    Trigger,
    Quest,
    Animation,
    Footstep,
    Interaction,
    Damage,
    Combat
};

struct GameplayEvent {
    EntityId source_entity{InvalidEntity};
    GameplayEventType type{GameplayEventType::Animation};
    std::string name;
    float time_seconds{0.0f};
    EntityId target_entity{InvalidEntity};
    float value{0.0f};
};

// Frame-scoped, asset-independent event transport between engine systems.
// Producers publish during simulation; consumers drain after the relevant update.
class GameplayEventBus {
public:
    void publish(GameplayEvent event) {
        m_pending.push_back(std::move(event));
    }

    const std::vector<GameplayEvent>& pending() const noexcept {
        return m_pending;
    }

    std::vector<GameplayEvent> drain() {
        std::vector<GameplayEvent> events = std::move(m_pending);
        m_pending.clear();
        return events;
    }

    void clear() noexcept {
        m_pending.clear();
    }

private:
    std::vector<GameplayEvent> m_pending;
};

} // namespace seed
