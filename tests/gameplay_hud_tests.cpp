#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayHUD.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/scene/Scene.h"
#include "seed/simulation/SimulationComponents.h"

#include <cassert>
#include <iostream>

int main() {
    seed::Scene scene;
    const auto player = scene.create_entity("Player", "player");

    seed::HealthComponent health;
    health.maximum = 125.0f;
    health.current = 80.0f;
    scene.add_component<seed::HealthComponent>(player, health);

    seed::InventoryComponent inventory;
    inventory.items["CabinKey"] = 1;
    scene.add_component<seed::InventoryComponent>(player, inventory);

    seed::QuestComponent quest;
    quest.title = "Open the Cabin";
    quest.required_item = "CabinKey";
    quest.required_quantity = 1;
    quest.status = seed::QuestStatus::Active;
    scene.add_component<seed::QuestComponent>(player, quest);

    const auto shop = scene.create_entity("Fresh Market");
    seed::ShopInventoryComponent shop_inventory;
    shop_inventory.products = {{"milk", 12, 20}};
    scene.add_component<seed::ShopInventoryComponent>(shop, shop_inventory);
    seed::WalletComponent wallet;
    wallet.balance = 250.0f;
    scene.add_component<seed::WalletComponent>(shop, wallet);
    seed::ShopMetricsComponent metrics;
    metrics.daily_revenue = 45.0f;
    metrics.orders_completed_today = 3;
    scene.add_component<seed::ShopMetricsComponent>(shop, metrics);
    seed::ShopGoalComponent goal;
    goal.title = "Reach today's target";
    goal.target_revenue = 40.0f;
    goal.target_customers = 2;
    scene.add_component<seed::ShopGoalComponent>(shop, goal);
    seed::SimulationClockComponent clock;
    clock.day = 2;
    clock.hour = 10.5f;
    scene.add_component<seed::SimulationClockComponent>(shop, clock);
    scene.add_component<seed::ShopHoursComponent>(shop);

    const auto customer = scene.create_entity("Customer");
    seed::CustomerBehaviorComponent behavior;
    behavior.state = "shopping";
    scene.add_component<seed::CustomerBehaviorComponent>(customer, behavior);
    const auto order_entity = scene.create_entity("Order");
    seed::CustomerOrderComponent order;
    order.fulfilled = false;
    scene.add_component<seed::CustomerOrderComponent>(order_entity, order);

    seed::GameplayRuntime runtime;
    assert(runtime.start(scene, player));

    const auto hud = seed::GameplayHUDSystem::capture(scene, runtime);
    assert(hud.active);
    assert(hud.health_visible);
    assert(hud.health_current == 80.0f);
    assert(hud.health_maximum == 125.0f);
    assert(hud.inventory.at("CabinKey") == 1);
    assert(hud.active_objectives.size() == 1);
    assert(hud.active_objectives.front() == "Open the Cabin (CabinKey 1)");
    assert(!hud.dialogue_visible);
    assert(hud.simulation_visible);
    assert(hud.shop_name == "Fresh Market");
    assert(hud.wallet_balance == 250.0f);
    assert(hud.daily_revenue == 45.0f);
    assert(hud.simulation_day == 2);
    assert(hud.stock_quantity == 12);
    assert(hud.stock_capacity == 20);
    assert(hud.active_customers == 1);
    assert(hud.queued_orders == 1);
    assert(hud.completed_orders == 3);
    assert(hud.goal_title == "Reach today's target");
    assert(hud.goal_revenue == 45.0f);
    assert(hud.goal_target_revenue == 40.0f);
    assert(hud.goal_customers == 0);
    assert(hud.goal_target_customers == 2);
    assert(!hud.goal_completed);

    runtime.stop();
    assert(!seed::GameplayHUDSystem::capture(scene, runtime).active);

    std::cout << "SeedGameplayHUDTests passed.\n";
    return 0;
}
