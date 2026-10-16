#include "seed/gameplay/GameplayRuntime.h"

#include "seed/animation/AnimationComponents.h"

#include "seed/gameplay/GameplayEvents.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/CombatSystem.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Hierarchy.h"
#include "seed/simulation/SimulationComponents.h"
#include "seed/simulation/SimulationSystem.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace seed {
namespace {

void set_action_state(
    InputAction action,
    bool down,
    bool& forward,
    bool& backward,
    bool& left,
    bool& right,
    bool& move_down,
    bool& move_up,
    bool& fast
) {
    switch (action) {
    case InputAction::MoveForward: forward = down; break;
    case InputAction::MoveBackward: backward = down; break;
    case InputAction::MoveLeft: left = down; break;
    case InputAction::MoveRight: right = down; break;
    case InputAction::MoveDown: move_down = down; break;
    case InputAction::MoveUp: move_up = down; break;
    case InputAction::Sprint: fast = down; break;
    default: break;
    }
}

float maximum_scale(const TransformComponent& transform) {
    return std::max({
        std::fabs(transform.scale.x),
        std::fabs(transform.scale.y),
        std::fabs(transform.scale.z),
    });
}

float yaw_from_direction(const Vec3& direction) {
    return degrees(std::atan2(direction.x, -direction.z));
}

void publish_locomotion(
    Scene& scene,
    EntityId entity,
    float speed,
    bool grounded,
    float vertical_velocity,
    bool sprinting
) {
    auto* animation = scene.get_component<AnimationPlayerComponent>(entity);
    if (animation == nullptr || !animation->locomotion_enabled) return;
    animation->locomotion_speed = speed;
    animation->locomotion_grounded = grounded;
    animation->locomotion_vertical_velocity = vertical_velocity;
    animation->locomotion_sprinting = sprinting;
}

void update_customers(
    Scene& scene,
    double delta_seconds,
    GameplayEventBus* event_bus,
    std::string& status_message
) {
    const float dt = static_cast<float>(std::max(0.0, delta_seconds));
    EntityId shop_entity = InvalidEntity;
    ShopInventoryComponent* inventory = nullptr;
    WalletComponent* wallet = nullptr;
    ShopHoursComponent* hours = nullptr;
    SimulationClockComponent* clock = nullptr;
    CustomerCapacityComponent* capacity = nullptr;
    CustomerSpawnerComponent* spawner = nullptr;

    scene.for_each<ShopInventoryComponent, WalletComponent>(
        [&](EntityId entity, ShopInventoryComponent& candidate_inventory, WalletComponent& candidate_wallet) {
            if (shop_entity != InvalidEntity) return;
            shop_entity = entity;
            inventory = &candidate_inventory;
            wallet = &candidate_wallet;
            hours = scene.get_component<ShopHoursComponent>(entity);
            clock = scene.get_component<SimulationClockComponent>(entity);
            capacity = scene.get_component<CustomerCapacityComponent>(entity);
            spawner = scene.get_component<CustomerSpawnerComponent>(entity);
        }
    );
    if (inventory == nullptr || wallet == nullptr) return;

    const auto record_satisfaction = [&](float satisfaction) {
        auto* metrics = scene.get_component<ShopMetricsComponent>(shop_entity);
        if (metrics == nullptr) return;
        const float clamped = std::clamp(satisfaction, 0.0f, 100.0f);
        metrics->daily_satisfaction =
            metrics->satisfaction_samples == 0
                ? clamped
                : (metrics->daily_satisfaction *
                   static_cast<float>(metrics->satisfaction_samples) +
                   clamped) /
                    static_cast<float>(metrics->satisfaction_samples + 1);
        ++metrics->satisfaction_samples;
    };

    const bool shop_open = hours == nullptr || clock == nullptr ||
        SimulationSystem::is_shop_open(*hours, *clock);

    ShopUpgradeComponent* upgrade = nullptr;
    scene.for_each<ShopUpgradeComponent>(
        [&](EntityId, ShopUpgradeComponent& candidate) {
            if (upgrade == nullptr) upgrade = &candidate;
        }
    );
    const int upgrade_levels = upgrade == nullptr
        ? 0
        : std::max(0, upgrade->level - 1);
    const int max_active_customers = std::max(
        1,
        (capacity == nullptr ? 4 : capacity->max_active_customers) +
            (capacity == nullptr ? 0 : upgrade_levels * capacity->customers_per_upgrade)
    );
    const int max_queue_length = std::max(
        1,
        capacity == nullptr ? 3 : capacity->max_queue_length
    );

    Vec3 queue_anchor{0.0f, 0.9f, -1.0f};
    float queue_spacing = 0.9f;
    bool authored_queue = false;
    scene.for_each<TransformComponent, TriggerVolumeComponent, InteractableComponent>(
        [&](EntityId, TransformComponent& transform, TriggerVolumeComponent&, InteractableComponent& interactable) {
            if (authored_queue || interactable.prompt != "Join Customer Queue") return;
            queue_anchor = transform.position;
            queue_anchor.y += 0.55f;
            authored_queue = true;
        }
    );

    std::vector<Vec3> checkout_anchors;
    scene.for_each<TransformComponent, CheckoutStationComponent>(
        [&](EntityId entity, TransformComponent& transform, CheckoutStationComponent& station) {
            if (!station.enabled) return;
            if (scene.has_component<TriggerVolumeComponent>(entity)) {
                checkout_anchors.push_back(transform.position + Vec3{0.0f, 0.45f, 0.0f});
            }
        }
    );
    if (checkout_anchors.empty()) {
        scene.for_each<TransformComponent, CheckoutStationComponent>(
            [&](EntityId, TransformComponent& transform, CheckoutStationComponent& station) {
                if (station.enabled) checkout_anchors.push_back(transform.position + Vec3{0.0f, 0.45f, 0.0f});
            }
        );
    }

    struct BrowseTarget {
        Vec3 position{};
        float dwell_seconds{2.0f};
        int priority{0};
        std::string product_id{};
    };
    std::vector<BrowseTarget> browse_targets;
    scene.for_each<TransformComponent, CustomerBrowsePointComponent>(
        [&](EntityId, TransformComponent& transform, CustomerBrowsePointComponent& browse_point) {
            if (!browse_point.enabled) return;
            browse_targets.push_back({
                transform.position + Vec3{0.0f, 0.45f, 0.0f},
                std::max(0.1f, browse_point.dwell_seconds),
                browse_point.priority,
                browse_point.product_id
            });
        }
    );
    std::sort(browse_targets.begin(), browse_targets.end(),
        [](const BrowseTarget& left, const BrowseTarget& right) {
            return left.priority > right.priority;
        });

    Vec3 exit_anchor{-4.0f, 0.9f, 4.0f};
    scene.for_each<TransformComponent, InteractableComponent>(
        [&](EntityId, TransformComponent& transform, InteractableComponent& interactable) {
            if (interactable.prompt == "Enter Shop") {
                exit_anchor = transform.position + Vec3{0.0f, -0.35f, 1.5f};
            }
        }
    );
    const int simulation_day = clock == nullptr ? 1 : std::max(1, clock->day);
    const float growth_multiplier = capacity == nullptr
        ? 1.0f
        : std::max(0.1f, capacity->arrival_rate_multiplier) *
            (1.0f + std::max(0, simulation_day - 1) *
                std::max(0.0f, capacity->daily_growth_rate));

    const auto shift_hours = [](const StaffWorkerComponent& worker) {
        const float start = std::clamp(worker.shift_start_hour, 0.0f, 24.0f);
        const float end = std::clamp(worker.shift_end_hour, 0.0f, 24.0f);
        if (std::fabs(start - end) < 0.001f) return 24.0f;
        return start < end ? end - start : (24.0f - start) + end;
    };
    const auto worker_on_shift = [&](const StaffWorkerComponent& worker) {
        if (clock == nullptr) return true;
        const float start = std::clamp(worker.shift_start_hour, 0.0f, 24.0f);
        const float end = std::clamp(worker.shift_end_hour, 0.0f, 24.0f);
        if (std::fabs(start - end) < 0.001f) return true;
        if (start < end) {
            return clock->hour >= start && clock->hour < end;
        }
        return clock->hour >= start || clock->hour < end;
    };

    if (clock != nullptr) {
        scene.for_each<StaffWorkerComponent>(
            [&](EntityId, StaffWorkerComponent& worker) {
                if (!worker.enabled || !worker.employed || worker.last_paid_day >= clock->day) return;
                const int unpaid_days = std::max(
                    1, clock->day - std::max(1, worker.last_paid_day)
                );
                const float wages = std::max(0.0f, worker.wage_per_hour) *
                    shift_hours(worker) * static_cast<float>(unpaid_days);
                wallet->balance -= wages;
                wallet->lifetime_expenses += wages;
                worker.last_paid_day = clock->day;
                status_message = wages > 0.0f
                    ? "Paid staff wages: " + std::to_string(wages)
                    : status_message;
            }
        );
    }

    int checkout_capacity = 0;
    std::vector<float> checkout_service_times;
    scene.for_each<CheckoutStationComponent>(
        [&](EntityId station_entity, CheckoutStationComponent& station) {
            if (!station.enabled) return;

            const auto* staffing = scene.get_component<StationStaffingComponent>(
                station_entity
            );
            int staffed_workers = 0;
            float combined_work_speed = 0.0f;
            scene.for_each<StaffWorkerComponent>(
                [&](EntityId, StaffWorkerComponent& worker) {
                    if (!worker.enabled ||
                        !worker.employed ||
                        !worker_on_shift(worker) ||
                        worker.assigned_station_persistent_id !=
                            scene.entity_persistent_id(station_entity)) {
                        return;
                    }
                    if (staffing != nullptr &&
                        !staffing->required_role.empty() &&
                        worker.role != staffing->required_role) {
                        return;
                    }
                    ++staffed_workers;
                    combined_work_speed += std::max(0.1f, worker.work_speed) *
                        (1.0f + static_cast<float>(std::max(0, worker.skill_level - 1)) * 0.1f);
                }
            );

            if (staffing != nullptr &&
                staffing->requires_staff &&
                staffed_workers < std::max(1, staffing->required_workers)) {
                return;
            }

            const float service_speed = combined_work_speed > 0.0f
                ? combined_work_speed
                : 1.0f;
            const int slots = std::max(1, station.service_slots);
            checkout_capacity += slots;
            for (int slot = 0; slot < slots; ++slot) {
                checkout_service_times.push_back(
                    std::max(0.1f, station.service_time_seconds) / service_speed
                );
            }
        }
    );
    if (checkout_service_times.empty()) {
        checkout_service_times.push_back(1.0f);
    }
    checkout_capacity = static_cast<int>(checkout_service_times.size());

    if (spawner != nullptr && spawner->enabled && shop_open) {
        spawner->timer_seconds += dt;
        int customer_count = 0;
        EntityId template_customer = InvalidEntity;
        scene.for_each<CustomerBehaviorComponent, CustomerOrderComponent>(
            [&](EntityId entity, CustomerBehaviorComponent&, CustomerOrderComponent&) {
                ++customer_count;
                if (template_customer == InvalidEntity &&
                    entity != shop_entity &&
                    (spawner->customer_template_persistent_id.empty() ||
                        entity == scene.find_entity_by_persistent_id(
                            spawner->customer_template_persistent_id))) {
                    template_customer = entity;
                }
            }
        );

        if (customer_count < std::max(1, spawner->max_customers) &&
            spawner->timer_seconds >= std::max(0.1f, spawner->spawn_interval_seconds)) {
            if (template_customer != InvalidEntity) {
                const std::string customer_name =
                    spawner->customer_name_prefix + " " +
                    std::to_string(std::max(1, spawner->next_customer_number));
                const EntityId spawned = scene.copy_entity_from(
                    scene,
                    template_customer,
                    customer_name
                );
                if (auto* behavior = scene.get_component<CustomerBehaviorComponent>(spawned)) {
                    behavior->state = "waiting";
                    behavior->timer_seconds = 0.0f;
                    behavior->order_sequence = 1;
                }
                if (auto* order = scene.get_component<CustomerOrderComponent>(spawned)) {
                    order->order_id = customer_name;
                    order->customer_name = customer_name;
                    order->lines.clear();
                    order->fulfilled = false;
                    order->delivered = false;
                }
                if (auto* transform = scene.get_component<TransformComponent>(spawned)) {
                    const float lane = static_cast<float>(customer_count % 3);
                    transform->position = {4.0f, 0.9f, -3.5f - lane * 0.9f};
                }
                ++spawner->next_customer_number;
                spawner->timer_seconds = 0.0f;
                status_message = customer_name + " entered the shop.";
                if (event_bus != nullptr) {
                    event_bus->publish({
                        spawned,
                        GameplayEventType::Interaction,
                        "CustomerSpawned",
                        0.0f,
                        shop_entity
                    });
                }
            } else {
                spawner->timer_seconds = 0.0f;
            }
        }
    }

    std::vector<EntityId> queued_customers;
    int active_customer_count = 0;
    scene.for_each<CustomerBehaviorComponent, CustomerOrderComponent>(
        [&](EntityId entity, CustomerBehaviorComponent& customer, CustomerOrderComponent&) {
            if (!customer.enabled) return;
            if (customer.state == "arriving" ||
                customer.state == "browsing" ||
                customer.state == "queued" ||
                customer.state == "shopping" ||
                customer.state == "awaiting_payment") {
                ++active_customer_count;
            }
            if (customer.state == "arriving" ||
                customer.state == "queued" ||
                customer.state == "shopping" ||
                customer.state == "awaiting_payment") {
                queued_customers.push_back(entity);
            }
        }
    );
    std::sort(queued_customers.begin(), queued_customers.end());

    scene.for_each<CustomerBehaviorComponent, CustomerOrderComponent>(
        [&](EntityId customer_entity, CustomerBehaviorComponent& customer,
            CustomerOrderComponent& order) {
            if (!customer.enabled) return;

            const auto queue_it = std::find(
                queued_customers.begin(),
                queued_customers.end(),
                customer_entity
            );
            const std::size_t queue_position = queue_it == queued_customers.end()
                ? queued_customers.size()
                : static_cast<std::size_t>(queue_it - queued_customers.begin());

            auto move_customer_toward = [&](const Vec3& target) {
                auto* transform = scene.get_component<TransformComponent>(customer_entity);
                if (transform == nullptr) return true;
                Vec3 offset = target - transform->position;
                offset.y = 0.0f;
                const float distance = length(offset);
                if (distance <= 0.15f) {
                    transform->position.y = target.y;
                    return true;
                }
                const Vec3 direction = normalize(offset);
                const float step = dt * 2.5f;
                if (step >= distance) {
                    transform->position += direction * distance;
                    transform->position.y = target.y;
                    transform->rotation_degrees.y = yaw_from_direction(direction);
                    return true;
                }
                transform->position += direction * step;
                transform->rotation_degrees.y = yaw_from_direction(direction);
                return false;
            };

            customer.timer_seconds += dt;
            if (customer.state == "queued" &&
                customer.queue_patience_seconds > 0.0f &&
                customer.timer_seconds >= customer.queue_patience_seconds) {
                customer.state = "leaving";
                customer.timer_seconds = 0.0f;
                record_satisfaction(
                    100.0f - std::max(0.0f, customer.queue_abandon_satisfaction_penalty)
                );
                status_message = order.customer_name +
                    " left because the queue was too long.";
                if (event_bus != nullptr) {
                    event_bus->publish({
                        customer_entity,
                        GameplayEventType::Interaction,
                        "CustomerLeftQueue",
                        0.0f,
                        shop_entity
                    });
                }
            }
            if (!shop_open) {
                if (customer.state == "arriving" ||
                    customer.state == "browsing" ||
                    customer.state == "queued" ||
                    customer.state == "shopping" ||
                    customer.state == "awaiting_payment") {
                    customer.state = "leaving";
                    customer.timer_seconds = 0.0f;
                    status_message = order.customer_name + " is leaving because the shop is closed.";
                }
                if (customer.state == "leaving") {
                    if (!move_customer_toward(exit_anchor)) return;
                    if (customer.timer_seconds >= customer.leave_duration_seconds) {
                        customer.timer_seconds = 0.0f;
                        customer.state = "waiting";
                        ++customer.order_sequence;
                    }
                }
                return;
            }
            if (customer.state == "waiting") {
                if (active_customer_count >= max_active_customers) return;
                const float effective_arrival_interval =
                    customer.arrival_interval_seconds / growth_multiplier;
                if (customer.timer_seconds < effective_arrival_interval) return;

                customer.timer_seconds = 0.0f;
                customer.state = "arriving";
                order.order_id = "customer-" + std::to_string(customer.order_sequence);
                order.customer_name = scene.entity_name(customer_entity);
                order.lines.clear();

                std::vector<ProductDefinitionComponent*> available_products;
                scene.for_each<ProductDefinitionComponent>(
                    [&](EntityId, ProductDefinitionComponent& product) {
                        const auto stock = std::find_if(
                            inventory->products.begin(),
                            inventory->products.end(),
                            [&](const ProductStock& candidate) {
                                return candidate.product_id == product.product_id;
                            }
                        );
                        if (stock != inventory->products.end() && stock->quantity > 0) {
                            available_products.push_back(&product);
                        }
                    }
                );
                const bool created_order = !available_products.empty();
                if (created_order) {
                    auto demand_score = [](const ProductDefinitionComponent& product) {
                        const float reference_price = std::max(
                            0.01f, product.wholesale_price * 2.0f
                        );
                        const float markup_penalty = std::max(
                            0.0f, product.retail_price / reference_price - 1.0f
                        );
                        return std::max(0.0f, product.demand_weight) /
                            (1.0f + std::max(0.0f, product.price_sensitivity) *
                                markup_penalty);
                    };

                    float total_demand = 0.0f;
                    for (const auto* product : available_products) {
                        total_demand += demand_score(*product);
                    }
                    const bool use_uniform_demand = total_demand <= 0.0f;
                    if (use_uniform_demand) {
                        total_demand = static_cast<float>(available_products.size());
                    }

                    const auto customer_seed =
                        static_cast<std::size_t>(customer_entity) * 17u +
                        static_cast<std::size_t>(customer.order_sequence) * 31u;
                    const float sample =
                        static_cast<float>(customer_seed % 1000u) / 1000.0f *
                        total_demand;

                    const ProductDefinitionComponent* selected_product =
                        available_products.back();
                    float cursor = 0.0f;
                    for (const auto* product : available_products) {
                        cursor += use_uniform_demand ? 1.0f : demand_score(*product);
                        if (cursor > sample) {
                            selected_product = product;
                            break;
                        }
                    }

                    const auto& product = *selected_product;
                    order.lines.push_back({
                        product.product_id,
                        1,
                        0,
                        product.retail_price
                    });
                }
                order.fulfilled = false;
                order.delivered = false;
                if (!created_order) {
                    customer.state = "waiting";
                    status_message = "Customer arrived, but the shop has no products.";
                    return;
                }

                if (!browse_targets.empty()) {
                    customer.state = "browsing";
                    status_message = order.customer_name + " is browsing.";
                } else {
                    customer.state = "arriving";
                    status_message = order.customer_name + " is shopping.";
                }
                if (event_bus != nullptr) {
                    event_bus->publish({
                        customer_entity,
                        GameplayEventType::Interaction,
                        "CustomerArrived",
                        0.0f,
                        shop_entity
                    });
                }
                return;
            }

            if (customer.state == "browsing") {
                if (browse_targets.empty()) {
                    customer.state = "arriving";
                    customer.timer_seconds = 0.0f;
                    return;
                }
                const BrowseTarget* browse_target = nullptr;
                if (!order.lines.empty()) {
                    for (const auto& candidate : browse_targets) {
                        if (!candidate.product_id.empty() &&
                            candidate.product_id == order.lines.front().product_id) {
                            browse_target = &candidate;
                            break;
                        }
                    }
                }
                if (browse_target == nullptr) {
                    std::vector<const BrowseTarget*> generic_targets;
                    for (const auto& candidate : browse_targets) {
                        if (candidate.product_id.empty()) {
                            generic_targets.push_back(&candidate);
                        }
                    }
                    if (!generic_targets.empty()) {
                        const std::size_t browse_index =
                            (static_cast<std::size_t>(customer_entity) +
                             static_cast<std::size_t>(std::max(0, customer.order_sequence - 1))) %
                            generic_targets.size();
                        browse_target = generic_targets[browse_index];
                    } else {
                        browse_target = &browse_targets[
                            (static_cast<std::size_t>(customer_entity) +
                             static_cast<std::size_t>(std::max(0, customer.order_sequence - 1))) %
                            browse_targets.size()
                        ];
                    }
                }
                if (!move_customer_toward(browse_target->position)) return;
                if (customer.timer_seconds < browse_target->dwell_seconds) return;
                customer.state = "arriving";
                customer.timer_seconds = 0.0f;
                status_message = order.customer_name + " is heading to checkout.";
                return;
            }

            if (customer.state == "arriving") {
                if (queue_position >= static_cast<std::size_t>(max_queue_length)) {
                    customer.state = "waiting";
                    customer.timer_seconds = 0.0f;
                    return;
                }
                const float queue_offset = static_cast<float>(queue_position) * queue_spacing;
                const Vec3 queue_target{
                    queue_anchor.x,
                    queue_anchor.y,
                    queue_anchor.z - queue_offset
                };
                if (!move_customer_toward(queue_target)) return;
                customer.state = "queued";
                customer.timer_seconds = 0.0f;
                status_message = order.customer_name + " joined the queue.";
                return;
            }

            if (customer.state == "queued") {
                if (queue_position >= static_cast<std::size_t>(checkout_capacity)) return;
                const int service_slot = static_cast<int>(queue_position);
                const float lane = static_cast<float>(service_slot % 3) - 1.0f;
                const float row = static_cast<float>(service_slot / 3);
                Vec3 checkout_target{
                    lane * 1.4f,
                    0.9f,
                    -0.2f - row * 0.9f
                };
                if (!checkout_anchors.empty()) {
                    checkout_target = checkout_anchors[
                        static_cast<std::size_t>(service_slot) % checkout_anchors.size()
                    ];
                    checkout_target.x += lane * 0.35f;
                    checkout_target.z += row * 0.35f;
                }
                if (!move_customer_toward(checkout_target)) return;
                customer.state = "shopping";
                customer.shopping_duration_seconds = checkout_service_times[
                    std::min(
                        static_cast<std::size_t>(service_slot),
                        checkout_service_times.size() - 1
                    )
                ];
                customer.timer_seconds = 0.0f;
                status_message = order.customer_name + " reached checkout.";
                return;
            }

            if (customer.state == "shopping") {
                if (customer.timer_seconds < customer.shopping_duration_seconds) return;
                customer.timer_seconds = 0.0f;
                customer.state = "awaiting_payment";
                status_message = order.customer_name + " is ready for checkout.";
                if (event_bus != nullptr) {
                    event_bus->publish({
                        customer_entity,
                        GameplayEventType::Interaction,
                        "CustomerReadyForCheckout",
                        SimulationSystem::order_total(order),
                        shop_entity
                    });
                }
                return;
            }

            if (customer.state == "awaiting_payment") {
                return;
            }

            if (customer.state == "leaving") {
                if (!move_customer_toward(exit_anchor)) return;
                if (customer.timer_seconds >= customer.leave_duration_seconds) {
                    customer.timer_seconds = 0.0f;
                    customer.state = "waiting";
                    ++customer.order_sequence;
                }
            }
        }
    );
}

} // namespace

