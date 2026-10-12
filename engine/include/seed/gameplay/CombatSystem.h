#pragma once

#include "seed/gameplay/GameplayEvents.h"

namespace seed {

class Scene;

class CombatSystem {
public:
    // Consumes animation and damage events without draining the shared bus,
    // allowing feedback, audio, effects, and analytics consumers to observe
    // the same frame.
    static void consume_events(
        Scene& scene,
        GameplayEventBus& event_bus
    );

    // Applies an authored attack to a target when the attack window is open.
    // Collision/range systems can call this once they confirm a hit.
    static bool apply_attack_damage(
        Scene& scene,
        EntityId attacker,
        EntityId target,
        GameplayEventBus& event_bus,
        float time_seconds
    );

private:
    static bool apply_damage(
        Scene& scene,
        EntityId source,
        EntityId target,
        float amount,
        GameplayEventBus& event_bus,
        float time_seconds
    );
};

} // namespace seed
