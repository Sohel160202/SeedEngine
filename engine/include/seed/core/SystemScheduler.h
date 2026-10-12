#pragma once

#include "seed/gameplay/GameplayEvents.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace seed {

enum class SystemPhase : std::uint8_t {
    Input = 0,
    Gameplay = 1,
    Physics = 2,
    Animation = 3,
    EventDispatch = 4,
    Custom = 5
};

using UpdateSystem = std::function<void(
    Scene&,
    double,
    GameplayEventBus&
)>;

class SystemScheduler {
public:
    struct Entry {
        std::string name;
        SystemPhase phase{SystemPhase::Custom};
        UpdateSystem update;
    };

    bool register_system(
        std::string name,
        SystemPhase phase,
        UpdateSystem update
    ) {
        if (name.empty() || !update || contains(name)) {
            return false;
        }

        m_systems.push_back({
            .name = std::move(name),
            .phase = phase,
            .update = std::move(update),
        });
        std::stable_sort(
            m_systems.begin(),
            m_systems.end(),
            [](const Entry& left, const Entry& right) {
                return static_cast<std::uint8_t>(left.phase) <
                    static_cast<std::uint8_t>(right.phase);
            }
        );
        return true;
    }

    bool unregister_system(const std::string& name) {
        const auto old_size = m_systems.size();
        m_systems.erase(
            std::remove_if(
                m_systems.begin(),
                m_systems.end(),
                [&](const Entry& entry) { return entry.name == name; }
            ),
            m_systems.end()
        );
        return m_systems.size() != old_size;
    }

    void clear() noexcept {
        m_systems.clear();
    }

    void update(
        Scene& scene,
        double delta_seconds,
        GameplayEventBus& event_bus
    ) {
        for (auto& system : m_systems) {
            system.update(scene, delta_seconds, event_bus);
        }
    }

    const std::vector<Entry>& systems() const noexcept {
        return m_systems;
    }

private:
    bool contains(const std::string& name) const {
        return std::any_of(
            m_systems.begin(),
            m_systems.end(),
            [&](const Entry& entry) { return entry.name == name; }
        );
    }

    std::vector<Entry> m_systems;
};

} // namespace seed
