#pragma once

#include "seed/gameplay/GameplayEvents.h"

namespace seed {

class Scene;

class GameplayRulesSystem {
public:
    // Evaluates beginner-authored "When ... Then ..." rules against the
    // frame-scoped semantic event bus without taking ownership of events.
    static void consume_events(Scene& scene, GameplayEventBus& events);
};

} // namespace seed