bool GameplayRuntime::start(Scene& scene, EntityId player_entity) {
    stop();

    if (player_entity == InvalidEntity || !scene.is_alive(player_entity)) {
        m_status_message = "Play Mode could not find a valid player.";
        return false;
    }

    if (!scene.has_component<TransformComponent>(player_entity)) {
        scene.add_component<TransformComponent>(player_entity);
    }
    if (!scene.has_component<AttackComponent>(player_entity)) {
        scene.add_component<AttackComponent>(player_entity);
    }
    if (const auto* spawn = scene.get_component<TransformComponent>(player_entity)) {
        m_spawn_position = spawn->position;
        m_spawn_rotation = spawn->rotation_degrees;
    }
    if (!scene.has_component<PlayerControllerComponent>(player_entity)) {
        scene.add_component<PlayerControllerComponent>(player_entity);
    }
    if (!scene.has_component<InventoryComponent>(player_entity)) {
        scene.add_component<InventoryComponent>(player_entity);
    }
    if (!scene.has_component<AnimationPlayerComponent>(player_entity)) {
        AnimationPlayerComponent animation;
        animation.locomotion_enabled = true;
        scene.add_component<AnimationPlayerComponent>(player_entity, std::move(animation));
    }

    m_player_entity = player_entity;
    m_camera_entity = resolve_player_camera(scene);
    m_active = true;

    const auto* controller = scene.get_component<PlayerControllerComponent>(player_entity);
    if (controller != nullptr && controller->view_mode == PlayerViewMode::ThirdPerson) {
        if (m_camera_entity != InvalidEntity) {
            const TransformComponent camera_world = world_transform(scene, m_camera_entity);
            m_third_person_yaw = camera_world.rotation_degrees.y;
            const float min_pitch = std::min(controller->camera_min_pitch, controller->camera_max_pitch);
            const float max_pitch = std::max(controller->camera_min_pitch, controller->camera_max_pitch);
            m_third_person_pitch = std::clamp(camera_world.rotation_degrees.x, min_pitch, max_pitch);
            update_third_person_camera(scene);
            m_status_message = "Third-Person Play Mode running.";
        } else {
            m_status_message = "Third-Person Player is missing its game Camera.";
        }
    } else {
        m_status_message = "First-Person Play Mode running.";
    }

    if (scene.has_component<CharacterBodyComponent>(player_entity)) {
        PhysicsSystem::initialize_character(scene, player_entity, m_character_state);
    }

    scene.for_each<TransformComponent, DoorComponent>(
        [&](EntityId entity, TransformComponent& transform, DoorComponent& door) {
            DoorRuntimeState state;
            state.closed_position = transform.position;
            state.closed_rotation = transform.rotation_degrees;
            state.progress = door.starts_open ? 1.0f : 0.0f;
            state.target_open = door.starts_open;
            m_doors.emplace(entity, state);
            apply_door_transform(scene, entity, state);
        }
    );

    refresh_interaction_target(scene);
    return true;
}

