#pragma once

#include "seed/gameplay/GameplayEvents.h"

namespace seed {

class Scene;

class CombatSystem {
public:
    // Consumes animation events without draining the shared bus, allowing
    // audio, effects, and analytics consumers to observe the same frame.
    static void consume_events(
        Scene& scene,
        const GameplayEventBus& event_bus
    );
};

} // namespace seed
