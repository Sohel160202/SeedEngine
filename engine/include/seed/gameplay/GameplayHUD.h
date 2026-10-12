#pragma once

#include "seed/core/Types.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace seed {

class GameplayRuntime;
class Scene;

// Renderer-agnostic HUD snapshot. Editor, runtime overlays, and future UI
// backends can present the same beginner-facing gameplay state.
struct GameplayHUDState {
    bool active{false};
    bool health_visible{false};
    float health_current{0.0f};
    float health_maximum{0.0f};

    bool interaction_visible{false};
    EntityId interaction_target{InvalidEntity};
    std::string interaction_prompt;

    bool dialogue_visible{false};
    std::string status_message;
    std::vector<std::string> active_objectives;
    std::unordered_map<std::string, int> inventory;
};

class GameplayHUDSystem {
public:
    static GameplayHUDState capture(
        const Scene& scene,
        const GameplayRuntime& runtime
    );
};

} // namespace seed