void GameplayRuntime::stop() {
    m_active = false;
    m_player_entity = InvalidEntity;
    m_camera_entity = InvalidEntity;
    m_interaction_target = InvalidEntity;
    m_forward = false;
    m_backward = false;
    m_left = false;
    m_right = false;
    m_down = false;
    m_up = false;
    m_fast = false;
    m_looking = false;
    m_has_mouse_position = false;
    m_interact_requested = false;
    m_jump_requested = false;
    m_attack_requested = false;
    m_pending_look_x = 0.0f;
    m_pending_look_y = 0.0f;
    m_third_person_yaw = 0.0f;
    m_third_person_pitch = -12.0f;
    m_footstep_elapsed = 0.0f;
    m_respawn_elapsed = 0.0f;
    m_spawn_position = {};
    m_spawn_rotation = {};
    m_character_state = {};
    m_doors.clear();
    m_enemy_attack_elapsed.clear();
    m_trigger_inside.clear();
    m_begin_play_published = false;
    m_victory_announced = false;
    m_dialogue_entity = InvalidEntity;
    m_dialogue_line_index = 0;
    m_dialogue_active = false;
    m_dialogue_choice_requested = -1;
    m_interaction_prompt.clear();
    m_status_message.clear();
}

void GameplayRuntime::handle_event(const PlatformEvent& event) {
    if (!m_active) {
        return;
    }

    if (event.type == PlatformEventType::Key) {
        const bool down = event.button_state != ButtonState::Released;
        const auto actions = m_input_map.actions_for(event.key);
        for (const InputAction action : actions) {
            set_action_state(
                action,
                down,
                m_forward,
                m_backward,
                m_left,
                m_right,
                m_down,
                m_up,
                m_fast
            );

            if (action == InputAction::Interact && event.button_state == ButtonState::Pressed) {
                m_interact_requested = true;
            }
            if (action == InputAction::Attack && event.button_state == ButtonState::Pressed) {
                m_attack_requested = true;
            }
            if (event.button_state == ButtonState::Pressed) {
                switch (action) {
                case InputAction::DialogueChoice1: m_dialogue_choice_requested = 0; break;
                case InputAction::DialogueChoice2: m_dialogue_choice_requested = 1; break;
                case InputAction::DialogueChoice3: m_dialogue_choice_requested = 2; break;
                default: break;
                }
            }
            if (action == InputAction::Jump && event.button_state == ButtonState::Pressed) {
                m_jump_requested = true;
            }
        }
    }

    if (event.type == PlatformEventType::MouseButton &&
        event.mouse_button == MouseButton::Right) {
        m_looking = event.button_state != ButtonState::Released;
        m_has_mouse_position = false;
    }

    if (event.type == PlatformEventType::MouseMove && m_looking) {
        if (m_has_mouse_position) {
            m_pending_look_x += event.x - m_last_mouse_x;
            m_pending_look_y += event.y - m_last_mouse_y;
        }
        m_last_mouse_x = event.x;
        m_last_mouse_y = event.y;
        m_has_mouse_position = true;
    }

    if (event.type == PlatformEventType::WindowFocusChanged && !event.focused) {
        m_forward = false;
        m_backward = false;
        m_left = false;
        m_right = false;
        m_down = false;
        m_up = false;
        m_fast = false;
        m_looking = false;
        m_has_mouse_position = false;
        m_jump_requested = false;
        m_dialogue_choice_requested = -1;
    }
}

