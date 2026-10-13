#include "seed/simulation/SimulationSystem.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace seed {
namespace {

ProductStock* find_stock(ShopInventoryComponent& inventory, const std::string& product_id) {
    for(auto& stock : inventory.products) {
        if(stock.product_id == product_id) return &stock;
    }
    return nullptr;
}

SimulationTransaction failure(std::string reason) {
    return {.succeeded = false, .quantity = 0, .total = 0.0f, .reason = std::move(reason)};
}

} // namespace

void SimulationSystem::advance_clock(
    SimulationClockComponent& clock,
    float real_delta_seconds
) {
    if(clock.paused || real_delta_seconds <= 0.0f || clock.time_scale <= 0.0f) return;
    clock.hour += real_delta_seconds * clock.time_scale / 60.0f;
    while(clock.hour >= 24.0f) {
        clock.hour -= 24.0f;
        ++clock.day;
    }
}

bool SimulationSystem::is_shop_open(
    const ShopHoursComponent& hours,
    const SimulationClockComponent& clock
) {
    if(hours.opening_hour == hours.closing_hour) return true;
    if(hours.opening_hour < hours.closing_hour) {
        return clock.hour >= hours.opening_hour && clock.hour < hours.closing_hour;
    }
    return clock.hour >= hours.opening_hour || clock.hour < hours.closing_hour;
}

SimulationTransaction SimulationSystem::sell_product(
    ShopInventoryComponent& inventory,
    WalletComponent& wallet,
    const ProductDefinitionComponent& product,
    int quantity
) {
    if(quantity <= 0) return failure("Quantity must be greater than zero.");
    auto* stock = find_stock(inventory, product.product_id);
    if(stock == nullptr || stock->quantity < quantity) return failure("Not enough stock.");
    if(product.retail_price < 0.0f) return failure("Retail price cannot be negative.");

    const float total = product.retail_price * static_cast<float>(quantity);
    stock->quantity -= quantity;
    wallet.balance += total;
    wallet.lifetime_revenue += total;
    return {.succeeded = true, .quantity = quantity, .total = total, .reason = {}};
}

SimulationTransaction SimulationSystem::restock_product(
    ShopInventoryComponent& inventory,
    WalletComponent& wallet,
    const ProductDefinitionComponent& product,
    int quantity
) {
    if(quantity <= 0) return failure("Quantity must be greater than zero.");
    if(product.wholesale_price < 0.0f) return failure("Wholesale price cannot be negative.");
    auto* stock = find_stock(inventory, product.product_id);
    if(stock == nullptr) {
        inventory.products.push_back({
            .product_id = product.product_id,
            .quantity = 0,
            .shelf_capacity = product.max_stock
        });
        stock = &inventory.products.back();
    }
    const int capacity = std::max(0, product.max_stock);
    const int accepted = std::min(quantity, std::max(0, capacity - stock->quantity));
    if(accepted <= 0) return failure("Inventory is full.");
    const float total = product.wholesale_price * static_cast<float>(accepted);
    if(wallet.balance < total) return failure("Not enough money.");
    stock->quantity += accepted;
    wallet.balance -= total;
    wallet.lifetime_expenses += total;
    return {.succeeded = true, .quantity = accepted, .total = total, .reason = {}};
}

float SimulationSystem::order_total(const CustomerOrderComponent& order) {
    float total = 0.0f;
    for(const auto& line : order.lines) {
        if(line.quantity > 0 && std::isfinite(line.unit_price)) {
            total += line.unit_price * static_cast<float>(line.quantity);
        }
    }
    return total;
}

} // namespace seed
