#include "seed/simulation/SimulationSystem.h"

#include <cassert>
#include <cmath>

int main() {
    seed::ProductDefinitionComponent product{
        .product_id = "milk",
        .display_name = "Milk",
        .category = "Food",
        .wholesale_price = 2.0f,
        .retail_price = 4.0f,
        .max_stock = 20,
        .perishable = true,
    };
    seed::ShopInventoryComponent inventory;
    seed::WalletComponent wallet{.balance = 20.0f};

    const auto restock = seed::SimulationSystem::restock_product(inventory, wallet, product, 5);
    assert(restock.succeeded);
    assert(restock.quantity == 5);
    assert(std::abs(wallet.balance - 10.0f) < 0.001f);
    assert(inventory.products.front().quantity == 5);

    const auto sale = seed::SimulationSystem::sell_product(inventory, wallet, product, 2);
    assert(sale.succeeded);
    assert(sale.total == 8.0f);
    assert(inventory.products.front().quantity == 3);
    assert(std::abs(wallet.balance - 18.0f) < 0.001f);

    const auto rejected = seed::SimulationSystem::sell_product(inventory, wallet, product, 5);
    assert(!rejected.succeeded);

    seed::SimulationClockComponent clock{.day = 1, .hour = 23.5f, .time_scale = 2.0f};
    seed::SimulationSystem::advance_clock(clock, 30.0f);
    assert(clock.day == 2);
    assert(clock.hour > 0.4f && clock.hour < 0.6f);

    seed::CustomerOrderComponent order;
    order.lines = {{"milk", 2, 0, 4.0f}, {"bread", 1, 0, 3.0f}};
    assert(seed::SimulationSystem::order_total(order) == 11.0f);
    return 0;
}