void GameplayRuntime::update(Scene& scene, double delta_seconds, GameplayEventBus* event_bus) {
    if (!m_active || !scene.is_alive(m_player_entity)) {
        return;
    }

    if (auto* health = scene.get_component<HealthComponent>(m_player_entity);
        health != nullptr && health->current <= 0.0f) {
        m_respawn_elapsed += static_cast<float>(std::max(0.0, delta_seconds));
        m_status_message = "You were defeated. Respawning...";
        if (m_respawn_elapsed >= 2.0f) {
            health->current = std::max(0.0f, health->maximum);
            if (auto* transform = scene.get_component<TransformComponent>(m_player_entity)) {
                transform->position = m_spawn_position;
                transform->rotation_degrees = m_spawn_rotation;
            }
            m_respawn_elapsed = 0.0f;
            m_status_message = "Respawned.";
        }
        return;
    }
    m_respawn_elapsed = 0.0f;

    const float dt = static_cast<float>(std::max(0.0, delta_seconds));
    scene.for_each<SimulationClockComponent>(
        [&](EntityId shop_entity, SimulationClockComponent& clock) {
            const int previous_day = clock.day;
            SimulationSystem::advance_clock(clock, dt);
            if (clock.day == previous_day) return;

            if (auto* metrics = scene.get_component<ShopMetricsComponent>(shop_entity)) {
                metrics->tracked_day = clock.day;
                metrics->customers_served_today = 0;
                metrics->orders_completed_today = 0;
                metrics->products_sold_today = 0;
                metrics->daily_revenue = 0.0f;
                metrics->daily_satisfaction = 100.0f;
                metrics->satisfaction_samples = 0;
            }
            m_status_message = "Day " + std::to_string(clock.day) + " started.";
        }
    );

    if (!m_begin_play_published && event_bus != nullptr) {
        event_bus->publish({
            .source_entity = m_player_entity,
            .type = GameplayEventType::BeginPlay,
            .name = "BeginPlay",
        });
        m_begin_play_published = true;
    }

    consume_shop_events(scene, event_bus);
    update_player(scene, delta_seconds, event_bus);
    if (m_attack_requested) {
        m_attack_requested = false;
        perform_attack(scene, event_bus);
    }
    update_doors(scene, delta_seconds);
    update_quests(scene);
    refresh_interaction_target(scene);

    update_enemies(scene, delta_seconds, event_bus);
    update_customers(scene, delta_seconds, event_bus, m_status_message);
    update_triggers(scene, event_bus);

    if (m_dialogue_choice_requested >= 0) {
        const int choice_index = m_dialogue_choice_requested;
        m_dialogue_choice_requested = -1;
        advance_dialogue(scene, choice_index);
        refresh_interaction_target(scene);
    } else if (m_interact_requested) {
        perform_interaction(scene, event_bus);
        m_interact_requested = false;
        refresh_interaction_target(scene);
    }
}


void GameplayRuntime::consume_shop_events(Scene& scene, GameplayEventBus* event_bus) {
    if (event_bus == nullptr) return;

    const std::vector<GameplayEvent> pending = event_bus->pending();
    for (const GameplayEvent& event : pending) {
        if (event.target_entity != m_player_entity) continue;

        if (event.name == "CheckoutEntered" &&
            scene.is_alive(event.source_entity) &&
            scene.has_component<CheckoutStationComponent>(event.source_entity)) {
            const EntityId previous_target = m_interaction_target;
            m_interaction_target = event.source_entity;
            perform_interaction(scene, event_bus);
            m_interaction_target = previous_target;
        } else if (event.name == "CustomerQueueEntered") {
            m_status_message = "Customer queue is ready.";
        } else if (event.name == "CustomerQueueExited") {
            m_status_message = "You left the customer queue area.";
        }
    }
}

void GameplayRuntime::update_triggers(Scene& scene, GameplayEventBus* event_bus) {
    const auto* player_transform = scene.get_component<TransformComponent>(m_player_entity);
    if (player_transform == nullptr || event_bus == nullptr) return;

    scene.for_each<TransformComponent, TriggerVolumeComponent>(
        [&](EntityId entity, TransformComponent& transform, TriggerVolumeComponent& trigger) {
            if (!trigger.enabled || (trigger.fire_once && trigger.fired)) return;
            const Vec3 offset = player_transform->position - transform.position;
            const bool inside = length(offset) <= std::max(0.1f, trigger.radius);
            const bool was_inside = m_trigger_inside[entity];
            if (inside && !was_inside) {
                event_bus->publish({
                    .source_entity = entity,
                    .type = GameplayEventType::Trigger,
                    .name = "EnterTrigger",
                    .target_entity = m_player_entity,
                });
                if (trigger.fire_once) trigger.fired = true;
            } else if (!inside && was_inside) {
                event_bus->publish({
                    .source_entity = entity,
                    .type = GameplayEventType::Trigger,
                    .name = "ExitTrigger",
                    .target_entity = m_player_entity,
                });
            }
            m_trigger_inside[entity] = inside;
        }
    );
}

void GameplayRuntime::update_enemies(Scene& scene, double delta_seconds, GameplayEventBus* event_bus) {
    const auto* player_transform = scene.get_component<TransformComponent>(m_player_entity);
    auto* player_health = scene.get_component<HealthComponent>(m_player_entity);
    if (player_transform == nullptr || player_health == nullptr) return;

    const float dt = static_cast<float>(std::max(0.0, delta_seconds));
    int enemy_count = 0;
    int living_enemies = 0;
    scene.for_each<TransformComponent, HealthComponent, EnemyAIComponent>(
        [&](EntityId entity, TransformComponent& transform, HealthComponent& health, EnemyAIComponent& enemy) {
            if (entity == m_player_entity || !enemy.enabled) return;
            ++enemy_count;
            if (health.current <= 0.0f) return;
            ++living_enemies;

            Vec3 offset = player_transform->position - transform.position;
            offset.y = 0.0f;
            const float distance = length(offset);
            if (distance <= 0.001f || distance > enemy.detection_range) return;

            if (distance > enemy.attack_range) {
                const Vec3 direction = offset * (1.0f / distance);
                transform.position += direction * (enemy.move_speed * dt);
                transform.rotation_degrees.y = yaw_from_direction(direction);
                return;
            }

            float& elapsed = m_enemy_attack_elapsed[entity];
            elapsed += dt;
            if (elapsed < std::max(0.05f, enemy.attack_cooldown)) return;
            elapsed = 0.0f;
            if (player_health->invulnerable || player_health->current <= 0.0f) return;

            player_health->current = std::max(0.0f, player_health->current - enemy.attack_damage);
            if (event_bus != nullptr) {
                GameplayEvent damage;
                damage.source_entity = entity;
                damage.target_entity = m_player_entity;
                damage.type = GameplayEventType::Damage;
                damage.name = "EnemyAttack";
                damage.value = enemy.attack_damage;
                event_bus->publish(damage);
            }
        }
    );

    if (enemy_count > 0 && living_enemies == 0 && !m_victory_announced) {
        m_victory_announced = true;
        m_status_message = "Area cleared! You win.";
        if (event_bus != nullptr) {
            GameplayEvent victory;
            victory.source_entity = m_player_entity;
            victory.type = GameplayEventType::Interaction;
            victory.name = "Victory";
            event_bus->publish(victory);
        }
    }
}

EntityId GameplayRuntime::resolve_player_camera(Scene& scene) const {
    if (m_player_entity == InvalidEntity || !scene.is_alive(m_player_entity)) {
        return InvalidEntity;
    }

    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    const bool wants_third_person =
        controller != nullptr && controller->view_mode == PlayerViewMode::ThirdPerson;

    if (wants_third_person) {
        const std::string& player_id = scene.entity_persistent_id(m_player_entity);
        EntityId child_camera = InvalidEntity;
        scene.for_each<TransformComponent, CameraComponent>(
            [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
                if (child_camera != InvalidEntity || !camera.enabled || camera.editor_only) return;
                const auto* parent = scene.get_component<ParentComponent>(entity);
                if (parent != nullptr && parent->parent_persistent_id == player_id) {
                    child_camera = entity;
                }
            }
        );
        if (child_camera != InvalidEntity) {
            return child_camera;
        }
    }

    if (const auto* camera = scene.get_component<CameraComponent>(m_player_entity);
        camera != nullptr && camera->enabled && !camera->editor_only) {
        return m_player_entity;
    }

    const std::string& player_id = scene.entity_persistent_id(m_player_entity);
    EntityId child_camera = InvalidEntity;
    scene.for_each<TransformComponent, CameraComponent>(
        [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
            if (child_camera != InvalidEntity || !camera.enabled || camera.editor_only) return;
            const auto* parent = scene.get_component<ParentComponent>(entity);
            if (parent != nullptr && parent->parent_persistent_id == player_id) {
                child_camera = entity;
            }
        }
    );
    if (child_camera != InvalidEntity) {
        return child_camera;
    }

    EntityId primary_camera = InvalidEntity;
    scene.for_each<TransformComponent, CameraComponent>(
        [&](EntityId entity, TransformComponent&, CameraComponent& camera) {
            if (primary_camera == InvalidEntity && camera.enabled && camera.primary && !camera.editor_only) {
                primary_camera = entity;
            }
        }
    );
    return primary_camera;
}

