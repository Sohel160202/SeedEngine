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

    bool simulation_visible{false};
    std::string shop_name;
    float wallet_balance{0.0f};
    float daily_revenue{0.0f};
    int simulation_day{1};
    float simulation_hour{8.0f};
    bool shop_open{false};
    int stock_quantity{0};
    int stock_capacity{0};
    int active_customers{0};
    int queued_orders{0};
    int completed_orders{0};
    float daily_satisfaction{100.0f};
    float lifetime_revenue{0.0f};
    float lifetime_expenses{0.0f};
    int shop_upgrade_level{1};
    std::string goal_title;
    float goal_revenue{0.0f};
    float goal_target_revenue{0.0f};
    int goal_customers{0};
    int goal_target_customers{0};
    bool goal_completed{false};
};

class GameplayHUDSystem {
public:
    static GameplayHUDState capture(
        const Scene& scene,
        const GameplayRuntime& runtime
    );
};

} // namespace seed
