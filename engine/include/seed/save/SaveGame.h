#pragma once

#include "seed/math/Math.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace seed {

class Scene;

inline constexpr int SeedSaveFormatVersion = 1;

struct SaveTransformState {
    Vec3 position{};
    Vec3 rotation_degrees{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct SaveHealthState {
    float current{100.0f};
};

struct SaveInventoryState {
    std::unordered_map<std::string, int> items;
};

struct SaveQuestState {
    std::string quest_id;
    int status{0};
};

struct SaveWalletState {
    float balance{0.0f};
    float lifetime_revenue{0.0f};
    float lifetime_expenses{0.0f};
};

struct SaveProductStockState {
    std::string product_id;
    int quantity{0};
    int shelf_capacity{0};
};

struct SaveShopInventoryState {
    std::vector<SaveProductStockState> products;
};

struct SaveShopMetricsState {
    int tracked_day{1};
    int customers_served_today{0};
    int orders_completed_today{0};
    int products_sold_today{0};
    float daily_revenue{0.0f};
    float daily_satisfaction{100.0f};
    int satisfaction_samples{0};
};

struct SaveShopClockState {
    int day{1};
    float hour{8.0f};
    float time_scale{1.0f};
    bool paused{false};
};

struct SaveShopHoursState {
    float opening_hour{8.0f};
    float closing_hour{20.0f};
};

struct SaveShopUpgradeState {
    int level{1};
};

struct SaveStaffState {
    bool employed{true};
    int last_paid_day{1};
    int skill_level{1};
};

struct SaveCustomerBehaviorState {
    bool enabled{true};
    float timer_seconds{0.0f};
    std::string state{"waiting"};
    int order_sequence{1};
};

struct SaveOrderLineState {
    std::string product_id;
    int quantity{0};
    int fulfilled_quantity{0};
    float unit_price{0.0f};
};

struct SaveCustomerOrderState {
    std::string order_id;
    std::string customer_name;
    std::vector<SaveOrderLineState> lines;
    bool fulfilled{false};
    bool delivered{false};
};

struct SaveEntityState {
    std::string persistent_id;
    std::optional<SaveTransformState> transform;
    std::optional<SaveHealthState> health;
    std::optional<SaveInventoryState> inventory;
    std::optional<SaveQuestState> quest;
    std::optional<SaveWalletState> wallet;
    std::optional<SaveShopInventoryState> shop_inventory;
    std::optional<SaveShopMetricsState> shop_metrics;
    std::optional<SaveShopClockState> shop_clock;
    std::optional<SaveShopHoursState> shop_hours;
    std::optional<SaveShopUpgradeState> shop_upgrade;
    std::optional<SaveStaffState> staff;
    std::optional<SaveCustomerBehaviorState> customer_behavior;
    std::optional<SaveCustomerOrderState> customer_order;
};

struct SaveGameSnapshot {
    int format_version{SeedSaveFormatVersion};
    std::string slot{"slot_1"};
    int game_schema_version{1};
    std::vector<SaveEntityState> entities;
};

struct SaveCaptureResult {
    SaveGameSnapshot snapshot;
    std::vector<std::string> warnings;
};

struct SaveApplyResult {
    std::size_t entities_applied{0};
    std::size_t properties_applied{0};
    std::size_t entities_skipped{0};
    std::vector<std::string> warnings;

    bool success() const noexcept { return warnings.empty(); }
};

// Native Seed gameplay persistence. Authored defaults live in .seedscene;
// runtime progress lives in .seedsave. Stable persistent IDs are the semantic
// bridge between the two, and load never guesses when structure changed.
class SaveGameSystem {
public:
    static SaveCaptureResult capture(
        const Scene& scene,
        std::string slot,
        int game_schema_version = 1
    );

    static SaveApplyResult apply(Scene& scene, const SaveGameSnapshot& snapshot);

    static bool write_file(
        const SaveGameSnapshot& snapshot,
        const std::filesystem::path& file,
        std::string* error = nullptr
    );

    static std::optional<SaveGameSnapshot> read_file(
        const std::filesystem::path& file,
        std::string* error = nullptr
    );
};

} // namespace seed