void GameplayRuntime::update_player(Scene& scene, double delta_seconds, GameplayEventBus* event_bus) {
    auto* transform = scene.get_component<TransformComponent>(m_player_entity);
    auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (transform == nullptr || controller == nullptr || !controller->enabled) {
        m_jump_requested = false;
        return;
    }

    if (controller->view_mode == PlayerViewMode::ThirdPerson) {
        m_third_person_yaw += m_pending_look_x * controller->look_sensitivity;
        m_third_person_pitch -= m_pending_look_y * controller->look_sensitivity;
        const float min_pitch = std::min(controller->camera_min_pitch, controller->camera_max_pitch);
        const float max_pitch = std::max(controller->camera_min_pitch, controller->camera_max_pitch);
        m_third_person_pitch = std::clamp(m_third_person_pitch, min_pitch, max_pitch);
    } else {
        transform->rotation_degrees.y += m_pending_look_x * controller->look_sensitivity;
        transform->rotation_degrees.x -= m_pending_look_y * controller->look_sensitivity;
        transform->rotation_degrees.x = std::clamp(transform->rotation_degrees.x, -89.0f, 89.0f);
    }
    m_pending_look_x = 0.0f;
    m_pending_look_y = 0.0f;

    const float speed = controller->move_speed * (m_fast ? controller->fast_multiplier : 1.0f);
    const float distance = speed * static_cast<float>(delta_seconds);
    const float movement_yaw = controller->view_mode == PlayerViewMode::ThirdPerson
        ? m_third_person_yaw
        : transform->rotation_degrees.y;
    const Vec3 movement_rotation{0.0f, movement_yaw, 0.0f};
    const Vec3 movement_forward = forward_from_euler(movement_rotation);
    const Vec3 movement_right = right_from_euler(movement_rotation);

    Vec3 wish{};
    if (m_forward) wish += movement_forward;
    if (m_backward) wish += movement_forward * -1.0f;
    if (m_right) wish += movement_right;
    if (m_left) wish += movement_right * -1.0f;
    wish.y = 0.0f;
    if (length(wish) > 1.0f) wish = normalize(wish);

    if (controller->view_mode == PlayerViewMode::ThirdPerson &&
        controller->orient_to_movement && length(wish) > 0.0001f) {
        transform->rotation_degrees.x = 0.0f;
        transform->rotation_degrees.y = yaw_from_direction(wish);
        transform->rotation_degrees.z = 0.0f;
    }

    auto* character_body = scene.get_component<CharacterBodyComponent>(m_player_entity);
    if (character_body != nullptr && character_body->enabled) {
        PhysicsSystem::move_character(
            scene,
            m_player_entity,
            wish * distance,
            m_jump_requested,
            delta_seconds,
            m_character_state
        );
        m_jump_requested = false;
        const float movement_speed = length(wish) * speed;
        const bool sprinting = m_fast && length(wish) > 0.0001f;
        publish_locomotion(
            scene,
            m_player_entity,
            movement_speed,
            m_character_state.grounded,
            m_character_state.vertical_velocity,
            sprinting
        );
        publish_footstep_event(delta_seconds, movement_speed, m_character_state.grounded, sprinting, event_bus);
        if (controller->view_mode == PlayerViewMode::ThirdPerson) {
            update_third_person_camera(scene);
        }
        return;
    }

    if (length(wish) > 0.0001f) {
        transform->position += wish * distance;
    }

    if (controller->view_mode == PlayerViewMode::FirstPerson) {
        const Vec3 world_up{0.0f, 1.0f, 0.0f};
        if (m_up) transform->position += world_up * distance;
        if (m_down) transform->position += world_up * -distance;
    }

    const float movement_speed = length(wish) * speed;
    const bool sprinting = m_fast && length(wish) > 0.0001f;
    publish_locomotion(
        scene,
        m_player_entity,
        movement_speed,
        true,
        0.0f,
        sprinting
    );
    publish_footstep_event(delta_seconds, movement_speed, true, sprinting, event_bus);
    m_jump_requested = false;
    if (controller->view_mode == PlayerViewMode::ThirdPerson) {
        update_third_person_camera(scene);
    }
}

void GameplayRuntime::publish_footstep_event(
    double delta_seconds,
    float movement_speed,
    bool grounded,
    bool sprinting,
    GameplayEventBus* event_bus
) {
    if (event_bus == nullptr || !grounded || movement_speed <= 0.001f) {
        m_footstep_elapsed = 0.0f;
        return;
    }

    m_footstep_elapsed += static_cast<float>(std::max(0.0, delta_seconds));
    const float interval = sprinting ? 0.30f : 0.45f;
    while (m_footstep_elapsed >= interval) {
        m_footstep_elapsed -= interval;
        event_bus->publish({m_player_entity, GameplayEventType::Footstep, "Footstep", 0.0f});
    }
}

void GameplayRuntime::update_third_person_camera(Scene& scene) {
    if (m_camera_entity == InvalidEntity || !scene.is_alive(m_camera_entity)) {
        m_camera_entity = resolve_player_camera(scene);
    }

    auto* camera_transform = scene.get_component<TransformComponent>(m_camera_entity);
    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (camera_transform == nullptr || controller == nullptr ||
        !scene.has_component<TransformComponent>(m_player_entity)) {
        return;
    }

    const TransformComponent player_world = world_transform(scene, m_player_entity);
    const Vec3 view_rotation{m_third_person_pitch, m_third_person_yaw, 0.0f};
    const Vec3 view_forward = forward_from_euler(view_rotation);
    const Vec3 view_right = right_from_euler(view_rotation);
    const Vec3 anchor = player_world.position +
        Vec3{0.0f, std::max(0.0f, controller->camera_height), 0.0f};
    const Vec3 desired =
        anchor - view_forward * std::max(0.1f, controller->camera_distance) +
        view_right * controller->camera_shoulder_offset;
    const Vec3 clipped = PhysicsSystem::clip_camera_position(
        scene,
        m_player_entity,
        anchor,
        desired,
        0.15f,
        0.05f
    );

    const bool camera_is_child = parent_entity(scene, m_camera_entity) == m_player_entity;
    if (camera_is_child) {
        Mat4 inverse_parent{};
        if (inverse_matrix(world_transform_matrix(scene, m_player_entity), inverse_parent)) {
            const Vec4 local = inverse_parent * Vec4{clipped.x, clipped.y, clipped.z, 1.0f};
            camera_transform->position = {local.x, local.y, local.z};
        } else {
            camera_transform->position = clipped - player_world.position;
        }
        camera_transform->rotation_degrees = {
            m_third_person_pitch,
            m_third_person_yaw - player_world.rotation_degrees.y,
            0.0f
        };
    } else {
        camera_transform->position = clipped;
        camera_transform->rotation_degrees = view_rotation;
    }
}

void GameplayRuntime::refresh_interaction_target(Scene& scene) {
    m_interaction_target = InvalidEntity;
    m_interaction_prompt.clear();

    if (m_dialogue_active) {
        if (scene.is_alive(m_dialogue_entity)) {
            m_interaction_target = m_dialogue_entity;
            m_interaction_prompt = "[E] Next";
        } else {
            m_dialogue_active = false;
            m_dialogue_entity = InvalidEntity;
            m_dialogue_line_index = 0;
        }
        return;
    }

    const auto* controller = scene.get_component<PlayerControllerComponent>(m_player_entity);
    if (controller == nullptr || !controller->enabled || !scene.has_component<TransformComponent>(m_player_entity)) {
        return;
    }

    const TransformComponent player_world = world_transform(scene, m_player_entity);
    TransformComponent view_transform = player_world;
    if (m_camera_entity != InvalidEntity && scene.is_alive(m_camera_entity) &&
        scene.has_component<TransformComponent>(m_camera_entity)) {
        view_transform = world_transform(scene, m_camera_entity);
    }

    const Vec3 origin = view_transform.position;
    const Vec3 forward = forward_from_euler(view_transform.rotation_degrees);
    const bool third_person = controller->view_mode == PlayerViewMode::ThirdPerson;
    const float ray_distance = controller->interaction_distance +
        (third_person ? std::max(0.0f, controller->camera_distance) : 0.0f);
    float best_projection = std::numeric_limits<float>::max();

    scene.for_each<TransformComponent, InteractableComponent>(
        [&](EntityId entity, TransformComponent&, InteractableComponent& interactable) {
            if (entity == m_player_entity || !interactable.enabled) {
                return;
            }

            const TransformComponent target_world = world_transform(scene, entity);
            const float entity_radius = std::max(0.4f, maximum_scale(target_world) * 0.65f);

            const float player_distance = length(target_world.position - player_world.position);
            if (player_distance > controller->interaction_distance + entity_radius) {
                return;
            }

            const Vec3 to_target = target_world.position - origin;
            const float projection = dot(to_target, forward);
            if (projection <= 0.0f || projection > ray_distance + entity_radius) {
                return;
            }

            const Vec3 closest_point = origin + forward * projection;
            const float perpendicular_distance = length(target_world.position - closest_point);
            const float allowed_radius = controller->interaction_radius + entity_radius;
            if (perpendicular_distance > allowed_radius || projection >= best_projection) {
                return;
            }

            best_projection = projection;
            m_interaction_target = entity;
        }
    );

    if (m_interaction_target == InvalidEntity) {
        return;
    }

    const auto* interactable = scene.get_component<InteractableComponent>(m_interaction_target);
    if (interactable == nullptr) {
        return;
    }

    if (const auto* worker = scene.get_component<StaffWorkerComponent>(m_interaction_target)) {
        float cost = worker->employed ? worker->skill_upgrade_cost : worker->hiring_cost;
        scene.for_each<ShopInventoryComponent, WalletComponent>(
            [&](EntityId entity, ShopInventoryComponent&, WalletComponent&) {
                const auto* manager = scene.get_component<StaffManagerComponent>(entity);
                if (manager != nullptr && cost <= 0.0f) {
                    cost = worker->employed
                        ? manager->default_skill_upgrade_cost
                        : manager->default_hiring_cost;
                }
            }
        );
        if (!worker->employed) {
            m_interaction_prompt = "[E] Hire " + worker->role + " — " + std::to_string(cost);
        } else if (worker->skill_level < worker->max_skill_level) {
            m_interaction_prompt = "[E] Upgrade " + worker->role + " — " + std::to_string(cost);
        } else {
            m_interaction_prompt = "[E] " + worker->role + " (Max Skill)";
        }
    } else if (scene.has_component<CheckoutStationComponent>(m_interaction_target)) {
        bool customer_waiting = false;
        scene.for_each<CustomerBehaviorComponent, CustomerOrderComponent>(
            [&](EntityId, CustomerBehaviorComponent& behavior, CustomerOrderComponent& order) {
                if (customer_waiting ||
                    !behavior.enabled ||
                    behavior.state != "awaiting_payment" ||
                    order.fulfilled) {
                    return;
                }
                m_interaction_prompt = "[E] Pay " + order.customer_name + " — " +
                    std::to_string(SimulationSystem::order_total(order));
                customer_waiting = true;
            }
        );
        if (!customer_waiting) {
            m_interaction_prompt = "[E] Checkout Customer (No customer waiting)";
        }
    } else {
    m_interaction_prompt = "[E] " + interactable->prompt;
    }
    if (const auto* dialogue = scene.get_component<DialogueComponent>(m_interaction_target);
        dialogue != nullptr && !dialogue->interaction_prompt.empty()) {
        m_interaction_prompt = "[E] " + dialogue->interaction_prompt;
    }

    const auto* pickup = scene.get_component<PickupComponent>(m_interaction_target);
    if (pickup != nullptr && pickup->quantity > 1) {
        m_interaction_prompt += "  x" + std::to_string(pickup->quantity);
    }

    const auto* door = scene.get_component<DoorComponent>(m_interaction_target);
    if (door != nullptr && !door->required_item.empty() && !player_has_item(scene, door->required_item)) {
        m_interaction_prompt += "  (Requires " + door->required_item + ")";
    }
}

