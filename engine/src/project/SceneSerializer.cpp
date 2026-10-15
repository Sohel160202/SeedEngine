#include "seed/project/SceneSerializer.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/simulation/SimulationComponents.h"
#include "seed/scene/Scene.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_map>

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

nlohmann::json vec4_to_json(const Vec4& value) {
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

Vec4 vec4_from_json(
    const nlohmann::json& value,
    Vec4 fallback = {1.0f, 1.0f, 1.0f, 1.0f}
) {
    if (!value.is_array() || value.size() != 4) return fallback;
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
        value.at(3).get<float>(),
    };
}

const char* door_motion_name(DoorMotion motion) {
    return motion == DoorMotion::Slide ? "slide" : "rotate";
}

DoorMotion door_motion_from_name(const std::string& name) {
    return name == "slide" ? DoorMotion::Slide : DoorMotion::Rotate;
}

const char* player_view_mode_name(PlayerViewMode mode) {
    return mode == PlayerViewMode::ThirdPerson ? "third_person" : "first_person";
}

PlayerViewMode player_view_mode_from_name(const std::string& name) {
    return name == "third_person" ? PlayerViewMode::ThirdPerson : PlayerViewMode::FirstPerson;
}

const char* quest_status_name(QuestStatus status) {
    switch (status) {
    case QuestStatus::Active: return "active";
    case QuestStatus::Completed: return "completed";
    case QuestStatus::Inactive:
    default: return "inactive";
    }
}

QuestStatus quest_status_from_name(const std::string& name) {
    if (name == "active") return QuestStatus::Active;
    if (name == "completed") return QuestStatus::Completed;
    return QuestStatus::Inactive;
}

} // namespace

