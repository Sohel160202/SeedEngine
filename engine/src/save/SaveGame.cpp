#include "seed/save/SaveGame.h"

#include "seed/gameplay/Components.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/simulation/SimulationComponents.h"
#include "seed/scene/Scene.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <utility>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

nlohmann::json vec3_to_json(const Vec3& value) {
    return nlohmann::json::array({value.x, value.y, value.z});
}

Vec3 vec3_from_json(const nlohmann::json& value, Vec3 fallback = {}) {
    if (!value.is_array() || value.size() != 3) return fallback;
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
    };
}

std::string entity_label(const Scene& scene, EntityId entity) {
    const std::string& name = scene.entity_name(entity);
    const std::string& id = scene.entity_persistent_id(entity);
    return name.empty() ? id : name + " (" + id + ")";
}

} // namespace

SaveCaptureResult SaveGameSystem::capture(
    const Scene& scene,
    std::string slot,
    int game_schema_version
) {
    SaveCaptureResult result;
    result.snapshot.slot = slot.empty() ? "slot_1" : std::move(slot);
    result.snapshot.game_schema_version = std::max(1, game_schema_version);

    scene.for_each_entity([&](EntityId entity, const std::string&) {
        if (!scene.is_alive(entity)) return;
        const auto* persistent = scene.get_component<PersistentComponent>(entity);
        const bool has_runtime_state =
            scene.has_component<TransformComponent>(entity) ||
            scene.has_component<HealthComponent>(entity) ||
            scene.has_component<InventoryComponent>(entity) ||
            scene.has_component<QuestComponent>(entity) ||
            scene.has_component<WalletComponent>(entity) ||
            scene.has_component<ShopInventoryComponent>(entity) ||
            scene.has_component<ShopMetricsComponent>(entity) ||
            scene.has_component<SimulationClockComponent>(entity) ||
            scene.has_component<ShopHoursComponent>(entity) ||
            scene.has_component<ShopUpgradeComponent>(entity) ||
            scene.has_component<StaffWorkerComponent>(entity) ||
            scene.has_component<CustomerBehaviorComponent>(entity) ||
            scene.has_component<CustomerOrderComponent>(entity);
        if ((persistent == nullptr || !persistent->enabled) && !has_runtime_state) return;

        SaveEntityState state;
        state.persistent_id = scene.entity_persistent_id(entity);
        if (state.persistent_id.empty()) {
            result.warnings.push_back(
                "Persistent entity has no stable ID and was skipped: " + scene.entity_name(entity)
            );
            return;
        }

        if (const auto* transform = scene.get_component<TransformComponent>(entity)) {
            state.transform = SaveTransformState{
                .position = transform->position,
                .rotation_degrees = transform->rotation_degrees,
                .scale = transform->scale,
            };
        }

        if (const auto* health = scene.get_component<HealthComponent>(entity)) {
            state.health = SaveHealthState{.current = health->current};
        }

        if (const auto* inventory = scene.get_component<InventoryComponent>(entity)) {
            state.inventory = SaveInventoryState{.items = inventory->items};
        }

        if (const auto* quest = scene.get_component<QuestComponent>(entity)) {
            state.quest = SaveQuestState{
                .quest_id = quest->quest_id,
                .status = static_cast<int>(quest->status),
            };
        }
        if (const auto* wallet = scene.get_component<WalletComponent>(entity)) {
            state.wallet = SaveWalletState{
                .balance = wallet->balance,
                .lifetime_revenue = wallet->lifetime_revenue,
                .lifetime_expenses = wallet->lifetime_expenses,
            };
        }
        if (const auto* inventory = scene.get_component<ShopInventoryComponent>(entity)) {
            SaveShopInventoryState saved_inventory;
            for (const auto& product : inventory->products) {
                saved_inventory.products.push_back({
                    .product_id = product.product_id,
                    .quantity = product.quantity,
                    .shelf_capacity = product.shelf_capacity,
                });
            }
            state.shop_inventory = std::move(saved_inventory);
        }
        if (const auto* metrics = scene.get_component<ShopMetricsComponent>(entity)) {
            state.shop_metrics = SaveShopMetricsState{
                .tracked_day = metrics->tracked_day,
                .customers_served_today = metrics->customers_served_today,
                .orders_completed_today = metrics->orders_completed_today,
                .products_sold_today = metrics->products_sold_today,
                .daily_revenue = metrics->daily_revenue,
                .daily_satisfaction = metrics->daily_satisfaction,
                .satisfaction_samples = metrics->satisfaction_samples,
            };
        }
        if (const auto* clock = scene.get_component<SimulationClockComponent>(entity)) {
            state.shop_clock = SaveShopClockState{
                .day = clock->day,
                .hour = clock->hour,
                .time_scale = clock->time_scale,
                .paused = clock->paused,
            };
        }
        if (const auto* hours = scene.get_component<ShopHoursComponent>(entity)) {
            state.shop_hours = SaveShopHoursState{
                .opening_hour = hours->opening_hour,
                .closing_hour = hours->closing_hour,
            };
        }
        if (const auto* upgrade = scene.get_component<ShopUpgradeComponent>(entity)) {
            state.shop_upgrade = SaveShopUpgradeState{.level = upgrade->level};
        }
        if (const auto* staff = scene.get_component<StaffWorkerComponent>(entity)) {
            state.staff = SaveStaffState{
                .employed = staff->employed,
                .last_paid_day = staff->last_paid_day,
                .skill_level = staff->skill_level,
            };
        }
        if (const auto* behavior = scene.get_component<CustomerBehaviorComponent>(entity)) {
            state.customer_behavior = SaveCustomerBehaviorState{
                .enabled = behavior->enabled,
                .timer_seconds = behavior->timer_seconds,
                .state = behavior->state,
                .order_sequence = behavior->order_sequence,
            };
        }
        if (const auto* order = scene.get_component<CustomerOrderComponent>(entity)) {
            SaveCustomerOrderState saved_order;
            saved_order.order_id = order->order_id;
            saved_order.customer_name = order->customer_name;
            saved_order.fulfilled = order->fulfilled;
            saved_order.delivered = order->delivered;
            for (const auto& line : order->lines) {
                saved_order.lines.push_back({
                    .product_id = line.product_id,
                    .quantity = line.quantity,
                    .fulfilled_quantity = line.fulfilled_quantity,
                    .unit_price = line.unit_price,
                });
            }
            state.customer_order = std::move(saved_order);
        }

        if (!state.transform.has_value() && !state.health.has_value() &&
            !state.inventory.has_value() && !state.quest.has_value() &&
            !state.wallet.has_value() && !state.shop_inventory.has_value() &&
            !state.shop_metrics.has_value() && !state.shop_clock.has_value() &&
            !state.shop_hours.has_value() && !state.shop_upgrade.has_value() &&
            !state.staff.has_value() && !state.customer_behavior.has_value() &&
            !state.customer_order.has_value()) {
            result.warnings.push_back(
                "Persistent entity has no Save Game v0 state yet: " + entity_label(scene, entity)
            );
            return;
        }

        result.snapshot.entities.push_back(std::move(state));
    });

    std::sort(
        result.snapshot.entities.begin(),
        result.snapshot.entities.end(),
        [](const SaveEntityState& a, const SaveEntityState& b) {
            return a.persistent_id < b.persistent_id;
        }
    );

    return result;
}