void GameplayRuntime::perform_interaction(Scene& scene, GameplayEventBus* event_bus) {
    if (m_dialogue_active) {
        advance_dialogue(scene);
        return;
    }

    if (m_interaction_target == InvalidEntity || !scene.is_alive(m_interaction_target)) {
        m_status_message = "Nothing to interact with.";
        return;
    }

    auto* interactable = scene.get_component<InteractableComponent>(m_interaction_target);
    if (interactable == nullptr || !interactable->enabled) {
        m_status_message = "This object is not interactable.";
        return;
    }

    if (auto* dialogue = scene.get_component<DialogueComponent>(m_interaction_target)) {
        if (dialogue->lines.empty()) {
            m_status_message = "Dialogue has no lines.";
            return;
        }
        m_dialogue_entity = m_interaction_target;
        m_dialogue_line_index = 0;
        m_dialogue_active = true;
        present_dialogue_line(scene);
        if (event_bus != nullptr) {
            event_bus->publish({m_interaction_target, GameplayEventType::Interaction, "DialogueStarted", 0.0f});
        }
        return;
    }

    if (auto* worker = scene.get_component<StaffWorkerComponent>(m_interaction_target)) {
        EntityId shop_entity = InvalidEntity;
        WalletComponent* wallet = nullptr;
        StaffManagerComponent* manager = nullptr;
        SimulationClockComponent* clock = nullptr;
        scene.for_each<ShopInventoryComponent, WalletComponent>(
            [&](EntityId entity, ShopInventoryComponent&, WalletComponent& candidate_wallet) {
                if (shop_entity != InvalidEntity) return;
                shop_entity = entity;
                wallet = &candidate_wallet;
                manager = scene.get_component<StaffManagerComponent>(entity);
                clock = scene.get_component<SimulationClockComponent>(entity);
            }
        );
        if (wallet == nullptr) {
            m_status_message = "This worker is not connected to a shop.";
            return;
        }

        int employed_staff = 0;
        scene.for_each<StaffWorkerComponent>(
            [&](EntityId, StaffWorkerComponent& candidate) {
                if (candidate.enabled && candidate.employed) ++employed_staff;
            }
        );

        if (!worker->employed) {
            const int max_staff = manager == nullptr ? 1000 : std::max(1, manager->max_staff);
            if (employed_staff >= max_staff) {
                m_status_message = "The staff roster is full.";
                return;
            }
            const float hiring_cost = worker->hiring_cost > 0.0f
                ? worker->hiring_cost
                : (manager == nullptr ? 0.0f : manager->default_hiring_cost);
            if (wallet->balance < hiring_cost) {
                m_status_message = "Not enough money to hire this worker.";
                return;
            }
            wallet->balance -= hiring_cost;
            wallet->lifetime_expenses += hiring_cost;
            worker->employed = true;
            if (clock != nullptr) worker->last_paid_day = clock->day;
            m_status_message = "Hired " + worker->role + " for " +
                std::to_string(hiring_cost);
            if (event_bus != nullptr) {
                event_bus->publish({
                    m_interaction_target,
                    GameplayEventType::Interaction,
                    "WorkerHired",
                    hiring_cost,
                    shop_entity
                });
            }
            return;
        }

        const int max_skill = std::max(1, worker->max_skill_level);
        if (worker->skill_level >= max_skill) {
            m_status_message = "Worker skill is already at maximum.";
            return;
        }
        const float upgrade_cost = worker->skill_upgrade_cost > 0.0f
            ? worker->skill_upgrade_cost
            : (manager == nullptr ? 0.0f : manager->default_skill_upgrade_cost);
        if (wallet->balance < upgrade_cost) {
            m_status_message = "Not enough money to upgrade this worker.";
            return;
        }
        wallet->balance -= upgrade_cost;
        wallet->lifetime_expenses += upgrade_cost;
        ++worker->skill_level;
        m_status_message = "Upgraded " + worker->role + " to Skill " +
            std::to_string(worker->skill_level);
        if (event_bus != nullptr) {
            event_bus->publish({
                m_interaction_target,
                GameplayEventType::Interaction,
                "WorkerUpgraded",
                upgrade_cost,
                shop_entity
            });
        }
        return;
    }

    if (const auto* checkout = scene.get_component<CheckoutStationComponent>(m_interaction_target)) {
        if (!checkout->enabled) {
            m_status_message = "This checkout station is disabled.";
            return;
        }

        EntityId shop_entity = InvalidEntity;
        ShopInventoryComponent* inventory = nullptr;
        WalletComponent* wallet = nullptr;
        ShopHoursComponent* hours = nullptr;
        SimulationClockComponent* clock = nullptr;
        scene.for_each<ShopInventoryComponent, WalletComponent>(
            [&](EntityId entity, ShopInventoryComponent& candidate_inventory,
                WalletComponent& candidate_wallet) {
                if (shop_entity != InvalidEntity) return;
                shop_entity = entity;
                inventory = &candidate_inventory;
                wallet = &candidate_wallet;
                hours = scene.get_component<ShopHoursComponent>(entity);
                clock = scene.get_component<SimulationClockComponent>(entity);
            }
        );

        if (inventory == nullptr || wallet == nullptr) {
            m_status_message = "Checkout is not connected to a shop.";
            return;
        }
        if (hours != nullptr && clock != nullptr &&
            !SimulationSystem::is_shop_open(*hours, *clock)) {
            m_status_message = "The shop is closed.";
            return;
        }

        EntityId customer_entity = InvalidEntity;
        CustomerOrderComponent* customer_order = nullptr;
        CustomerBehaviorComponent* customer_behavior = nullptr;
        scene.for_each<CustomerBehaviorComponent, CustomerOrderComponent>(
            [&](EntityId entity, CustomerBehaviorComponent& behavior,
                CustomerOrderComponent& order) {
                if (customer_entity != InvalidEntity ||
                    !behavior.enabled ||
                    behavior.state != "awaiting_payment" ||
                    order.fulfilled ||
                    order.lines.empty()) {
                    return;
                }
                customer_entity = entity;
                customer_behavior = &behavior;
                customer_order = &order;
            }
        );

        if (customer_order == nullptr || customer_behavior == nullptr) {
            m_status_message = "No customer is waiting at checkout.";
            return;
        }

        std::vector<ProductDefinitionComponent*> products;
        bool valid_order = true;
        for (const auto& line : customer_order->lines) {
            ProductDefinitionComponent* product = nullptr;
            scene.for_each<ProductDefinitionComponent>(
                [&](EntityId, ProductDefinitionComponent& candidate) {
                    if (product == nullptr && candidate.product_id == line.product_id) {
                        product = &candidate;
                    }
                }
            );
            const auto stock = std::find_if(
                inventory->products.begin(),
                inventory->products.end(),
                [&](const ProductStock& candidate) {
                    return candidate.product_id == line.product_id;
                }
            );
            if (product == nullptr || stock == inventory->products.end() ||
                stock->quantity < line.quantity) {
                valid_order = false;
                break;
            }
            products.push_back(product);
        }

        if (!valid_order) {
            m_status_message = customer_order->customer_name + " is waiting for stock.";
            return;
        }

        float total = 0.0f;
        for (std::size_t index = 0; index < customer_order->lines.size(); ++index) {
            const auto transaction = SimulationSystem::sell_product(
                *inventory,
                *wallet,
                *products[index],
                customer_order->lines[index].quantity
            );
            if (!transaction.succeeded) {
                m_status_message = "Checkout could not complete the order.";
                return;
            }
            customer_order->lines[index].fulfilled_quantity =
                customer_order->lines[index].quantity;
            total += transaction.total;
        }

        if (auto* metrics = scene.get_component<ShopMetricsComponent>(shop_entity)) {
            int waiting_customers = 0;
            scene.for_each<CustomerBehaviorComponent, CustomerOrderComponent>(
                [&](EntityId, CustomerBehaviorComponent& behavior,
                    CustomerOrderComponent& order) {
                    if (behavior.enabled &&
                        (behavior.state == "queued" ||
                         behavior.state == "shopping" ||
                         behavior.state == "awaiting_payment") &&
                        !order.fulfilled) {
                        ++waiting_customers;
                    }
                }
            );

            metrics->daily_revenue += total;
            ++metrics->customers_served_today;
            ++metrics->orders_completed_today;
            for (const auto& line : customer_order->lines) {
                metrics->products_sold_today += line.quantity;
            }
            const float satisfaction = std::max(
                0.0f,
                100.0f - static_cast<float>(std::max(0, waiting_customers - 1)) * 10.0f
            );
            metrics->daily_satisfaction =
                metrics->satisfaction_samples == 0
                    ? satisfaction
                    : (metrics->daily_satisfaction *
                       static_cast<float>(metrics->satisfaction_samples) +
                       satisfaction) /
                        static_cast<float>(metrics->satisfaction_samples + 1);
            ++metrics->satisfaction_samples;
        }

        customer_order->fulfilled = true;
        customer_order->delivered = true;
        customer_behavior->state = "leaving";
        customer_behavior->timer_seconds = 0.0f;
        m_status_message = "Checkout complete for " + customer_order->customer_name +
            " — paid " + std::to_string(total) + ".";
        if (event_bus != nullptr) {
            event_bus->publish({
                customer_entity,
                GameplayEventType::Interaction,
                "CustomerPaymentCompleted",
                total,
                shop_entity
            });
        }
        return;
    }

    if (const auto* product = scene.get_component<ProductDefinitionComponent>(m_interaction_target)) {
        EntityId shop_entity = InvalidEntity;
        ShopInventoryComponent* inventory = nullptr;
        WalletComponent* wallet = nullptr;
        ShopHoursComponent* hours = nullptr;
        SimulationClockComponent* clock = nullptr;

        scene.for_each<ShopInventoryComponent, WalletComponent>(
            [&](EntityId entity, ShopInventoryComponent& candidate_inventory, WalletComponent& candidate_wallet) {
                if (shop_entity != InvalidEntity) return;
                shop_entity = entity;
                inventory = &candidate_inventory;
                wallet = &candidate_wallet;
                hours = scene.get_component<ShopHoursComponent>(entity);
                clock = scene.get_component<SimulationClockComponent>(entity);
            }
        );

        if (inventory == nullptr || wallet == nullptr) {
            m_status_message = "This product is not connected to a shop.";
            return;
        }
        if (hours != nullptr && clock != nullptr &&
            !SimulationSystem::is_shop_open(*hours, *clock)) {
            m_status_message = "The shop is closed.";
            return;
        }

        const SimulationTransaction transaction =
            SimulationSystem::sell_product(*inventory, *wallet, *product, 1);
        if (!transaction.succeeded) {
            m_status_message = "Could not sell " + product->display_name + ": " + transaction.reason;
            return;
        }

        bool order_completed = false;
        scene.for_each<CustomerOrderComponent>(
            [&](EntityId, CustomerOrderComponent& order) {
                if (order.fulfilled) return;
                for (auto& line : order.lines) {
                    if (line.product_id == product->product_id &&
                        line.fulfilled_quantity < line.quantity) {
                        ++line.fulfilled_quantity;
                        break;
                    }
                }
                order.fulfilled = !order.lines.empty();
                for (const auto& line : order.lines) {
                    if (line.fulfilled_quantity < line.quantity) {
                        order.fulfilled = false;
                        break;
                    }
                }
                order_completed = order_completed || order.fulfilled;
            }
        );

        m_status_message = "Sold " + product->display_name + " for " +
            std::to_string(transaction.total) + ".";
        if (order_completed) {
            m_status_message += " Customer order fulfilled.";
        }
        if (event_bus != nullptr) {
            GameplayEvent sale;
            sale.source_entity = m_interaction_target;
            sale.type = GameplayEventType::Interaction;
            sale.name = "ProductSold";
            sale.value = transaction.total;
            sale.target_entity = shop_entity;
            event_bus->publish(sale);
            if (order_completed) {
                GameplayEvent order_event;
                order_event.source_entity = shop_entity;
                order_event.type = GameplayEventType::Interaction;
                order_event.name = "OrderFulfilled";
                event_bus->publish(order_event);
            }
        }
        return;
    }

    if (auto* upgrade = scene.get_component<ShopUpgradeComponent>(m_interaction_target)) {
        EntityId shop_entity = InvalidEntity;
        ShopInventoryComponent* inventory = nullptr;
        WalletComponent* wallet = nullptr;
        scene.for_each<ShopInventoryComponent, WalletComponent>(
            [&](EntityId entity, ShopInventoryComponent& candidate_inventory,
                WalletComponent& candidate_wallet) {
                if (shop_entity != InvalidEntity) return;
                shop_entity = entity;
                inventory = &candidate_inventory;
                wallet = &candidate_wallet;
            }
        );

        if (inventory == nullptr || wallet == nullptr) {
            m_status_message = "Upgrade station is not connected to a shop.";
            return;
        }
        if (upgrade->level >= upgrade->max_level) {
            m_status_message = "The shop is already fully upgraded.";
            return;
        }

        const float upgrade_cost = upgrade->base_upgrade_cost *
            std::pow(
                std::max(1.0f, upgrade->cost_multiplier),
                static_cast<float>(std::max(0, upgrade->level - 1))
            );
        if (wallet->balance < upgrade_cost) {
            m_status_message = "Upgrade costs " + std::to_string(upgrade_cost) +
                ". Keep serving customers.";
            return;
        }

        wallet->balance -= upgrade_cost;
        wallet->lifetime_expenses += upgrade_cost;
        ++upgrade->level;
        for (auto& stock : inventory->products) {
            stock.shelf_capacity += std::max(0, upgrade->shelf_capacity_bonus);
        }

        m_status_message = "Shop upgraded to Level " +
            std::to_string(upgrade->level) + " for " +
            std::to_string(upgrade_cost) + ".";
        if (event_bus != nullptr) {
            event_bus->publish({
                m_interaction_target,
                GameplayEventType::Interaction,
                "ShopUpgraded",
                upgrade_cost,
                shop_entity
            });
        }
        return;
    }

    if (auto* inventory = scene.get_component<ShopInventoryComponent>(m_interaction_target)) {
        auto* wallet = scene.get_component<WalletComponent>(m_interaction_target);
        if (wallet == nullptr) {
            m_status_message = "This restock station has no wallet.";
            return;
        }

        ShopHoursComponent* hours = scene.get_component<ShopHoursComponent>(m_interaction_target);
        SimulationClockComponent* clock = scene.get_component<SimulationClockComponent>(m_interaction_target);
        if (hours != nullptr && clock != nullptr &&
            !SimulationSystem::is_shop_open(*hours, *clock)) {
            m_status_message = "The shop is closed.";
            return;
        }

        int restocked_units = 0;
        float total_cost = 0.0f;
        scene.for_each<ProductDefinitionComponent>(
            [&](EntityId, ProductDefinitionComponent& product) {
                const SimulationTransaction transaction =
                    SimulationSystem::restock_product(*inventory, *wallet, product, 5);
                if (transaction.succeeded) {
                    restocked_units += transaction.quantity;
                    total_cost += transaction.total;
                }
            }
        );

        if (restocked_units == 0) {
            m_status_message = "No products needed restocking.";
            return;
        }
        m_status_message = "Restocked " + std::to_string(restocked_units) +
            " units for " + std::to_string(total_cost) + ".";
        if (event_bus != nullptr) {
            GameplayEvent restock_event;
            restock_event.source_entity = m_interaction_target;
            restock_event.type = GameplayEventType::Interaction;
            restock_event.name = "ProductsRestocked";
            restock_event.value = total_cost;
            event_bus->publish(restock_event);
        }
        return;
    }

    if (auto* pickup = scene.get_component<PickupComponent>(m_interaction_target)) {
        const std::string item_id = pickup->item_id;
        const std::string display_name = pickup->display_name.empty() ? item_id : pickup->display_name;
        const int quantity = std::max(1, pickup->quantity);
        const bool destroy_on_pickup = pickup->destroy_on_pickup;
        const EntityId pickup_entity = m_interaction_target;

        if (item_id.empty()) {
            m_status_message = "Pickup needs an Item ID.";
            return;
        }
        if (!add_player_item(scene, item_id, quantity)) {
            m_status_message = "Could not add " + display_name + " to Inventory.";
            return;
        }

        m_status_message = "Picked up " + display_name;
        if (quantity > 1) {
            m_status_message += " x" + std::to_string(quantity);
        }
        m_status_message += ".";

        if (destroy_on_pickup) {
            m_doors.erase(pickup_entity);
            scene.destroy_entity(pickup_entity);
        } else {
            interactable = scene.get_component<InteractableComponent>(pickup_entity);
            if (interactable != nullptr) {
                interactable->enabled = false;
            }
        }

        if (event_bus != nullptr) {
            event_bus->publish({pickup_entity, GameplayEventType::Interaction, "PickupCollected", 0.0f});
        }
        m_interaction_target = InvalidEntity;
        return;
    }

    auto* door = scene.get_component<DoorComponent>(m_interaction_target);
    if (door == nullptr) {
        m_status_message = interactable->prompt + " triggered.";
        if (event_bus != nullptr) {
            event_bus->publish({m_interaction_target, GameplayEventType::Interaction, "Interact", 0.0f});
        }
        return;
    }

    auto state_it = m_doors.find(m_interaction_target);
    if (state_it == m_doors.end()) {
        auto* transform = scene.get_component<TransformComponent>(m_interaction_target);
        if (transform == nullptr) {
            m_status_message = "Door cannot move because it has no Transform.";
            return;
        }
        DoorRuntimeState state;
        state.closed_position = transform->position;
        state.closed_rotation = transform->rotation_degrees;
        state.progress = door->starts_open ? 1.0f : 0.0f;
        state.target_open = door->starts_open;
        state_it = m_doors.emplace(m_interaction_target, state).first;
    }

    auto& state = state_it->second;
    const bool opening = !state.target_open;

    if (opening && !door->required_item.empty()) {
        if (!player_has_item(scene, door->required_item)) {
            m_status_message = "Locked — requires " + door->required_item + ".";
            return;
        }
        if (door->consume_required_item && !consume_player_item(scene, door->required_item)) {
            m_status_message = "Locked — requires " + door->required_item + ".";
            return;
        }
    }

    state.target_open = opening;
    m_status_message = opening ? "Opening " : "Closing ";
    m_status_message += scene.entity_name(m_interaction_target) + ".";
    if (event_bus != nullptr) {
        event_bus->publish({
            m_interaction_target,
            GameplayEventType::Interaction,
            opening ? "DoorOpened" : "DoorClosed",
            0.0f
        });
    }
}

