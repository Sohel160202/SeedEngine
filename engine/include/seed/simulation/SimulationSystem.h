#pragma once

#include "seed/simulation/SimulationComponents.h"

#include <string>

namespace seed {

struct SimulationTransaction {
    bool succeeded{false};
    int quantity{0};
    float total{0.0f};
    std::string reason;
};

class SimulationSystem {
public:
    static void advance_clock(
        SimulationClockComponent& clock,
        float real_delta_seconds
    );

    static bool is_shop_open(
        const ShopHoursComponent& hours,
        const SimulationClockComponent& clock
    );

    static SimulationTransaction sell_product(
        ShopInventoryComponent& inventory,
        WalletComponent& wallet,
        const ProductDefinitionComponent& product,
        int quantity
    );

    static SimulationTransaction restock_product(
        ShopInventoryComponent& inventory,
        WalletComponent& wallet,
        const ProductDefinitionComponent& product,
        int quantity
    );

    static float order_total(const CustomerOrderComponent& order);
};

} // namespace seed
