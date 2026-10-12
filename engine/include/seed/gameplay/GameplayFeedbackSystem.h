#pragma once

#include "seed/gameplay/GameplayEvents.h"
#include "seed/gameplay/GameplayFeedback.h"

namespace seed {

class Scene;

class GameplayFeedbackSystem {
public:
    static void consume_events(
        const GameplayEventBus& events,
        GameplayFeedbackBus& feedback,
        const Scene* scene = nullptr
    );
};

} // namespace seed