void GameplayRuntime::present_dialogue_line(Scene& scene) {
    auto* dialogue = scene.get_component<DialogueComponent>(m_dialogue_entity);
    if (dialogue == nullptr || m_dialogue_line_index >= dialogue->lines.size()) return;

    const auto& line = dialogue->lines[m_dialogue_line_index];
    m_status_message = line.speaker + ": " + line.text;
    if (line.choices.empty()) {
        m_interaction_prompt = "[E] Next";
        return;
    }

    m_interaction_prompt = "[1-3] Choose";
    const std::size_t count = std::min<std::size_t>(3, line.choices.size());
    for (std::size_t index = 0; index < count; ++index) {
        m_status_message += "\n" + std::to_string(index + 1) + ") " + line.choices[index].label;
    }
}

void GameplayRuntime::advance_dialogue(Scene& scene, int choice_index) {
    if (!m_dialogue_active || !scene.is_alive(m_dialogue_entity)) {
        m_dialogue_active = false;
        m_dialogue_entity = InvalidEntity;
        m_dialogue_line_index = 0;
        return;
    }

    auto* dialogue = scene.get_component<DialogueComponent>(m_dialogue_entity);
    if (dialogue == nullptr || dialogue->lines.empty()) {
        m_dialogue_active = false;
        m_dialogue_entity = InvalidEntity;
        m_dialogue_line_index = 0;
        m_status_message = "Dialogue ended.";
        return;
    }

    const auto& current_line = dialogue->lines[m_dialogue_line_index];
    if (!current_line.choices.empty()) {
        if (choice_index < 0 ||
            choice_index >= static_cast<int>(current_line.choices.size()) ||
            choice_index >= 3) {
            m_status_message = "Choose a response with 1, 2, or 3.";
            m_interaction_prompt = "[1-3] Choose";
            return;
        }
        const int next_line = current_line.choices[static_cast<std::size_t>(choice_index)].next_line;
        if (next_line >= 0 && next_line < static_cast<int>(dialogue->lines.size())) {
            m_dialogue_line_index = static_cast<std::size_t>(next_line);
            present_dialogue_line(scene);
            return;
        }
    } else if (choice_index >= 0) {
        m_status_message = "This dialogue line has no choices.";
        m_interaction_prompt = "[E] Next";
        return;
    } else {
        ++m_dialogue_line_index;
        if (m_dialogue_line_index < dialogue->lines.size()) {
            present_dialogue_line(scene);
            return;
        }
    }

    const std::string quest_id = dialogue->quest_id_to_start;
    if (!dialogue->repeatable) {
        if (auto* interactable = scene.get_component<InteractableComponent>(m_dialogue_entity)) {
            interactable->enabled = false;
        }
    }
    m_dialogue_active = false;
    m_dialogue_entity = InvalidEntity;
    m_dialogue_line_index = 0;

    if (!quest_id.empty()) {
        bool started = false;
        scene.for_each<QuestComponent>([&](EntityId, QuestComponent& quest) {
            if (!started && quest.quest_id == quest_id && quest.status == QuestStatus::Inactive) {
                quest.status = QuestStatus::Active;
                m_status_message = "Quest started: " + quest.title + ".";
                started = true;
            }
        });
        if (!started) {
            m_status_message = "Dialogue ended.";
        }
    } else {
        m_status_message = "Dialogue ended.";
    }
}

