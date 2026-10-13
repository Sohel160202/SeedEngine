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

struct CustomerOrderComponent {
    std::string order_id{"order-1"};
    std::string customer_name{"Customer"};
    std::vector<OrderLine> lines;
    bool fulfilled{false};
    bool delivered{false};
};

} // namespace seed
