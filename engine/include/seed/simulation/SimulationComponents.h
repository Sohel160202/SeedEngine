#pragma once

#include <string>
#include <vector>

namespace seed {

struct ProductDefinitionComponent {
    std::string product_id{"product"};
    std::string display_name{"Product"};
    std::string category{"General"};
    float wholesale_price{1.0f};
    float retail_price{2.0f};
    float demand_weight{1.0f};
    float price_sensitivity{1.0f};
    int max_stock{100};
    bool perishable{false};
};

struct ProductStock {
    std::string product_id{"product"};
    int quantity{0};
    int shelf_capacity{20};
};

struct ShopInventoryComponent {
    std::vector<ProductStock> products;
};

struct WalletComponent {
    float balance{1000.0f};
    float lifetime_revenue{0.0f};
    float lifetime_expenses{0.0f};
};

struct ShopMetricsComponent {
    int tracked_day{1};
    int customers_served_today{0};
    int orders_completed_today{0};
    int products_sold_today{0};
    float daily_revenue{0.0f};
    float daily_satisfaction{100.0f};
    int satisfaction_samples{0};
};

struct ShopUpgradeComponent {
    int level{1};
    int max_level{5};
    float base_upgrade_cost{100.0f};
    float cost_multiplier{1.5f};
    int shelf_capacity_bonus{5};
};

struct ShopHoursComponent {
    float opening_hour{8.0f};
    float closing_hour{20.0f};
};

struct SimulationClockComponent {
    int day{1};
    float hour{8.0f};
    float time_scale{1.0f};
    bool paused{false};
};

struct OrderLine {
    std::string product_id{"product"};
    int quantity{1};
    int fulfilled_quantity{0};
    float unit_price{0.0f};
};

struct CheckoutStationComponent {
    bool enabled{true};
    float service_time_seconds{1.0f};
};

struct CustomerOrderComponent {
    std::string order_id{"order-1"};
    std::string customer_name{"Customer"};
    std::vector<OrderLine> lines;
    bool fulfilled{false};
    bool delivered{false};
};

struct CustomerBehaviorComponent {
    bool enabled{true};
    float arrival_interval_seconds{12.0f};
    float shopping_duration_seconds{4.0f};
    float leave_duration_seconds{5.0f};
    float timer_seconds{0.0f};
    std::string state{"waiting"};
    int order_sequence{1};
};

} // namespace seed