void GameplayRuntime::perform_attack(Scene& scene, GameplayEventBus* event_bus) {
    if (event_bus == nullptr || m_player_entity == InvalidEntity) return;

    auto* attack = scene.get_component<AttackComponent>(m_player_entity);
    const auto* player_transform = scene.get_component<TransformComponent>(m_player_entity);
    if (attack == nullptr || player_transform == nullptr) return;

    attack->active = true;
    event_bus->publish({
        .source_entity = m_player_entity,
        .type = GameplayEventType::Animation,
        .name = attack->open_event,
        .time_seconds = 0.0f,
    });

    const Vec3 forward = forward_from_euler(player_transform->rotation_degrees);
    EntityId best_target = InvalidEntity;
    float best_distance = std::numeric_limits<float>::max();
    scene.for_each<TransformComponent, HealthComponent>(
        [&](EntityId entity, TransformComponent& transform, HealthComponent& health) {
            if (entity == m_player_entity || health.current <= 0.0f || health.invulnerable) return;
            const Vec3 offset = transform.position - player_transform->position;
            const float distance = length(offset);
            if (distance > 3.0f || distance <= 0.001f) return;
            if (dot(normalize(offset), forward) < 0.15f) return;
            if (distance < best_distance) {
                best_distance = distance;
                best_target = entity;
            }
        }
    );

    if (best_target != InvalidEntity) {
        CombatSystem::apply_attack_damage(
            scene,
            m_player_entity,
            best_target,
            *event_bus,
            0.0f
        );
    }

    event_bus->publish({
        .source_entity = m_player_entity,
        .type = GameplayEventType::Animation,
        .name = attack->close_event,
        .time_seconds = 0.0f,
    });
    attack->active = false;
}

void GameplayRuntime::update_quests(Scene& scene) {
    scene.for_each<QuestComponent>([&](EntityId, QuestComponent& quest) {
        if (quest.status != QuestStatus::Active) return;
        const int required = std::max(1, quest.required_quantity);
        if (!quest.required_item.empty()) {
            const auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
            const auto it = inventory == nullptr
                ? std::unordered_map<std::string, int>::const_iterator{}
                : inventory->items.find(quest.required_item);
            if (inventory == nullptr || it == inventory->items.end() || it->second < required) {
                return;
            }
        }
        quest.status = QuestStatus::Completed;
        m_status_message = "Quest complete: " + quest.title + ".";
        if (!quest.reward_item.empty()) {
            const int reward_quantity = std::max(1, quest.reward_quantity);
            if (add_player_item(scene, quest.reward_item, reward_quantity)) {
                m_status_message += " Reward: " + quest.reward_item;
                if (reward_quantity > 1) {
                    m_status_message += " x" + std::to_string(reward_quantity);
                }
                m_status_message += ".";
            }
        }
    });
}

void GameplayRuntime::update_doors(Scene& scene, double delta_seconds) {
    for (auto& [entity, state] : m_doors) {
        if (!scene.is_alive(entity)) {
            continue;
        }

        const auto* door = scene.get_component<DoorComponent>(entity);
        if (door == nullptr) {
            continue;
        }

        const float duration = std::max(door->duration_seconds, 0.05f);
        const float step = static_cast<float>(delta_seconds) / duration;
        if (state.target_open) {
            state.progress = std::min(1.0f, state.progress + step);
        } else {
            state.progress = std::max(0.0f, state.progress - step);
        }
        apply_door_transform(scene, entity, state);
    }
}

void GameplayRuntime::apply_door_transform(
    Scene& scene,
    EntityId entity,
    const DoorRuntimeState& state
) {
    auto* transform = scene.get_component<TransformComponent>(entity);
    const auto* door = scene.get_component<DoorComponent>(entity);
    if (transform == nullptr || door == nullptr) {
        return;
    }

    transform->position = state.closed_position;
    transform->rotation_degrees = state.closed_rotation;

    if (door->motion == DoorMotion::Rotate) {
        transform->rotation_degrees.y += door->open_amount * state.progress;
    } else {
        const Vec3 slide_direction = right_from_euler(state.closed_rotation);
        transform->position += slide_direction * (door->open_amount * state.progress);
    }
}

bool GameplayRuntime::player_has_item(const Scene& scene, const std::string& item_id) const {
    if (item_id.empty()) {
        return true;
    }

    const auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
    if (inventory == nullptr) {
        return false;
    }

    const auto it = inventory->items.find(item_id);
    return it != inventory->items.end() && it->second > 0;
}

bool GameplayRuntime::add_player_item(Scene& scene, const std::string& item_id, int quantity) {
    if (item_id.empty() || quantity <= 0 || !scene.is_alive(m_player_entity)) {
        return false;
    }

    auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
    if (inventory == nullptr) {
        inventory = &scene.add_component<InventoryComponent>(m_player_entity);
    }

    inventory->items[item_id] += quantity;
    return true;
}

bool GameplayRuntime::consume_player_item(Scene& scene, const std::string& item_id) {
    auto* inventory = scene.get_component<InventoryComponent>(m_player_entity);
    if (inventory == nullptr) {
        return false;
    }

    const auto it = inventory->items.find(item_id);
    if (it == inventory->items.end() || it->second <= 0) {
        return false;
    }

    --it->second;
    if (it->second <= 0) {
        inventory->items.erase(it);
    }
    return true;
}

} // namespace seed