SaveApplyResult SaveGameSystem::apply(Scene& scene, const SaveGameSnapshot& snapshot) {
    SaveApplyResult result;

    if (snapshot.format_version != SeedSaveFormatVersion) {
        result.warnings.push_back(
            "Unsupported Seed save format version " + std::to_string(snapshot.format_version) +
            "; this build supports version " + std::to_string(SeedSaveFormatVersion) + "."
        );
        result.entities_skipped = snapshot.entities.size();
        return result;
    }

    for (const SaveEntityState& saved : snapshot.entities) {
        const EntityId entity = scene.find_entity_by_persistent_id(saved.persistent_id);
        if (entity == InvalidEntity || !scene.is_alive(entity)) {
            ++result.entities_skipped;
            result.warnings.push_back(
                "Saved entity no longer exists in this scene: " + saved.persistent_id
            );
            continue;
        }

        const auto* persistent = scene.get_component<PersistentComponent>(entity);
        if (persistent != nullptr && !persistent->enabled) {
            ++result.entities_skipped;
            result.warnings.push_back(
                "Saved entity is disabled for persistence and was not restored: " +
                entity_label(scene, entity)
            );
            continue;
        }

        std::size_t applied_for_entity = 0;

        if (saved.transform.has_value()) {
            if (auto* transform = scene.get_component<TransformComponent>(entity)) {
                transform->position = saved.transform->position;
                transform->rotation_degrees = saved.transform->rotation_degrees;
                transform->scale = saved.transform->scale;
                ++result.properties_applied;
                ++applied_for_entity;
            } else {
                result.warnings.push_back(
                    "Saved Transform could not be restored because the component was removed: " +
                    entity_label(scene, entity)
                );
            }
        }

        if (saved.health.has_value()) {
            if (auto* health = scene.get_component<HealthComponent>(entity)) {
                const float maximum = std::max(0.0f, health->maximum);
                health->current = std::clamp(saved.health->current, 0.0f, maximum);
                ++result.properties_applied;
                ++applied_for_entity;
            } else {
                result.warnings.push_back(
                    "Saved Health could not be restored because the component was removed: " +
                    entity_label(scene, entity)
                );
            }
        }

        if (saved.inventory.has_value()) {
            if (auto* inventory = scene.get_component<InventoryComponent>(entity)) {
                inventory->items = saved.inventory->items;
                ++result.properties_applied;
                ++applied_for_entity;
            } else {
                result.warnings.push_back(
                    "Saved Inventory could not be restored because the component was removed: " +
                    entity_label(scene, entity)
                );
            }
        }

        if (saved.quest.has_value()) {
            if (auto* quest = scene.get_component<QuestComponent>(entity)) {
                quest->status = static_cast<QuestStatus>(
                    std::clamp(saved.quest->status, 0, 2)
                );
                ++result.properties_applied;
                ++applied_for_entity;
            } else {
                result.warnings.push_back(
                    "Saved Quest could not be restored because the component was removed: " +
                    entity_label(scene, entity)
                );
            }
        }
        if (saved.wallet.has_value()) {
            if (auto* wallet = scene.get_component<WalletComponent>(entity)) {
                wallet->balance = saved.wallet->balance;
                wallet->lifetime_revenue = saved.wallet->lifetime_revenue;
                wallet->lifetime_expenses = saved.wallet->lifetime_expenses;
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.shop_inventory.has_value()) {
            if (auto* inventory = scene.get_component<ShopInventoryComponent>(entity)) {
                inventory->products.clear();
                for (const auto& product : saved.shop_inventory->products) {
                    inventory->products.push_back({
                        .product_id = product.product_id,
                        .quantity = product.quantity,
                        .shelf_capacity = product.shelf_capacity,
                    });
                }
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.shop_metrics.has_value()) {
            if (auto* metrics = scene.get_component<ShopMetricsComponent>(entity)) {
                metrics->tracked_day = saved.shop_metrics->tracked_day;
                metrics->customers_served_today = saved.shop_metrics->customers_served_today;
                metrics->orders_completed_today = saved.shop_metrics->orders_completed_today;
                metrics->products_sold_today = saved.shop_metrics->products_sold_today;
                metrics->daily_revenue = saved.shop_metrics->daily_revenue;
                metrics->daily_satisfaction = saved.shop_metrics->daily_satisfaction;
                metrics->satisfaction_samples = saved.shop_metrics->satisfaction_samples;
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.shop_clock.has_value()) {
            if (auto* clock = scene.get_component<SimulationClockComponent>(entity)) {
                clock->day = std::max(1, saved.shop_clock->day);
                clock->hour = std::clamp(saved.shop_clock->hour, 0.0f, 24.0f);
                clock->time_scale = std::max(0.0f, saved.shop_clock->time_scale);
                clock->paused = saved.shop_clock->paused;
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.shop_hours.has_value()) {
            if (auto* hours = scene.get_component<ShopHoursComponent>(entity)) {
                hours->opening_hour = std::clamp(saved.shop_hours->opening_hour, 0.0f, 24.0f);
                hours->closing_hour = std::clamp(saved.shop_hours->closing_hour, 0.0f, 24.0f);
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.shop_upgrade.has_value()) {
            if (auto* upgrade = scene.get_component<ShopUpgradeComponent>(entity)) {
                upgrade->level = std::clamp(saved.shop_upgrade->level, 1, upgrade->max_level);
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.staff.has_value()) {
            if (auto* staff = scene.get_component<StaffWorkerComponent>(entity)) {
                staff->employed = saved.staff->employed;
                staff->last_paid_day = std::max(1, saved.staff->last_paid_day);
                staff->skill_level = std::clamp(saved.staff->skill_level, 1, staff->max_skill_level);
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.customer_behavior.has_value()) {
            if (auto* behavior = scene.get_component<CustomerBehaviorComponent>(entity)) {
                behavior->enabled = saved.customer_behavior->enabled;
                behavior->timer_seconds = std::max(0.0f, saved.customer_behavior->timer_seconds);
                behavior->state = saved.customer_behavior->state;
                behavior->order_sequence = std::max(1, saved.customer_behavior->order_sequence);
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }
        if (saved.customer_order.has_value()) {
            if (auto* order = scene.get_component<CustomerOrderComponent>(entity)) {
                order->order_id = saved.customer_order->order_id;
                order->customer_name = saved.customer_order->customer_name;
                order->fulfilled = saved.customer_order->fulfilled;
                order->delivered = saved.customer_order->delivered;
                order->lines.clear();
                for (const auto& line : saved.customer_order->lines) {
                    order->lines.push_back({
                        .product_id = line.product_id,
                        .quantity = line.quantity,
                        .fulfilled_quantity = line.fulfilled_quantity,
                        .unit_price = line.unit_price,
                    });
                }
                ++result.properties_applied;
                ++applied_for_entity;
            }
        }

        if (applied_for_entity > 0) ++result.entities_applied;
        else ++result.entities_skipped;
    }

    return result;
}

bool SaveGameSystem::write_file(
    const SaveGameSnapshot& snapshot,
    const std::filesystem::path& file,
    std::string* error
) {
    try {
        if (snapshot.format_version != SeedSaveFormatVersion) {
            set_error(error, "Refusing to write unsupported Seed save format version.");
            return false;
        }

        if (!file.parent_path().empty()) {
            std::filesystem::create_directories(file.parent_path());
        }

        nlohmann::json entities = nlohmann::json::array();
        for (const SaveEntityState& state : snapshot.entities) {
            nlohmann::json components = nlohmann::json::object();

            if (state.transform.has_value()) {
                components["Transform"] = {
                    {"position", vec3_to_json(state.transform->position)},
                    {"rotation", vec3_to_json(state.transform->rotation_degrees)},
                    {"scale", vec3_to_json(state.transform->scale)},
                };
            }
            if (state.health.has_value()) {
                components["Health"] = {{"current", state.health->current}};
            }
            if (state.inventory.has_value()) {
                components["Inventory"] = {{"items", state.inventory->items}};
            }
            if (state.quest.has_value()) {
                components["Quest"] = {
                    {"quest_id", state.quest->quest_id},
                    {"status", state.quest->status},
                };
            }
            if (state.wallet.has_value()) {
                components["Wallet"] = {
                    {"balance", state.wallet->balance},
                    {"lifetime_revenue", state.wallet->lifetime_revenue},
                    {"lifetime_expenses", state.wallet->lifetime_expenses},
                };
            }
            if (state.shop_inventory.has_value()) {
                nlohmann::json products = nlohmann::json::array();
                for (const auto& product : state.shop_inventory->products) {
                    products.push_back({
                        {"product_id", product.product_id},
                        {"quantity", product.quantity},
                        {"shelf_capacity", product.shelf_capacity},
                    });
                }
                components["ShopInventory"] = {{"products", std::move(products)}};
            }
            if (state.shop_metrics.has_value()) {
                components["ShopMetrics"] = {
                    {"tracked_day", state.shop_metrics->tracked_day},
                    {"customers_served_today", state.shop_metrics->customers_served_today},
                    {"orders_completed_today", state.shop_metrics->orders_completed_today},
                    {"products_sold_today", state.shop_metrics->products_sold_today},
                    {"daily_revenue", state.shop_metrics->daily_revenue},
                    {"daily_satisfaction", state.shop_metrics->daily_satisfaction},
                    {"satisfaction_samples", state.shop_metrics->satisfaction_samples},
                };
            }
            if (state.shop_clock.has_value()) {
                components["SimulationClock"] = {
                    {"day", state.shop_clock->day},
                    {"hour", state.shop_clock->hour},
                    {"time_scale", state.shop_clock->time_scale},
                    {"paused", state.shop_clock->paused},
                };
            }
            if (state.shop_hours.has_value()) {
                components["ShopHours"] = {
                    {"opening_hour", state.shop_hours->opening_hour},
                    {"closing_hour", state.shop_hours->closing_hour},
                };
            }
            if (state.shop_upgrade.has_value()) {
                components["ShopUpgrade"] = {{"level", state.shop_upgrade->level}};
            }
            if (state.staff.has_value()) {
                components["Staff"] = {
                    {"employed", state.staff->employed},
                    {"last_paid_day", state.staff->last_paid_day},
                    {"skill_level", state.staff->skill_level},
                };
            }
            if (state.customer_behavior.has_value()) {
                components["CustomerBehavior"] = {
                    {"enabled", state.customer_behavior->enabled},
                    {"timer_seconds", state.customer_behavior->timer_seconds},
                    {"state", state.customer_behavior->state},
                    {"order_sequence", state.customer_behavior->order_sequence},
                };
            }
            if (state.customer_order.has_value()) {
                nlohmann::json lines = nlohmann::json::array();
                for (const auto& line : state.customer_order->lines) {
                    lines.push_back({
                        {"product_id", line.product_id},
                        {"quantity", line.quantity},
                        {"fulfilled_quantity", line.fulfilled_quantity},
                        {"unit_price", line.unit_price},
                    });
                }
                components["CustomerOrder"] = {
                    {"order_id", state.customer_order->order_id},
                    {"customer_name", state.customer_order->customer_name},
                    {"lines", std::move(lines)},
                    {"fulfilled", state.customer_order->fulfilled},
                    {"delivered", state.customer_order->delivered},
                };
            }

            entities.push_back({
                {"id", state.persistent_id},
                {"state", std::move(components)},
            });
        }

        const nlohmann::json document = {
            {"seed_format", "seedsave"},
            {"format_version", snapshot.format_version},
            {"slot", snapshot.slot},
            {"game_schema_version", snapshot.game_schema_version},
            {"entities", std::move(entities)},
        };

        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        if (!output) {
            set_error(error, "Could not open Seed save file for writing: " + file.string());
            return false;
        }
        output << document.dump(2) << '\n';
        if (!output) {
            set_error(error, "Could not finish writing Seed save file: " + file.string());
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

std::optional<SaveGameSnapshot> SaveGameSystem::read_file(
    const std::filesystem::path& file,
    std::string* error
) {
    try {
        std::ifstream input(file, std::ios::binary);
        if (!input) {
            set_error(error, "Could not open Seed save file: " + file.string());
            return std::nullopt;
        }

        nlohmann::json document;
        input >> document;
        if (document.value("seed_format", std::string{}) != "seedsave") {
            set_error(error, "File is not a Seed Save Game: " + file.string());
            return std::nullopt;
        }

        SaveGameSnapshot snapshot;
        snapshot.format_version = document.value("format_version", 0);
        if (snapshot.format_version != SeedSaveFormatVersion) {
            set_error(
                error,
                "Unsupported Seed save format version " + std::to_string(snapshot.format_version) +
                "; this build supports version " + std::to_string(SeedSaveFormatVersion) + "."
            );
            return std::nullopt;
        }

        snapshot.slot = document.value("slot", std::string{"slot_1"});
        snapshot.game_schema_version = std::max(1, document.value("game_schema_version", 1));

        for (const auto& entity_json : document.value("entities", nlohmann::json::array())) {
            SaveEntityState state;
            state.persistent_id = entity_json.value("id", std::string{});
            if (state.persistent_id.empty()) continue;

            const auto& components = entity_json.value("state", nlohmann::json::object());
            if (components.contains("Transform")) {
                const auto& value = components.at("Transform");
                SaveTransformState transform;
                transform.position = vec3_from_json(value.value("position", nlohmann::json::array()), {});
                transform.rotation_degrees = vec3_from_json(value.value("rotation", nlohmann::json::array()), {});
                transform.scale = vec3_from_json(value.value("scale", nlohmann::json::array()), {1.0f, 1.0f, 1.0f});
                state.transform = transform;
            }
            if (components.contains("Health")) {
                const auto& value = components.at("Health");
                state.health = SaveHealthState{.current = value.value("current", 100.0f)};
            }
            if (components.contains("Inventory")) {
                const auto& value = components.at("Inventory");
                SaveInventoryState inventory;
                if (value.contains("items")) {
                    inventory.items = value.at("items").get<std::unordered_map<std::string, int>>();
                }
                state.inventory = std::move(inventory);
            }
            if (components.contains("Quest")) {
                const auto& value = components.at("Quest");
                SaveQuestState quest;
                quest.quest_id = value.value("quest_id", std::string{});
                quest.status = value.value("status", 0);
                state.quest = std::move(quest);
            }
            if (components.contains("Wallet")) {
                const auto& value = components.at("Wallet");
                state.wallet = SaveWalletState{
                    .balance = value.value("balance", 0.0f),
                    .lifetime_revenue = value.value("lifetime_revenue", 0.0f),
                    .lifetime_expenses = value.value("lifetime_expenses", 0.0f),
                };
            }
            if (components.contains("ShopInventory")) {
                SaveShopInventoryState inventory;
                for (const auto& product : components.at("ShopInventory").value("products", nlohmann::json::array())) {
                    inventory.products.push_back({
                        .product_id = product.value("product_id", std::string{}),
                        .quantity = product.value("quantity", 0),
                        .shelf_capacity = product.value("shelf_capacity", 0),
                    });
                }
                state.shop_inventory = std::move(inventory);
            }
            if (components.contains("ShopMetrics")) {
                const auto& value = components.at("ShopMetrics");
                state.shop_metrics = SaveShopMetricsState{
                    .tracked_day = value.value("tracked_day", 1),
                    .customers_served_today = value.value("customers_served_today", 0),
                    .orders_completed_today = value.value("orders_completed_today", 0),
                    .products_sold_today = value.value("products_sold_today", 0),
                    .daily_revenue = value.value("daily_revenue", 0.0f),
                    .daily_satisfaction = value.value("daily_satisfaction", 100.0f),
                    .satisfaction_samples = value.value("satisfaction_samples", 0),
                };
            }
            if (components.contains("SimulationClock")) {
                const auto& value = components.at("SimulationClock");
                state.shop_clock = SaveShopClockState{
                    .day = value.value("day", 1),
                    .hour = value.value("hour", 8.0f),
                    .time_scale = value.value("time_scale", 1.0f),
                    .paused = value.value("paused", false),
                };
            }
            if (components.contains("ShopHours")) {
                const auto& value = components.at("ShopHours");
                state.shop_hours = SaveShopHoursState{
                    .opening_hour = value.value("opening_hour", 8.0f),
                    .closing_hour = value.value("closing_hour", 20.0f),
                };
            }
            if (components.contains("ShopUpgrade")) {
                state.shop_upgrade = SaveShopUpgradeState{
                    .level = components.at("ShopUpgrade").value("level", 1),
                };
            }
            if (components.contains("Staff")) {
                const auto& value = components.at("Staff");
                state.staff = SaveStaffState{
                    .employed = value.value("employed", true),
                    .last_paid_day = value.value("last_paid_day", 1),
                    .skill_level = value.value("skill_level", 1),
                };
            }
            if (components.contains("CustomerBehavior")) {
                const auto& value = components.at("CustomerBehavior");
                state.customer_behavior = SaveCustomerBehaviorState{
                    .enabled = value.value("enabled", true),
                    .timer_seconds = value.value("timer_seconds", 0.0f),
                    .state = value.value("state", std::string{"waiting"}),
                    .order_sequence = value.value("order_sequence", 1),
                };
            }
            if (components.contains("CustomerOrder")) {
                const auto& value = components.at("CustomerOrder");
                SaveCustomerOrderState order;
                order.order_id = value.value("order_id", std::string{});
                order.customer_name = value.value("customer_name", std::string{"Customer"});
                order.fulfilled = value.value("fulfilled", false);
                order.delivered = value.value("delivered", false);
                for (const auto& line : value.value("lines", nlohmann::json::array())) {
                    order.lines.push_back({
                        .product_id = line.value("product_id", std::string{}),
                        .quantity = line.value("quantity", 0),
                        .fulfilled_quantity = line.value("fulfilled_quantity", 0),
                        .unit_price = line.value("unit_price", 0.0f),
                    });
                }
                state.customer_order = std::move(order);
            }

            snapshot.entities.push_back(std::move(state));
        }

        return snapshot;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return std::nullopt;
    }
}

} // namespace seed
