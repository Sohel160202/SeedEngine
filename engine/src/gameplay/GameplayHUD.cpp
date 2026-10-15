#include "seed/gameplay/GameplayHUD.h"

#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/scene/Scene.h"
#include "seed/simulation/SimulationComponents.h"

#include <algorithm>
#include <string>

namespace seed {

GameplayHUDState GameplayHUDSystem::capture(
    const Scene& scene,
    const GameplayRuntime& runtime
) {
    GameplayHUDState state;
    state.active = runtime.active();
    state.interaction_target = runtime.interaction_target();
    state.interaction_visible =
        state.interaction_target != InvalidEntity &&
        !runtime.interaction_prompt().empty();
    state.interaction_prompt = runtime.interaction_prompt();
    state.dialogue_visible = runtime.dialogue_active();
    state.status_message = runtime.status_message();

    scene.for_each<ShopInventoryComponent>([&](EntityId shop_entity, const ShopInventoryComponent& inventory) {
        if (state.simulation_visible) return;
        state.simulation_visible = true;
        state.shop_name = scene.entity_name(shop_entity);
        for (const auto& product : inventory.products) {
            state.stock_quantity += std::max(0, product.quantity);
            state.stock_capacity += std::max(0, product.shelf_capacity);
        }
        if (const auto* wallet = scene.get_component<WalletComponent>(shop_entity)) {
            state.wallet_balance = wallet->balance;
            state.lifetime_revenue = wallet->lifetime_revenue;
            state.lifetime_expenses = wallet->lifetime_expenses;
        }
        if (const auto* metrics = scene.get_component<ShopMetricsComponent>(shop_entity)) {
            state.daily_revenue = metrics->daily_revenue;
            state.daily_satisfaction = std::clamp(metrics->daily_satisfaction, 0.0f, 100.0f);
            state.completed_orders = metrics->orders_completed_today;
        }
        if (const auto* upgrade = scene.get_component<ShopUpgradeComponent>(shop_entity)) {
            state.shop_upgrade_level = std::max(1, upgrade->level);
        }
        if (const auto* goal = scene.get_component<ShopGoalComponent>(shop_entity)) {
            state.goal_title = goal->title;
            state.goal_revenue = state.daily_revenue;
            const auto* clock = scene.get_component<SimulationClockComponent>(shop_entity);
            const int simulation_day = clock == nullptr
                ? state.simulation_day
                : std::max(1, clock->day);
            const int elapsed_days = std::max(0, simulation_day - 1);
            state.goal_target_revenue = std::max(
                0.0f,
                goal->target_revenue +
                    static_cast<float>(elapsed_days) * std::max(0.0f, goal->revenue_growth_per_day)
            );
            state.goal_customers = 0;
            if (const auto* metrics = scene.get_component<ShopMetricsComponent>(shop_entity)) {
                state.goal_customers = metrics->customers_served_today;
            }
            state.goal_target_customers = std::max(
                0,
                goal->target_customers + elapsed_days * std::max(0, goal->customer_growth_per_day)
            );
            const bool revenue_met = state.goal_target_revenue <= 0.0f ||
                state.goal_revenue >= state.goal_target_revenue;
            const bool customers_met = state.goal_target_customers <= 0 ||
                state.goal_customers >= state.goal_target_customers;
            state.goal_completed = revenue_met && customers_met;
        }
        if (const auto* clock = scene.get_component<SimulationClockComponent>(shop_entity)) {
            state.simulation_day = clock->day;
            state.simulation_hour = clock->hour;
        }
        if (const auto* hours = scene.get_component<ShopHoursComponent>(shop_entity)) {
            state.shop_open = state.simulation_hour >= hours->opening_hour &&
                state.simulation_hour < hours->closing_hour;
        }
    });
    scene.for_each<CustomerBehaviorComponent>([&](EntityId, const CustomerBehaviorComponent& customer) {
        if (customer.enabled && customer.state != "leaving") ++state.active_customers;
    });
    scene.for_each<CustomerOrderComponent>([&](EntityId, const CustomerOrderComponent& order) {
        if (!order.fulfilled) ++state.queued_orders;
    });

    if (runtime.player_entity() == InvalidEntity ||
        !scene.is_alive(runtime.player_entity())) {
        return state;
    }

    if (const auto* health = scene.get_component<HealthComponent>(runtime.player_entity())) {
        state.health_visible = true;
        state.health_maximum = std::max(0.0f, health->maximum);
        state.health_current = std::clamp(
            health->current,
            0.0f,
            state.health_maximum
        );
    }

    if (const auto* inventory = scene.get_component<InventoryComponent>(runtime.player_entity())) {
        state.inventory = inventory->items;
    }

    scene.for_each<QuestComponent>([&](EntityId, const QuestComponent& quest) {
        if (quest.status != QuestStatus::Active) return;

        std::string objective = quest.title;
        if (quest.required_quantity > 0 && !quest.required_item.empty()) {
            objective += " (" + quest.required_item + " " +
                std::to_string(quest.required_quantity) + ")";
        }
        state.active_objectives.push_back(std::move(objective));
    });

    return state;
}

} // namespace seed
