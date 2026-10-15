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

    scene.for_each<PersistentComponent>([&](EntityId entity, const PersistentComponent& persistent) {
        if (!persistent.enabled || !scene.is_alive(entity)) return;

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
        if (persistent == nullptr || !persistent->enabled) {
            ++result.entities_skipped;
            result.warnings.push_back(
                "Saved entity is no longer marked Persistent and was not restored: " +
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

            snapshot.entities.push_back(std::move(state));
        }

        return snapshot;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return std::nullopt;
    }
}

} // namespace seed