bool SceneSerializer::save(
    const Scene& scene,
    const std::filesystem::path& scene_file,
    std::string* error
) {
    try {
        if (!scene_file.parent_path().empty()) {
            std::filesystem::create_directories(scene_file.parent_path());
        }

        nlohmann::json entities = nlohmann::json::array();
        scene.for_each_entity([&](EntityId entity, const std::string& name) {
            if (const auto* camera = scene.get_component<CameraComponent>(entity);
                camera != nullptr && camera->editor_only) {
                return;
            }

            nlohmann::json components = nlohmann::json::object();

            if (const auto* transform = scene.get_component<TransformComponent>(entity)) {
                components["Transform"] = {
                    {"position", vec3_to_json(transform->position)},
                    {"rotation", vec3_to_json(transform->rotation_degrees)},
                    {"scale", vec3_to_json(transform->scale)},
                };
            }
            if (const auto* prefab = scene.get_component<PrefabInstanceComponent>(entity)) {
                components["PrefabInstance"] = {
                    {"source_path", prefab->source_path},
                    {"source_entity_id", prefab->source_entity_id},
                    {"linked", prefab->linked},
                };
            }
            if (const auto* parent = scene.get_component<ParentComponent>(entity);
                parent != nullptr && !parent->parent_persistent_id.empty()) {
                components["Parent"] = {{"entity", parent->parent_persistent_id}};
            }
            if (const auto* camera = scene.get_component<CameraComponent>(entity)) {
                components["Camera"] = {
                    {"field_of_view", camera->field_of_view_degrees},
                    {"near_plane", camera->near_plane},
                    {"far_plane", camera->far_plane},
                    {"enabled", camera->enabled},
                    {"primary", camera->primary},
                };
            }
            if (const auto* light = scene.get_component<DirectionalLightComponent>(entity)) {
                components["DirectionalLight"] = {
                    {"color", vec3_to_json(light->color)},
                    {"intensity", light->intensity},
                    {"enabled", light->enabled},
                    {"casts_shadows", light->casts_shadows},
                    {"shadow_distance", light->shadow_distance},
                };
            }
            if (const auto* light = scene.get_component<AmbientLightComponent>(entity)) {
                components["AmbientLight"] = {
                    {"color", vec3_to_json(light->color)},
                    {"intensity", light->intensity},
                    {"enabled", light->enabled},
                };
            }
            if (const auto* sky = scene.get_component<SkyComponent>(entity)) {
                components["Sky"] = {
                    {"zenith_color", vec3_to_json(sky->zenith_color)},
                    {"horizon_color", vec3_to_json(sky->horizon_color)},
                    {"intensity", sky->intensity},
                    {"enabled", sky->enabled},
                };
            }
            if (const auto* mesh = scene.get_component<MeshComponent>(entity)) {
                components["Mesh"] = {
                    {"asset", mesh->asset_id},
                    {"visible", mesh->visible},
                    {"cast_shadows", mesh->cast_shadows},
                    {"receive_shadows", mesh->receive_shadows},
                };
            }
            if (const auto* mesh = scene.get_component<SkinnedMeshComponent>(entity)) {
                components["SkinnedMesh"] = {
                    {"asset", mesh->asset_id},
                    {"visible", mesh->visible},
                    {"cast_shadows", mesh->cast_shadows},
                    {"receive_shadows", mesh->receive_shadows},
                };
            }
            if (const auto* animation = scene.get_component<AnimationPlayerComponent>(entity)) {
                nlohmann::json events = nlohmann::json::array();
                for (const auto& event : animation->events) {
                    events.push_back({
                        {"clip", event.clip_name},
                        {"name", event.name},
                        {"time", event.time_seconds},
                    });
                }
                components["AnimationPlayer"] = {
                    {"clip", animation->clip_name},
                    {"playback_speed", animation->playback_speed},
                    {"playing", animation->playing},
                    {"blend_duration", animation->blend_duration_seconds},
                    {"locomotion_enabled", animation->locomotion_enabled},
                    {"events", std::move(events)},
                };
            }
            if (const auto* material = scene.get_component<MaterialComponent>(entity)) {
                components["Material"] = {
                    {"asset", material->asset_id},
                    {"base_color", vec4_to_json(material->base_color)},
                    {"metallic", material->metallic},
                    {"roughness", material->roughness},
                    {"normal_scale", material->normal_scale},
                    {"use_base_color_texture", material->use_base_color_texture},
                    {"use_metallic_roughness_texture", material->use_metallic_roughness_texture},
                    {"use_normal_texture", material->use_normal_texture},
                };
            }
            if (const auto* collider = scene.get_component<BoxColliderComponent>(entity)) {
                components["BoxCollider"] = {
                    {"half_extents", vec3_to_json(collider->half_extents)},
                    {"offset", vec3_to_json(collider->offset)},
                    {"enabled", collider->enabled},
                    {"solid", collider->solid},
                };
            }
            if (const auto* character = scene.get_component<CharacterBodyComponent>(entity)) {
                components["CharacterBody"] = {
                    {"radius", character->radius},
                    {"height", character->height},
                    {"eye_height", character->eye_height},
                    {"gravity", character->gravity},
                    {"jump_speed", character->jump_speed},
                    {"max_fall_speed", character->max_fall_speed},
                    {"enabled", character->enabled},
                };
            }
            if (const auto* persistent = scene.get_component<PersistentComponent>(entity)) {
                components["Persistent"] = {{"enabled", persistent->enabled}};
            }
            if (const auto* interactable = scene.get_component<InteractableComponent>(entity)) {
                components["Interactable"] = {
                    {"prompt", interactable->prompt},
                    {"enabled", interactable->enabled},
                };
            }
            if (const auto* door = scene.get_component<DoorComponent>(entity)) {
                components["Door"] = {
                    {"motion", door_motion_name(door->motion)},
                    {"open_amount", door->open_amount},
                    {"duration_seconds", door->duration_seconds},
                    {"required_item", door->required_item},
                    {"consume_required_item", door->consume_required_item},
                    {"starts_open", door->starts_open},
                };
            }
            if (const auto* health = scene.get_component<HealthComponent>(entity)) {
                components["Health"] = {
                    {"maximum", health->maximum},
                    {"current", health->current},
                    {"invulnerable", health->invulnerable},
                };
            }
            if (const auto* attack = scene.get_component<AttackComponent>(entity)) {
                components["Attack"] = {
                    {"open_event", attack->open_event},
                    {"close_event", attack->close_event},
                    {"damage", attack->damage},
                };
            }
            if (const auto* enemy = scene.get_component<EnemyAIComponent>(entity)) {
                components["EnemyAI"] = {
                    {"detection_range", enemy->detection_range},
                    {"attack_range", enemy->attack_range},
                    {"move_speed", enemy->move_speed},
                    {"attack_damage", enemy->attack_damage},
                    {"attack_cooldown", enemy->attack_cooldown},
                    {"enabled", enemy->enabled},
                };
            }
            if (const auto* variables = scene.get_component<GameplayVariablesComponent>(entity)) {
                nlohmann::json values = nlohmann::json::array();
                for (const auto& variable : variables->values) {
                    values.push_back({{"name", variable.name}, {"number", variable.number}, {"boolean", variable.boolean}});
                }
                components["GameplayVariables"] = {{"values", std::move(values)}};
            }
            if (const auto* trigger = scene.get_component<TriggerVolumeComponent>(entity)) {
                components["TriggerVolume"] = {
                    {"radius", trigger->radius}, {"enabled", trigger->enabled}, {"fire_once", trigger->fire_once}
                };
            }
            if (const auto* spawn = scene.get_component<SpawnPointComponent>(entity)) {
                components["SpawnPoint"] = {{"spawn_tag", spawn->spawn_tag}, {"enabled", spawn->enabled}};
            }
            if (const auto* gameplay_rules = scene.get_component<GameplayRulesComponent>(entity)) {
                nlohmann::json rules = nlohmann::json::array();
                for (const auto& rule : gameplay_rules->rules) {
                    rules.push_back({
                        {"when", rule.when_event},
                        {"condition", rule.condition},
                        {"condition_value", rule.condition_value},
                        {"action", rule.action},
                        {"value", rule.action_value},
                        {"required_item", rule.required_item},
                        {"quantity", rule.quantity},
                        {"once", rule.once},
                    });
                }
                components["GameplayRules"] = {{"rules", std::move(rules)}};
            }
            if (const auto* feedback_cues = scene.get_component<FeedbackCueComponent>(entity)) {
                nlohmann::json bindings = nlohmann::json::array();
                for (const auto& binding : feedback_cues->bindings) {
                    bindings.push_back({
                        {"event_name", binding.event_name},
                        {"audio_asset", binding.audio_asset},
                        {"visual_asset", binding.visual_asset},
                        {"volume", binding.volume},
                        {"camera_shake", binding.camera_shake},
                    });
                }
                components["FeedbackCues"] = {{"bindings", std::move(bindings)}};
            }
            if (const auto* product = scene.get_component<ProductDefinitionComponent>(entity)) {
                components["SimulationProduct"] = {
                    {"product_id", product->product_id},
                    {"display_name", product->display_name},
                    {"category", product->category},
                    {"wholesale_price", product->wholesale_price},
                    {"retail_price", product->retail_price},
                    {"demand_weight", product->demand_weight},
                    {"price_sensitivity", product->price_sensitivity},
                    {"max_stock", product->max_stock},
                    {"perishable", product->perishable},
                };
            }
            if (const auto* inventory = scene.get_component<ShopInventoryComponent>(entity)) {
                nlohmann::json products = nlohmann::json::array();
                for (const auto& stock : inventory->products) {
                    products.push_back({
                        {"product_id", stock.product_id},
                        {"quantity", stock.quantity},
                        {"shelf_capacity", stock.shelf_capacity},
                    });
                }
                components["ShopInventory"] = {{"products", std::move(products)}};
            }
            if (const auto* wallet = scene.get_component<WalletComponent>(entity)) {
                components["Wallet"] = {
                    {"balance", wallet->balance},
                    {"lifetime_revenue", wallet->lifetime_revenue},
                    {"lifetime_expenses", wallet->lifetime_expenses},
                };
            }
            if (const auto* goal = scene.get_component<ShopGoalComponent>(entity)) {
                components["ShopGoal"] = {
                    {"title", goal->title},
                    {"target_revenue", goal->target_revenue},
                    {"target_customers", goal->target_customers},
                };
            }
            if (const auto* metrics = scene.get_component<ShopMetricsComponent>(entity)) {
                components["ShopMetrics"] = {
                    {"tracked_day", metrics->tracked_day},
                    {"customers_served_today", metrics->customers_served_today},
                    {"orders_completed_today", metrics->orders_completed_today},
                    {"products_sold_today", metrics->products_sold_today},
                    {"daily_revenue", metrics->daily_revenue},
                    {"daily_satisfaction", metrics->daily_satisfaction},
                    {"satisfaction_samples", metrics->satisfaction_samples},
                };
            }
            if (const auto* upgrade = scene.get_component<ShopUpgradeComponent>(entity)) {
                components["ShopUpgrade"] = {
                    {"level", upgrade->level},
                    {"max_level", upgrade->max_level},
                    {"base_upgrade_cost", upgrade->base_upgrade_cost},
                    {"cost_multiplier", upgrade->cost_multiplier},
                    {"shelf_capacity_bonus", upgrade->shelf_capacity_bonus},
                };
            }
            if (const auto* capacity = scene.get_component<CustomerCapacityComponent>(entity)) {
                components["CustomerCapacity"] = {
                    {"max_active_customers", capacity->max_active_customers},
                    {"max_queue_length", capacity->max_queue_length},
                    {"customers_per_upgrade", capacity->customers_per_upgrade},
                    {"arrival_rate_multiplier", capacity->arrival_rate_multiplier},
                    {"daily_growth_rate", capacity->daily_growth_rate},
                };
            }
            if (const auto* spawner = scene.get_component<CustomerSpawnerComponent>(entity)) {
                components["CustomerSpawner"] = {
                    {"enabled", spawner->enabled},
                    {"max_customers", spawner->max_customers},
                    {"spawn_interval_seconds", spawner->spawn_interval_seconds},
                    {"next_customer_number", spawner->next_customer_number},
                    {"customer_template_persistent_id", spawner->customer_template_persistent_id},
                    {"customer_name_prefix", spawner->customer_name_prefix},
                };
            }
            if (const auto* hours = scene.get_component<ShopHoursComponent>(entity)) {
                components["ShopHours"] = {
                    {"opening_hour", hours->opening_hour},
                    {"closing_hour", hours->closing_hour},
                };
            }
            if (const auto* clock = scene.get_component<SimulationClockComponent>(entity)) {
                components["SimulationClock"] = {
                    {"day", clock->day},
                    {"hour", clock->hour},
                    {"time_scale", clock->time_scale},
                    {"paused", clock->paused},
                };
            }
            if (const auto* checkout = scene.get_component<CheckoutStationComponent>(entity)) {
                components["CheckoutStation"] = {
                    {"enabled", checkout->enabled},
                    {"service_slots", checkout->service_slots},
                    {"service_time_seconds", checkout->service_time_seconds},
                };
            }
            if (const auto* staffing = scene.get_component<StationStaffingComponent>(entity)) {
                components["StationStaffing"] = {
                    {"requires_staff", staffing->requires_staff},
                    {"required_workers", staffing->required_workers},
                    {"required_role", staffing->required_role},
                };
            }
            if (const auto* staff_manager = scene.get_component<StaffManagerComponent>(entity)) {
                components["StaffManager"] = {
                    {"max_staff", staff_manager->max_staff},
                    {"default_hiring_cost", staff_manager->default_hiring_cost},
                    {"default_skill_upgrade_cost", staff_manager->default_skill_upgrade_cost},
                };
            }
            if (const auto* worker = scene.get_component<StaffWorkerComponent>(entity)) {
                components["StaffWorker"] = {
                    {"enabled", worker->enabled},
                    {"employed", worker->employed},
                    {"role", worker->role},
                    {"work_speed", worker->work_speed},
                    {"wage_per_hour", worker->wage_per_hour},
                    {"shift_start_hour", worker->shift_start_hour},
                    {"shift_end_hour", worker->shift_end_hour},
                    {"last_paid_day", worker->last_paid_day},
                    {"skill_level", worker->skill_level},
                    {"max_skill_level", worker->max_skill_level},
                    {"hiring_cost", worker->hiring_cost},
                    {"skill_upgrade_cost", worker->skill_upgrade_cost},
                    {"assigned_station_persistent_id", worker->assigned_station_persistent_id},
                };
            }
            if (const auto* order = scene.get_component<CustomerOrderComponent>(entity)) {
                nlohmann::json lines = nlohmann::json::array();
                for (const auto& line : order->lines) {
                    lines.push_back({
                        {"product_id", line.product_id},
                        {"quantity", line.quantity},
                        {"fulfilled_quantity", line.fulfilled_quantity},
                        {"unit_price", line.unit_price},
                    });
                }
                components["CustomerOrder"] = {
                    {"order_id", order->order_id},
                    {"customer_name", order->customer_name},
                    {"lines", std::move(lines)},
                    {"fulfilled", order->fulfilled},
                    {"delivered", order->delivered},
                };
            }
            if (const auto* customer = scene.get_component<CustomerBehaviorComponent>(entity)) {
                components["CustomerBehavior"] = {
                    {"enabled", customer->enabled},
                    {"arrival_interval_seconds", customer->arrival_interval_seconds},
                    {"shopping_duration_seconds", customer->shopping_duration_seconds},
                    {"leave_duration_seconds", customer->leave_duration_seconds},
                    {"state", customer->state},
                    {"order_sequence", customer->order_sequence},
                };
            }
            if (const auto* inventory = scene.get_component<InventoryComponent>(entity)) {
                components["Inventory"] = {{"items", inventory->items}};
            }
            if (const auto* pickup = scene.get_component<PickupComponent>(entity)) {
                components["Pickup"] = {
                    {"item_id", pickup->item_id},
                    {"display_name", pickup->display_name},
                    {"quantity", pickup->quantity},
                    {"destroy_on_pickup", pickup->destroy_on_pickup},
                };
            }
            if (const auto* dialogue = scene.get_component<DialogueComponent>(entity)) {
                nlohmann::json lines = nlohmann::json::array();
                for (const auto& line : dialogue->lines) {
                    nlohmann::json choices = nlohmann::json::array();
                    for (const auto& choice : line.choices) {
                        choices.push_back({
                            {"label", choice.label},
                            {"next_line", choice.next_line},
                        });
                    }
                    lines.push_back({
                        {"speaker", line.speaker},
                        {"text", line.text},
                        {"choices", std::move(choices)},
                    });
                }
                components["Dialogue"] = {
                    {"lines", std::move(lines)},
                    {"repeatable", dialogue->repeatable},
                    {"interaction_prompt", dialogue->interaction_prompt},
                    {"quest_id_to_start", dialogue->quest_id_to_start},
                };
            }
            if (const auto* quest = scene.get_component<QuestComponent>(entity)) {
                components["Quest"] = {
                    {"quest_id", quest->quest_id},
                    {"title", quest->title},
                    {"description", quest->description},
                    {"required_item", quest->required_item},
                    {"required_quantity", quest->required_quantity},
                    {"reward_item", quest->reward_item},
                    {"reward_quantity", quest->reward_quantity},
                    {"status", quest_status_name(quest->status)},
                };
            }
            if (const auto* player = scene.get_component<PlayerControllerComponent>(entity)) {
                components["PlayerController"] = {
                    {"view_mode", player_view_mode_name(player->view_mode)},
                    {"move_speed", player->move_speed},
                    {"fast_multiplier", player->fast_multiplier},
                    {"look_sensitivity", player->look_sensitivity},
                    {"interaction_distance", player->interaction_distance},
                    {"interaction_radius", player->interaction_radius},
                    {"camera_distance", player->camera_distance},
                    {"camera_height", player->camera_height},
                    {"camera_shoulder_offset", player->camera_shoulder_offset},
                    {"camera_min_pitch", player->camera_min_pitch},
                    {"camera_max_pitch", player->camera_max_pitch},
                    {"orient_to_movement", player->orient_to_movement},
                    {"enabled", player->enabled},
                };
            }

            entities.push_back({
                {"id", scene.entity_persistent_id(entity)},
                {"name", name},
                {"components", std::move(components)},
            });
        });

        nlohmann::json document = {
            {"seed_format", "seedscene"},
            {"format_version", 1},
            {"entities", std::move(entities)},
        };
        std::ofstream output(scene_file, std::ios::binary | std::ios::trunc);
        if (!output) {
            set_error(error, "Could not open scene file for writing: " + scene_file.string());
            return false;
        }
        output << document.dump(2) << '\n';
        return static_cast<bool>(output);
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

bool SceneSerializer::load(
    Scene& scene,
    const std::filesystem::path& scene_file,
    std::string* error
) {
    try {
        std::ifstream input(scene_file, std::ios::binary);
        if (!input) {
            set_error(error, "Could not open scene file: " + scene_file.string());
            return false;
        }

        nlohmann::json document;
        input >> document;
        const std::string seed_format = document.value("seed_format", std::string{});
        if (seed_format != "seedscene" && seed_format != "seedprefab") {
            set_error(error, "File is not a Seed scene or prefab: " + scene_file.string());
            return false;
        }

        Scene loaded_scene;
        for (const auto& entity_json : document.value("entities", nlohmann::json::array())) {
            const std::string name = entity_json.value("name", std::string{"Entity"});
            const std::string persistent_id = entity_json.value("id", std::string{});
            const EntityId entity = loaded_scene.create_entity(name, persistent_id);
            const auto& components =
                entity_json.value("components", nlohmann::json::object());

            if (components.contains("Transform")) {
                const auto& value = components.at("Transform");
                TransformComponent transform;
                transform.position = vec3_from_json(
                    value.value("position", nlohmann::json::array()),
                    {}
                );
                transform.rotation_degrees = vec3_from_json(
                    value.value("rotation", nlohmann::json::array()),
                    {}
                );
                transform.scale = vec3_from_json(
                    value.value("scale", nlohmann::json::array()),
                    {1.0f, 1.0f, 1.0f}
                );
                loaded_scene.add_component<TransformComponent>(entity, transform);
            }
            if (components.contains("PrefabInstance")) {
                const auto& value = components.at("PrefabInstance");
                PrefabInstanceComponent prefab;
                prefab.source_path = value.value("source_path", std::string{});
                prefab.source_entity_id = value.value("source_entity_id", std::string{});
                prefab.linked = value.value("linked", true);
                loaded_scene.add_component<PrefabInstanceComponent>(entity, std::move(prefab));
            }
            if (components.contains("Parent")) {
                const auto& value = components.at("Parent");
                ParentComponent parent;
                parent.parent_persistent_id = value.value("entity", std::string{});
                if (!parent.parent_persistent_id.empty()) {
                    loaded_scene.add_component<ParentComponent>(entity, std::move(parent));
                }
            }
            if (components.contains("Camera")) {
                const auto& value = components.at("Camera");
                CameraComponent camera;
                camera.field_of_view_degrees = value.value("field_of_view", 60.0f);
                camera.near_plane = value.value("near_plane", 0.1f);
                camera.far_plane = value.value("far_plane", 1000.0f);
                camera.enabled = value.value("enabled", true);
                camera.primary = value.value("primary", true);
                camera.editor_only = false;
                loaded_scene.add_component<CameraComponent>(entity, camera);
            }
            if (components.contains("DirectionalLight")) {
                const auto& value = components.at("DirectionalLight");
                DirectionalLightComponent light;
                light.color = vec3_from_json(
                    value.value("color", nlohmann::json::array()),
                    {1.0f, 0.96f, 0.88f}
                );
                light.intensity = value.value("intensity", 1.0f);
                light.enabled = value.value("enabled", true);
                light.casts_shadows = value.value("casts_shadows", true);
                light.shadow_distance = value.value("shadow_distance", 35.0f);
                loaded_scene.add_component<DirectionalLightComponent>(entity, light);
            }
            if (components.contains("AmbientLight")) {
                const auto& value = components.at("AmbientLight");
                AmbientLightComponent light;
                light.color = vec3_from_json(
                    value.value("color", nlohmann::json::array()),
                    {0.72f, 0.82f, 1.0f}
                );
                light.intensity = value.value("intensity", 0.22f);
                light.enabled = value.value("enabled", true);
                loaded_scene.add_component<AmbientLightComponent>(entity, light);
            }
            if (components.contains("Sky")) {
                const auto& value = components.at("Sky");
                SkyComponent sky;
                sky.zenith_color = vec3_from_json(
                    value.value("zenith_color", nlohmann::json::array()),
                    {0.08f, 0.20f, 0.42f}
                );
                sky.horizon_color = vec3_from_json(
                    value.value("horizon_color", nlohmann::json::array()),
                    {0.62f, 0.76f, 0.92f}
                );
                sky.intensity = value.value("intensity", 1.0f);
                sky.enabled = value.value("enabled", true);
                loaded_scene.add_component<SkyComponent>(entity, sky);
            }
            if (components.contains("Mesh")) {
                const auto& value = components.at("Mesh");
                MeshComponent mesh;
                mesh.asset_id = value.value("asset", std::string{"builtin:cube"});
                mesh.visible = value.value("visible", true);
                mesh.cast_shadows = value.value("cast_shadows", true);
                mesh.receive_shadows = value.value("receive_shadows", true);
                loaded_scene.add_component<MeshComponent>(entity, mesh);
            }
            if (components.contains("SkinnedMesh")) {
                const auto& value = components.at("SkinnedMesh");
                SkinnedMeshComponent mesh;
                mesh.asset_id = value.value("asset", std::string{});
                mesh.visible = value.value("visible", true);
                mesh.cast_shadows = value.value("cast_shadows", true);
                mesh.receive_shadows = value.value("receive_shadows", true);
                mesh.mesh = {};
                mesh.runtime_asset = nullptr;
                loaded_scene.add_component<SkinnedMeshComponent>(entity, std::move(mesh));
            }
            if (components.contains("AnimationPlayer")) {
                const auto& value = components.at("AnimationPlayer");
                AnimationPlayerComponent animation;
                animation.clip_name = value.value("clip", std::string{});
                animation.playback_speed = value.value("playback_speed", 1.0f);
                animation.playing = value.value("playing", true);
                animation.blend_duration_seconds = std::max(
                    0.0f,
                    value.value("blend_duration", 0.15f)
                );
                animation.locomotion_enabled = value.value("locomotion_enabled", false);
                if (value.contains("events") && value.at("events").is_array()) {
                    for (const auto& event_value : value.at("events")) {
                        AnimationEvent event;
                        event.clip_name = event_value.value("clip", std::string{});
                        event.name = event_value.value("name", std::string{"Event"});
                        event.time_seconds = std::max(
                            0.0f,
                            event_value.value("time", 0.0f)
                        );
                        animation.events.push_back(std::move(event));
                    }
                }
                animation.time_seconds = 0.0f;
                animation.pose = {};
                loaded_scene.add_component<AnimationPlayerComponent>(entity, std::move(animation));
            }
            if (components.contains("Material")) {
                const auto& value = components.at("Material");
                MaterialComponent material;
                material.asset_id = value.value(
                    "asset",
                    std::string{"builtin:seed_default"}
                );
                material.use_asset_defaults =
                    !value.contains("base_color") &&
                    !value.contains("metallic") &&
                    !value.contains("roughness");
                material.base_color = vec4_from_json(
                    value.value("base_color", nlohmann::json::array()),
                    {1.0f, 1.0f, 1.0f, 1.0f}
                );
                material.metallic = value.value("metallic", 0.0f);
                material.roughness = value.value("roughness", 0.65f);
                material.normal_scale = value.value("normal_scale", 1.0f);
                material.use_base_color_texture =
                    value.value("use_base_color_texture", true);
                material.use_metallic_roughness_texture =
                    value.value("use_metallic_roughness_texture", true);
                material.use_normal_texture = value.value("use_normal_texture", true);
                loaded_scene.add_component<MaterialComponent>(entity, material);
            }
            if (components.contains("BoxCollider")) {
                const auto& value = components.at("BoxCollider");
                BoxColliderComponent collider;
                collider.half_extents = vec3_from_json(
                    value.value("half_extents", nlohmann::json::array()),
                    {0.75f, 0.75f, 0.75f}
                );
                collider.offset = vec3_from_json(
                    value.value("offset", nlohmann::json::array()),
                    {}
                );
                collider.enabled = value.value("enabled", true);
                collider.solid = value.value("solid", true);
                loaded_scene.add_component<BoxColliderComponent>(entity, collider);
            }
            if (components.contains("CharacterBody")) {
                const auto& value = components.at("CharacterBody");
                CharacterBodyComponent character;
                character.radius = value.value("radius", 0.35f);
                character.height = value.value("height", 1.8f);
                character.eye_height = value.value("eye_height", 1.65f);
                character.gravity = value.value("gravity", 18.0f);
                character.jump_speed = value.value("jump_speed", 6.5f);
                character.max_fall_speed = value.value("max_fall_speed", 30.0f);
                character.enabled = value.value("enabled", true);
                loaded_scene.add_component<CharacterBodyComponent>(entity, character);
            }
            if (components.contains("Persistent")) {
                const auto& value = components.at("Persistent");
                PersistentComponent persistent;
                persistent.enabled = value.value("enabled", true);
                loaded_scene.add_component<PersistentComponent>(entity, persistent);
            }
            if (components.contains("Interactable")) {
                const auto& value = components.at("Interactable");
                InteractableComponent interactable;
                interactable.prompt = value.value("prompt", std::string{"Interact"});
                interactable.enabled = value.value("enabled", true);
                loaded_scene.add_component<InteractableComponent>(entity, interactable);
            }
            if (components.contains("Door")) {
                const auto& value = components.at("Door");
                DoorComponent door;
                door.motion = door_motion_from_name(
                    value.value("motion", std::string{"rotate"})
                );
                door.open_amount = value.value("open_amount", 90.0f);
                door.duration_seconds = value.value("duration_seconds", 0.75f);
                door.required_item = value.value("required_item", std::string{});
                door.consume_required_item = value.value("consume_required_item", false);
                door.starts_open = value.value("starts_open", false);
                loaded_scene.add_component<DoorComponent>(entity, door);
            }
            if (components.contains("Health")) {
                const auto& value = components.at("Health");
                HealthComponent health;
                health.maximum = value.value("maximum", 100.0f);
                health.current = value.value("current", health.maximum);
                health.invulnerable = value.value("invulnerable", false);
                loaded_scene.add_component<HealthComponent>(entity, health);
            }
            if (components.contains("Attack")) {
                const auto& value = components.at("Attack");
                AttackComponent attack;
                attack.open_event = value.value("open_event", std::string{"AttackStart"});
                attack.close_event = value.value("close_event", std::string{"AttackEnd"});
                attack.damage = std::max(0.0f, value.value("damage", 25.0f));
                attack.active = false;
                loaded_scene.add_component<AttackComponent>(entity, std::move(attack));
            }
            if (components.contains("EnemyAI")) {
                const auto& value = components.at("EnemyAI");
                EnemyAIComponent enemy;
                enemy.detection_range = std::max(0.0f, value.value("detection_range", 10.0f));
                enemy.attack_range = std::max(0.1f, value.value("attack_range", 1.5f));
                enemy.move_speed = std::max(0.0f, value.value("move_speed", 1.8f));
                enemy.attack_damage = std::max(0.0f, value.value("attack_damage", 10.0f));
                enemy.attack_cooldown = std::max(0.05f, value.value("attack_cooldown", 1.0f));
                enemy.enabled = value.value("enabled", true);
                loaded_scene.add_component<EnemyAIComponent>(entity, enemy);
            }
            if (components.contains("GameplayVariables")) {
                GameplayVariablesComponent variables;
                const auto& value = components.at("GameplayVariables");
                if (value.contains("values") && value.at("values").is_array()) {
                    for (const auto& item : value.at("values")) {
                        GameplayVariable variable;
                        variable.name = item.value("name", std::string{"Variable"});
                        variable.number = item.value("number", 0.0f);
                        variable.boolean = item.value("boolean", false);
                        variables.values.push_back(std::move(variable));
                    }
                }
                loaded_scene.add_component<GameplayVariablesComponent>(entity, std::move(variables));
            }
            if (components.contains("TriggerVolume")) {
                const auto& value = components.at("TriggerVolume");
                TriggerVolumeComponent trigger;
                trigger.radius = std::max(0.1f, value.value("radius", 2.0f));
                trigger.enabled = value.value("enabled", true);
                trigger.fire_once = value.value("fire_once", false);
                trigger.fired = false;
                loaded_scene.add_component<TriggerVolumeComponent>(entity, trigger);
            }
            if (components.contains("SpawnPoint")) {
                const auto& value = components.at("SpawnPoint");
                SpawnPointComponent spawn;
                spawn.spawn_tag = value.value("spawn_tag", std::string{"Player"});
                spawn.enabled = value.value("enabled", true);
                loaded_scene.add_component<SpawnPointComponent>(entity, std::move(spawn));
            }
            if (components.contains("GameplayRules")) {
                const auto& value = components.at("GameplayRules");
                GameplayRulesComponent gameplay_rules;
                if (value.contains("rules") && value.at("rules").is_array()) {
                    for (const auto& item : value.at("rules")) {
                        GameplayRule rule;
                        rule.when_event = item.value("when", std::string{"Interact"});
                        rule.condition = item.value("condition", std::string{});
                        rule.condition_value = item.value("condition_value", std::string{});
                        rule.action = item.value("action", std::string{"Publish Event"});
                        rule.action_value = item.value("value", std::string{"RuleTriggered"});
                        rule.required_item = item.value("required_item", std::string{});
                        rule.quantity = std::max(1, item.value("quantity", 1));
                        rule.once = item.value("once", false);
                        rule.fired = false;
                        gameplay_rules.rules.push_back(std::move(rule));
                    }
                }
                loaded_scene.add_component<GameplayRulesComponent>(
                    entity,
                    std::move(gameplay_rules)
                );
            }
            if (components.contains("FeedbackCues")) {
                const auto& value = components.at("FeedbackCues");
                FeedbackCueComponent feedback_cues;
                if (value.contains("bindings")) {
                    for (const auto& item : value.at("bindings")) {
                        FeedbackCueBinding binding;
                        binding.event_name = item.value("event_name", std::string{"Footstep"});
                        binding.audio_asset = item.value("audio_asset", std::string{});
                        binding.visual_asset = item.value("visual_asset", std::string{});
                        binding.volume = std::max(0.0f, item.value("volume", 1.0f));
                        binding.camera_shake = std::max(0.0f, item.value("camera_shake", 0.0f));
                        feedback_cues.bindings.push_back(std::move(binding));
                    }
                }
                loaded_scene.add_component<FeedbackCueComponent>(entity, std::move(feedback_cues));
            }
            if (components.contains("SimulationProduct")) {
                const auto& value = components.at("SimulationProduct");
                ProductDefinitionComponent product;
                product.product_id = value.value("product_id", std::string{"product"});
                product.display_name = value.value("display_name", std::string{"Product"});
                product.category = value.value("category", std::string{"General"});
                product.wholesale_price = value.value("wholesale_price", 1.0f);
                product.retail_price = value.value("retail_price", 2.0f);
                product.demand_weight = std::max(
                    0.0f, value.value("demand_weight", 1.0f)
                );
                product.price_sensitivity = std::max(
                    0.0f, value.value("price_sensitivity", 1.0f)
                );
                product.max_stock = std::max(0, value.value("max_stock", 100));
                product.perishable = value.value("perishable", false);
                loaded_scene.add_component<ProductDefinitionComponent>(entity, std::move(product));
            }
            if (components.contains("ShopInventory")) {
                const auto& value = components.at("ShopInventory");
                ShopInventoryComponent inventory;
                if(value.contains("products") && value.at("products").is_array()) {
                    for(const auto& item : value.at("products")) {
                        ProductStock stock;
                        stock.product_id = item.value("product_id", std::string{"product"});
                        stock.quantity = std::max(0, item.value("quantity", 0));
                        stock.shelf_capacity = std::max(0, item.value("shelf_capacity", 20));
                        inventory.products.push_back(std::move(stock));
                    }
                }
                loaded_scene.add_component<ShopInventoryComponent>(entity, std::move(inventory));
            }
            if (components.contains("Wallet")) {
                const auto& value = components.at("Wallet");
                WalletComponent wallet;
                wallet.balance = value.value("balance", 1000.0f);
                wallet.lifetime_revenue = value.value("lifetime_revenue", 0.0f);
                wallet.lifetime_expenses = value.value("lifetime_expenses", 0.0f);
                loaded_scene.add_component<WalletComponent>(entity, wallet);
            }
            if (components.contains("ShopGoal")) {
                const auto& value = components.at("ShopGoal");
                ShopGoalComponent goal;
                goal.title = value.value("title", std::string{"Daily Sales Goal"});
                goal.target_revenue = std::max(0.0f, value.value("target_revenue", 100.0f));
                goal.target_customers = std::max(0, value.value("target_customers", 5));
                loaded_scene.add_component<ShopGoalComponent>(entity, std::move(goal));
            }
            if (components.contains("ShopMetrics")) {
                const auto& value = components.at("ShopMetrics");
                ShopMetricsComponent metrics;
                metrics.tracked_day = std::max(1, value.value("tracked_day", 1));
                metrics.customers_served_today = std::max(
                    0, value.value("customers_served_today", 0)
                );
                metrics.orders_completed_today = std::max(
                    0, value.value("orders_completed_today", 0)
                );
                metrics.products_sold_today = std::max(
                    0, value.value("products_sold_today", 0)
                );
                metrics.daily_revenue = std::max(
                    0.0f, value.value("daily_revenue", 0.0f)
                );
                metrics.daily_satisfaction = std::clamp(
                    value.value("daily_satisfaction", 100.0f),
                    0.0f,
                    100.0f
                );
                metrics.satisfaction_samples = std::max(
                    0, value.value("satisfaction_samples", 0)
                );
                loaded_scene.add_component<ShopMetricsComponent>(entity, metrics);
            }
            if (components.contains("ShopUpgrade")) {
                const auto& value = components.at("ShopUpgrade");
                ShopUpgradeComponent upgrade;
                upgrade.level = std::clamp(value.value("level", 1), 1, 1000);
                upgrade.max_level = std::clamp(value.value("max_level", 5), upgrade.level, 1000);
                upgrade.base_upgrade_cost = std::max(
                    0.0f, value.value("base_upgrade_cost", 100.0f)
                );
                upgrade.cost_multiplier = std::max(
                    1.0f, value.value("cost_multiplier", 1.5f)
                );
                upgrade.shelf_capacity_bonus = std::max(
                    0, value.value("shelf_capacity_bonus", 5)
                );
                loaded_scene.add_component<ShopUpgradeComponent>(entity, upgrade);
            }
            if (components.contains("CustomerCapacity")) {
                const auto& value = components.at("CustomerCapacity");
                CustomerCapacityComponent capacity;
                capacity.max_active_customers = std::max(
                    1, value.value("max_active_customers", 4)
                );
                capacity.max_queue_length = std::max(
                    1, value.value("max_queue_length", 3)
                );
                capacity.customers_per_upgrade = std::max(
                    0, value.value("customers_per_upgrade", 1)
                );
                capacity.arrival_rate_multiplier = std::max(
                    0.1f, value.value("arrival_rate_multiplier", 1.0f)
                );
                capacity.daily_growth_rate = std::max(
                    0.0f, value.value("daily_growth_rate", 0.05f)
                );
                loaded_scene.add_component<CustomerCapacityComponent>(entity, capacity);
            }
            if (components.contains("CustomerSpawner")) {
                const auto& value = components.at("CustomerSpawner");
                CustomerSpawnerComponent spawner;
                spawner.enabled = value.value("enabled", true);
                spawner.max_customers = std::max(
                    1, value.value("max_customers", 8)
                );
                spawner.spawn_interval_seconds = std::max(
                    0.1f, value.value("spawn_interval_seconds", 10.0f)
                );
                spawner.timer_seconds = 0.0f;
                spawner.next_customer_number = std::max(
                    1, value.value("next_customer_number", 3)
                );
                spawner.customer_template_persistent_id = value.value(
                    "customer_template_persistent_id", std::string{}
                );
                spawner.customer_name_prefix = value.value(
                    "customer_name_prefix", std::string{"Customer"}
                );
                loaded_scene.add_component<CustomerSpawnerComponent>(entity, spawner);
            }
            if (components.contains("ShopHours")) {
                const auto& value = components.at("ShopHours");
                ShopHoursComponent hours;
                hours.opening_hour = value.value("opening_hour", 8.0f);
                hours.closing_hour = value.value("closing_hour", 20.0f);
                loaded_scene.add_component<ShopHoursComponent>(entity, hours);
            }
            if (components.contains("SimulationClock")) {
                const auto& value = components.at("SimulationClock");
                SimulationClockComponent clock;
                clock.day = std::max(1, value.value("day", 1));
                clock.hour = value.value("hour", 8.0f);
                clock.time_scale = value.value("time_scale", 1.0f);
                clock.paused = value.value("paused", false);
                loaded_scene.add_component<SimulationClockComponent>(entity, clock);
            }
            if (components.contains("CheckoutStation")) {
                const auto& value = components.at("CheckoutStation");
                CheckoutStationComponent checkout;
                checkout.enabled = value.value("enabled", true);
                checkout.service_slots = std::max(
                    1, value.value("service_slots", 1)
                );
                checkout.service_time_seconds = std::max(
                    0.1f, value.value("service_time_seconds", 1.0f)
                );
                loaded_scene.add_component<CheckoutStationComponent>(entity, checkout);
            }
            if (components.contains("StationStaffing")) {
                const auto& value = components.at("StationStaffing");
                StationStaffingComponent staffing;
                staffing.requires_staff = value.value("requires_staff", false);
                staffing.required_workers = std::max(
                    1, value.value("required_workers", 1)
                );
                staffing.required_role = value.value(
                    "required_role", std::string{"Staff"}
                );
                loaded_scene.add_component<StationStaffingComponent>(entity, staffing);
            }
            if (components.contains("StaffManager")) {
                const auto& value = components.at("StaffManager");
                StaffManagerComponent staff_manager;
                staff_manager.max_staff = std::max(1, value.value("max_staff", 4));
                staff_manager.default_hiring_cost = std::max(
                    0.0f, value.value("default_hiring_cost", 100.0f)
                );
                staff_manager.default_skill_upgrade_cost = std::max(
                    0.0f, value.value("default_skill_upgrade_cost", 75.0f)
                );
                loaded_scene.add_component<StaffManagerComponent>(entity, staff_manager);
            }
            if (components.contains("StaffWorker")) {
                const auto& value = components.at("StaffWorker");
                StaffWorkerComponent worker;
                worker.enabled = value.value("enabled", true);
                worker.employed = value.value("employed", true);
                worker.role = value.value("role", std::string{"Staff"});
                worker.work_speed = std::max(
                    0.1f, value.value("work_speed", 1.0f)
                );
                worker.wage_per_hour = std::max(
                    0.0f, value.value("wage_per_hour", 10.0f)
                );
                worker.shift_start_hour = value.value("shift_start_hour", 8.0f);
                worker.shift_end_hour = value.value("shift_end_hour", 20.0f);
                worker.last_paid_day = std::max(
                    1, value.value("last_paid_day", 1)
                );
                worker.max_skill_level = std::clamp(
                    value.value("max_skill_level", 5), 1, 100
                );
                worker.skill_level = std::clamp(
                    value.value("skill_level", 1), 1, worker.max_skill_level
                );
                worker.hiring_cost = std::max(
                    0.0f, value.value("hiring_cost", 100.0f)
                );
                worker.skill_upgrade_cost = std::max(
                    0.0f, value.value("skill_upgrade_cost", 75.0f)
                );
                worker.assigned_station_persistent_id = value.value(
                    "assigned_station_persistent_id", std::string{}
                );
                loaded_scene.add_component<StaffWorkerComponent>(entity, worker);
            }
            if (components.contains("CustomerOrder")) {
                const auto& value = components.at("CustomerOrder");
                CustomerOrderComponent order;
                order.order_id = value.value("order_id", std::string{"order-1"});
                order.customer_name = value.value("customer_name", std::string{"Customer"});
                order.fulfilled = value.value("fulfilled", false);
                order.delivered = value.value("delivered", false);
                if(value.contains("lines") && value.at("lines").is_array()) {
                    for(const auto& item : value.at("lines")) {
                        OrderLine line;
                        line.product_id = item.value("product_id", std::string{"product"});
                        line.quantity = std::max(1, item.value("quantity", 1));
                        line.fulfilled_quantity = std::clamp(
                            item.value("fulfilled_quantity", 0),
                            0,
                            line.quantity
                        );
                        line.unit_price = item.value("unit_price", 0.0f);
                        order.lines.push_back(std::move(line));
                    }
                }
                loaded_scene.add_component<CustomerOrderComponent>(entity, std::move(order));
            }
            if (components.contains("CustomerBehavior")) {
                const auto& value = components.at("CustomerBehavior");
                CustomerBehaviorComponent customer;
                customer.enabled = value.value("enabled", true);
                customer.arrival_interval_seconds = std::max(
                    0.1f, value.value("arrival_interval_seconds", 12.0f)
                );
                customer.shopping_duration_seconds = std::max(
                    0.1f, value.value("shopping_duration_seconds", 4.0f)
                );
                customer.leave_duration_seconds = std::max(
                    0.1f, value.value("leave_duration_seconds", 5.0f)
                );
                customer.timer_seconds = 0.0f;
                customer.state = value.value("state", std::string{"waiting"});
                customer.order_sequence = std::max(1, value.value("order_sequence", 1));
                loaded_scene.add_component<CustomerBehaviorComponent>(entity, customer);
            }
            if (components.contains("Inventory")) {
                const auto& value = components.at("Inventory");
                InventoryComponent inventory;
                if (value.contains("items")) {
                    inventory.items = value.at("items")
                        .get<std::unordered_map<std::string, int>>();
                }
                loaded_scene.add_component<InventoryComponent>(entity, std::move(inventory));
            }
            if (components.contains("Pickup")) {
                const auto& value = components.at("Pickup");
                PickupComponent pickup;
                pickup.item_id = value.value("item_id", std::string{"Item"});
                pickup.display_name = value.value("display_name", pickup.item_id);
                pickup.quantity = std::max(1, value.value("quantity", 1));
                pickup.destroy_on_pickup = value.value("destroy_on_pickup", true);
                loaded_scene.add_component<PickupComponent>(entity, std::move(pickup));
            }
            if (components.contains("Dialogue")) {
                const auto& value = components.at("Dialogue");
                DialogueComponent dialogue;
                dialogue.repeatable = value.value("repeatable", true);
                dialogue.interaction_prompt = value.value("interaction_prompt", std::string{"Talk"});
                dialogue.quest_id_to_start = value.value("quest_id_to_start", std::string{});
                if (value.contains("lines") && value.at("lines").is_array()) {
                    for (const auto& line_value : value.at("lines")) {
                        DialogueLine line;
                        line.speaker = line_value.value("speaker", std::string{"Speaker"});
                        line.text = line_value.value("text", std::string{"..."});
                        if (line_value.contains("choices") && line_value.at("choices").is_array()) {
                            for (const auto& choice_value : line_value.at("choices")) {
                                DialogueChoice choice;
                                choice.label = choice_value.value("label", std::string{"Continue"});
                                choice.next_line = choice_value.value("next_line", -1);
                                line.choices.push_back(std::move(choice));
                            }
                        }
                        dialogue.lines.push_back(std::move(line));
                    }
                }
                loaded_scene.add_component<DialogueComponent>(entity, std::move(dialogue));
            }
            if (components.contains("Quest")) {
                const auto& value = components.at("Quest");
                QuestComponent quest;
                quest.quest_id = value.value("quest_id", std::string{"quest"});
                quest.title = value.value("title", std::string{"New Quest"});
                quest.description = value.value("description", std::string{});
                quest.required_item = value.value("required_item", std::string{});
                quest.required_quantity = std::max(1, value.value("required_quantity", 1));
                quest.reward_item = value.value("reward_item", std::string{});
                quest.reward_quantity = std::max(1, value.value("reward_quantity", 1));
                quest.status = quest_status_from_name(value.value("status", std::string{"inactive"}));
                loaded_scene.add_component<QuestComponent>(entity, std::move(quest));
            }
            if (components.contains("PlayerController")) {
                const auto& value = components.at("PlayerController");
                PlayerControllerComponent player;
                player.view_mode = player_view_mode_from_name(
                    value.value("view_mode", std::string{"first_person"})
                );
                player.move_speed = value.value("move_speed", 4.0f);
                player.fast_multiplier = value.value("fast_multiplier", 2.5f);
                player.look_sensitivity = value.value("look_sensitivity", 0.12f);
                player.interaction_distance = value.value("interaction_distance", 4.0f);
                player.interaction_radius = value.value("interaction_radius", 1.25f);
                player.camera_distance = value.value("camera_distance", 4.5f);
                player.camera_height = value.value("camera_height", 1.55f);
                player.camera_shoulder_offset = value.value("camera_shoulder_offset", 0.0f);
                player.camera_min_pitch = value.value("camera_min_pitch", -60.0f);
                player.camera_max_pitch = value.value("camera_max_pitch", 70.0f);
                player.orient_to_movement = value.value("orient_to_movement", true);
                player.enabled = value.value("enabled", true);
                loaded_scene.add_component<PlayerControllerComponent>(entity, player);
            }
        }

        scene = std::move(loaded_scene);
        return true;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

} // namespace seed
