#include "PlayChangesReview.h"
#include "SemanticMergeReview.h"
#include "StudioShaders.h"
#include "StudioUI.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/animation/AnimationSystem.h"
#include "seed/assets/AssetImporter.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/core/Engine.h"
#include "seed/diagnostics/SeedDoctor.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/gameplay/CombatSystem.h"
#include "seed/gameplay/GameplayRulesSystem.h"
#include "seed/gameplay/GameplayHUD.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/project/ProjectSerializer.h"
#include "seed/project/SceneSerializer.h"
#include "seed/project/PrefabSerializer.h"
#include "seed/project/SceneRevision.h"
#include "seed/render/RenderComponents.h"
#include "seed/render/RenderSystem.h"
#include "seed/render/SkinnedShaders.h"
#include "seed/scene/SceneDiff.h"
#include "seed/simulation/SimulationComponents.h"
#include "seed/simulation/SimulationSystem.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::uint64_t parse_frame_limit(int argc, char** argv) {
    constexpr std::string_view prefix{"--frames="};
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (!argument.starts_with(prefix)) continue;
        std::uint64_t value = 0;
        const auto number = argument.substr(prefix.size());
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
        if (result.ec == std::errc{} && result.ptr == number.data() + number.size()) return value;
    }
    return 0;
}

struct CameraInputState {
    bool forward{false};
    bool backward{false};
    bool left{false};
    bool right{false};
    bool down{false};
    bool up{false};
    bool fast{false};
    bool looking{false};
    bool has_mouse_position{false};
    float last_mouse_x{0.0f};
    float last_mouse_y{0.0f};
    float move_speed{3.5f};
};

struct WorkspaceState {
    seed::ProjectDescriptor project{};
    std::filesystem::path project_file;
    std::filesystem::path scene_file;
    bool dirty{true};
    std::string status_message{"Unsaved Seed project."};
};

void set_key_state(CameraInputState& input, seed::KeyCode key, bool down) {
    switch (key) {
    case seed::KeyCode::W: input.forward = down; break;
    case seed::KeyCode::S: input.backward = down; break;
    case seed::KeyCode::A: input.left = down; break;
    case seed::KeyCode::D: input.right = down; break;
    case seed::KeyCode::Q: input.down = down; break;
    case seed::KeyCode::E: input.up = down; break;
    case seed::KeyCode::LeftShift:
    case seed::KeyCode::RightShift: input.fast = down; break;
    default: break;
    }
}

void clear_movement_input(CameraInputState& input) {
    input.forward = false;
    input.backward = false;
    input.left = false;
    input.right = false;
    input.down = false;
    input.up = false;
    input.fast = false;
}

void update_camera_movement(
    seed::TransformComponent& transform,
    const CameraInputState& input,
    double delta_seconds
) {
    const float distance = input.move_speed * (input.fast ? 3.0f : 1.0f) *
        static_cast<float>(delta_seconds);
    const seed::Vec3 forward = seed::forward_from_euler(transform.rotation_degrees);
    const seed::Vec3 right = seed::right_from_euler(transform.rotation_degrees);
    const seed::Vec3 up{0.0f, 1.0f, 0.0f};
    if (input.forward) transform.position += forward * distance;
    if (input.backward) transform.position += forward * -distance;
    if (input.right) transform.position += right * distance;
    if (input.left) transform.position += right * -distance;
    if (input.up) transform.position += up * distance;
    if (input.down) transform.position += up * -distance;
}

std::string project_file_stem(std::string name) {
    if (name.empty()) return "SeedProject";
    for (char& character : name) {
        const unsigned char value = static_cast<unsigned char>(character);
        if (!std::isalnum(value) && character != '-' && character != '_') character = '_';
    }
    return name;
}

seed::EntityId create_editor_camera(seed::Scene& scene) {
    const auto entity = scene.create_entity("Editor Camera");
    auto& transform = scene.add_component<seed::TransformComponent>(entity);
    transform.position = {0.0f, 0.0f, 4.2f};
    transform.rotation_degrees = {-8.0f, 0.0f, 0.0f};
    seed::CameraComponent camera;
    camera.primary = true;
    camera.enabled = true;
    camera.editor_only = true;
    scene.add_component<seed::CameraComponent>(entity, camera);
    return entity;
}

void create_default_lighting(seed::Scene& scene) {
    const auto sun = scene.create_entity("Sun");
    auto& transform = scene.add_component<seed::TransformComponent>(sun);
    transform.rotation_degrees = {-48.0f, 32.0f, 0.0f};
    scene.add_component<seed::DirectionalLightComponent>(sun);

    const auto ambient = scene.create_entity("Ambient Light");
    scene.add_component<seed::AmbientLightComponent>(ambient);

    const auto sky = scene.create_entity("Sky");
    scene.add_component<seed::SkyComponent>(sky);
}

seed::EntityId find_editor_camera(seed::Scene& scene) {
    seed::EntityId result = seed::InvalidEntity;
    scene.for_each<seed::TransformComponent, seed::CameraComponent>(
        [&](seed::EntityId entity, seed::TransformComponent&, seed::CameraComponent& camera) {
            if (result == seed::InvalidEntity && camera.editor_only) result = entity;
        }
    );
    return result;
}

seed::EntityId ensure_editor_camera(seed::Scene& scene) {
    const auto existing = find_editor_camera(scene);
    return existing != seed::InvalidEntity ? existing : create_editor_camera(scene);
}

void make_primary_game_camera(seed::Scene& scene, seed::EntityId player_entity) {
    scene.for_each<seed::CameraComponent>([&](seed::EntityId entity, seed::CameraComponent& camera) {
        if (!camera.editor_only) camera.primary = entity == player_entity;
    });
}

seed::EntityId create_first_person_player(
    seed::Scene& scene,
    seed::EntityId source_camera,
    std::string name = "Player",
    bool add_character_physics = true
) {
    seed::TransformComponent spawn;
    if (const auto* source = scene.get_component<seed::TransformComponent>(source_camera)) {
        spawn = *source;
    } else {
        spawn.position = {0.0f, 1.7f, 4.2f};
        spawn.rotation_degrees = {};
    }
    spawn.scale = {1.0f, 1.0f, 1.0f};
    if (add_character_physics) spawn.position.y = std::max(spawn.position.y, 1.65f);

    const auto player = scene.create_entity(std::move(name));
    scene.add_component<seed::TransformComponent>(player, spawn);
    seed::CameraComponent camera;
    camera.primary = true;
    camera.enabled = true;
    camera.editor_only = false;
    camera.field_of_view_degrees = 65.0f;
    scene.add_component<seed::CameraComponent>(player, camera);
    scene.add_component<seed::PlayerControllerComponent>(player);
    scene.add_component<seed::InventoryComponent>(player);
    if (add_character_physics) scene.add_component<seed::CharacterBodyComponent>(player);
    make_primary_game_camera(scene, player);
    return player;
}

seed::EntityId create_third_person_player(
    seed::Scene& scene,
    seed::EntityId source_camera,
    std::string name = "Third-Person Player"
) {
    seed::TransformComponent spawn;
    if (const auto* source = scene.get_component<seed::TransformComponent>(source_camera)) {
        spawn = *source;
    } else {
        spawn.position = {0.0f, 0.9f, 4.2f};
        spawn.rotation_degrees = {};
    }
    spawn.scale = {1.0f, 1.0f, 1.0f};
    spawn.position.y = std::max(spawn.position.y, 0.9f);

    const auto player = scene.create_entity(std::move(name));
    scene.add_component<seed::TransformComponent>(player, spawn);

    seed::CameraComponent camera;
    camera.primary = true;
    camera.enabled = true;
    camera.editor_only = false;
    camera.field_of_view_degrees = 65.0f;
    scene.add_component<seed::CameraComponent>(player, camera);

    seed::PlayerControllerComponent controller;
    controller.view_mode = seed::PlayerViewMode::ThirdPerson;
    controller.camera_distance = 4.5f;
    controller.camera_height = 1.55f;
    controller.orient_to_movement = true;
    scene.add_component<seed::PlayerControllerComponent>(player, controller);
    scene.add_component<seed::InventoryComponent>(player);

    seed::CharacterBodyComponent body;
    body.height = 1.8f;
    body.eye_height = 1.55f;
    scene.add_component<seed::CharacterBodyComponent>(player, body);

    seed::SkinnedMeshComponent character;
    character.visible = true;
    character.cast_shadows = true;
    character.receive_shadows = true;
    scene.add_component<seed::SkinnedMeshComponent>(player, character);

    seed::AnimationPlayerComponent animation;
    animation.locomotion_enabled = true;
    animation.blend_duration_seconds = 0.15f;
    scene.add_component<seed::AnimationPlayerComponent>(player, animation);

    const auto camera_entity = scene.create_entity("Third-Person Camera");
    seed::TransformComponent camera_transform;
    camera_transform.position = {0.0f, 1.55f, -4.5f};
    camera_transform.rotation_degrees = {-12.0f, 0.0f, 0.0f};
    scene.add_component<seed::TransformComponent>(camera_entity, camera_transform);
    scene.add_component<seed::ParentComponent>(
        camera_entity,
        seed::ParentComponent{scene.entity_persistent_id(player)}
    );

    seed::CameraComponent child_camera;
    child_camera.enabled = true;
    child_camera.primary = false;
    child_camera.editor_only = false;
    child_camera.field_of_view_degrees = 65.0f;
    scene.add_component<seed::CameraComponent>(camera_entity, child_camera);

    make_primary_game_camera(scene, player);
    return player;
}

seed::EntityId find_authored_player(seed::Scene& scene) {
    seed::EntityId fallback = seed::InvalidEntity;
    seed::EntityId primary = seed::InvalidEntity;
    scene.for_each<seed::TransformComponent, seed::CameraComponent, seed::PlayerControllerComponent>(
        [&](seed::EntityId entity, seed::TransformComponent&, seed::CameraComponent& camera,
            seed::PlayerControllerComponent& controller) {
            if (camera.editor_only || !camera.enabled || !controller.enabled) return;
            if (fallback == seed::InvalidEntity) fallback = entity;
            if (camera.primary) primary = entity;
        }
    );
    return primary != seed::InvalidEntity ? primary : fallback;
}

seed::EntityId create_temporary_play_player(seed::Scene& scene, seed::EntityId source_camera) {
    return create_first_person_player(scene, source_camera, "Play Player", false);
}

void remove_temporary_play_artifacts(
    seed::Scene& runtime_scene,
    const seed::Scene& authored_scene,
    const std::string& temporary_player_persistent_id
) {
    if (!temporary_player_persistent_id.empty()) {
        const seed::EntityId temporary =
            runtime_scene.find_entity_by_persistent_id(temporary_player_persistent_id);
        if (temporary != seed::InvalidEntity) runtime_scene.destroy_entity(temporary);
    }

    // Creating Seed's fallback Play Player temporarily makes it the primary game
    // camera. Restore authored camera flags before semantic diffing so the review
    // contains only creator/gameplay changes, not Seed's own fallback plumbing.
    authored_scene.for_each<seed::CameraComponent>(
        [&](seed::EntityId authored_entity, const seed::CameraComponent& authored_camera) {
            const std::string& persistent_id = authored_scene.entity_persistent_id(authored_entity);
            const seed::EntityId runtime_entity =
                runtime_scene.find_entity_by_persistent_id(persistent_id);
            if (runtime_entity == seed::InvalidEntity) return;
            if (auto* runtime_camera = runtime_scene.get_component<seed::CameraComponent>(runtime_entity)) {
                runtime_camera->primary = authored_camera.primary;
            }
        }
    );
}

seed::EntityId create_cube(
    seed::Scene& scene,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle shader,
    std::string name = "Seed Cube"
) {
    const auto cube = scene.create_entity(std::move(name));
    auto& transform = scene.add_component<seed::TransformComponent>(cube);
    transform.rotation_degrees = {20.0f, 32.0f, 0.0f};
    seed::MeshComponent mesh;
    mesh.mesh = cube_mesh;
    mesh.visible = true;
    mesh.asset_id = "builtin:cube";
    scene.add_component<seed::MeshComponent>(cube, mesh);
    seed::MaterialComponent material;
    material.shader = shader;
    material.asset_id = "builtin:seed_default";
    material.roughness = 0.55f;
    scene.add_component<seed::MaterialComponent>(cube, material);
    return cube;
}

seed::EntityId create_beginner_preset(
    seed::Scene& scene,
    const std::string& preset,
    seed::EntityId editor_camera,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle shader
) {
    if (preset == "player_first") {
        return create_first_person_player(scene, editor_camera, "Player", true);
    }
    if (preset == "player_third") {
        return create_third_person_player(scene, editor_camera, "Player");
    }
    if (preset == "simulation_shop") {
        const auto entity = scene.create_entity("Shop");
        scene.add_component<seed::TransformComponent>(entity);
        scene.add_component<seed::ShopInventoryComponent>(entity);
        scene.add_component<seed::WalletComponent>(entity);
        scene.add_component<seed::ShopHoursComponent>(entity);
        scene.add_component<seed::SimulationClockComponent>(entity);
        return entity;
    }
    if (preset == "simulation_product") {
        const auto entity = create_cube(scene, cube_mesh, shader, "Product");
        seed::ProductDefinitionComponent product;
        product.product_id = "coffee";
        product.display_name = "Coffee";
        product.category = "Food";
        product.wholesale_price = 2.0f;
        product.retail_price = 5.0f;
        product.max_stock = 50;
        scene.add_component<seed::ProductDefinitionComponent>(entity, product);
        return entity;
    }
    if (preset == "simulation_checkout") {
        const auto entity = create_cube(scene, cube_mesh, shader, "Checkout Counter");
        if (auto* transform = scene.get_component<seed::TransformComponent>(entity)) {
            transform->position = {0.0f, 0.55f, 0.35f};
            transform->scale = {1.5f, 0.55f, 0.5f};
        }
        scene.add_component<seed::CheckoutStationComponent>(entity);
        scene.add_component<seed::InteractableComponent>(
            entity,
            "Checkout Customer",
            true
        );
        return entity;
    }
    if (preset == "simulation_upgrade") {
        const auto entity = create_cube(scene, cube_mesh, shader, "Shop Upgrade Board");
        if (auto* transform = scene.get_component<seed::TransformComponent>(entity)) {
            transform->position = {3.6f, 0.8f, -1.0f};
            transform->scale = {0.7f, 0.9f, 0.25f};
        }
        scene.add_component<seed::ShopUpgradeComponent>(entity);
        scene.add_component<seed::InteractableComponent>(
            entity,
            "Upgrade Shop",
            true
        );
        return entity;
    }
    if (preset == "simulation_order") {
        const auto entity = scene.create_entity("Customer Order");
        seed::CustomerOrderComponent order;
        order.order_id = "order-1";
        order.customer_name = "Customer";
        order.lines = {{"coffee", 1, 0, 5.0f}};
        scene.add_component<seed::CustomerOrderComponent>(entity, order);
        return entity;
    }

    const bool is_enemy =
        preset == "enemy" ||
        preset == "enemy_basic" ||
        preset == "enemy_fast" ||
        preset == "enemy_boss";
    const std::string name =
        preset == "enemy_fast" ? "Fast Enemy" :
        preset == "enemy_boss" ? "Boss Enemy" :
        preset == "enemy_basic" || preset == "enemy" ? "Basic Enemy" :
        preset == "pickup" ? "Pickup" :
        preset == "door" ? "Door" :
        preset == "npc" ? "NPC" :
        "Gameplay Object";

    if (preset == "quest") {
        const auto quest = scene.create_entity("Quest");
        scene.add_component<seed::QuestComponent>(quest);
        return quest;
    }

    const auto entity = create_cube(scene, cube_mesh, shader, name);
    if (auto* transform = scene.get_component<seed::TransformComponent>(entity)) {
        transform->position = {0.0f, 0.75f, -2.5f};
    }

    if (is_enemy) {
        seed::HealthComponent health;
        seed::AttackComponent attack;
        seed::EnemyAIComponent ai;
        if (preset == "enemy_fast") {
            health.maximum = 80.0f;
            health.current = 80.0f;
            attack.damage = 18.0f;
            ai.detection_range = 14.0f;
            ai.attack_range = 1.6f;
            ai.move_speed = 3.5f;
            ai.attack_damage = 14.0f;
            ai.attack_cooldown = 0.8f;
        } else if (preset == "enemy_boss") {
            health.maximum = 250.0f;
            health.current = 250.0f;
            attack.damage = 35.0f;
            ai.detection_range = 18.0f;
            ai.attack_range = 2.0f;
            ai.move_speed = 1.2f;
            ai.attack_damage = 25.0f;
            ai.attack_cooldown = 1.5f;
            if (auto* transform = scene.get_component<seed::TransformComponent>(entity)) {
                transform->scale = {1.5f, 2.0f, 1.5f};
            }
        }
        scene.add_component<seed::HealthComponent>(entity, health);
        scene.add_component<seed::AttackComponent>(entity, attack);
        scene.add_component<seed::EnemyAIComponent>(entity, ai);
    } else if (preset == "pickup") {
        scene.add_component<seed::InteractableComponent>(entity, "Pick Up Item", true);
        seed::PickupComponent pickup;
        pickup.item_id = "Coin";
        pickup.display_name = "Coin";
        scene.add_component<seed::PickupComponent>(entity, pickup);
    } else if (preset == "door") {
        scene.add_component<seed::InteractableComponent>(entity, "Open Door", true);
        scene.add_component<seed::BoxColliderComponent>(entity);
        scene.add_component<seed::DoorComponent>(entity);
    } else if (preset == "npc") {
        scene.add_component<seed::InteractableComponent>(entity, "Talk", true);
        seed::DialogueComponent dialogue;
        dialogue.interaction_prompt = "Talk";
        dialogue.lines = {
            seed::DialogueLine{
                .speaker = "Guide",
                .text = "Welcome to your first Seed adventure.",
                .choices = {seed::DialogueChoice{.label = "Continue", .next_line = 1}}
            },
            seed::DialogueLine{
                .speaker = "Guide",
                .text = "Collect items, explore the world, and complete your goal.",
                .choices = {}
            }
        };
        scene.add_component<seed::DialogueComponent>(entity, dialogue);
    }
    return entity;
}

void ensure_beginner_prefab_library(
    const std::filesystem::path& project_root,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle shader
) {
    struct StarterPrefab {
        const char* id;
        const char* file;
    };
    constexpr StarterPrefab starters[] = {
        {"player_first", "Player_FirstPerson.seedprefab"},
        {"player_third", "Player_ThirdPerson.seedprefab"},
        {"enemy_basic", "Enemy_Basic.seedprefab"},
        {"enemy_fast", "Enemy_Fast.seedprefab"},
        {"enemy_boss", "Enemy_Boss.seedprefab"},
        {"pickup", "Pickup.seedprefab"},
        {"door", "Door.seedprefab"},
        {"npc", "NPC_Dialogue.seedprefab"},
        {"quest", "Quest.seedprefab"}
    };

    try {
        const auto prefab_directory = project_root / "Prefabs";
        std::filesystem::create_directories(prefab_directory);
        for(const auto& starter : starters) {
            const auto prefab_file = prefab_directory / starter.file;
            if(std::filesystem::exists(prefab_file)) continue;

            seed::Scene prefab_scene;
            const auto editor_camera = create_editor_camera(prefab_scene);
            const auto entity = create_beginner_preset(
                prefab_scene,
                starter.id,
                editor_camera,
                cube_mesh,
                shader
            );
            if(entity == seed::InvalidEntity) continue;

            std::string error;
            if(!seed::PrefabSerializer::save(prefab_scene, entity, prefab_file, &error)) {
                std::cerr << "[SeedStudio] Could not create starter prefab "
                          << prefab_file << ": " << error << "\n";
            }
        }
    } catch(const std::exception& exception) {
        std::cerr << "[SeedStudio] Could not create starter prefab library: "
                  << exception.what() << "\n";
    }
}

void build_project_template(
    seed::Scene& scene,
    const std::string& template_id,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle shader
) {
    scene.clear();
    const auto editor_camera = create_editor_camera(scene);
    create_default_lighting(scene);

    if (template_id == "empty") {
        create_cube(scene, cube_mesh, shader, "Starter Cube");
        return;
    }

    seed::EntityId player = seed::InvalidEntity;
    if (template_id == "first_person_exploration" || template_id == "shop_simulator") {
        player = create_first_person_player(scene, editor_camera, "Player", true);
    } else {
        player = create_third_person_player(scene, editor_camera, "Player");
    }

    if (auto* health = scene.get_component<seed::HealthComponent>(player)) {
        health->maximum = 100.0f;
        health->current = 100.0f;
    } else {
        scene.add_component<seed::HealthComponent>(player);
    }

    if (template_id == "shop_simulator") {
        const auto floor = create_cube(scene, cube_mesh, shader, "Shop Floor");
        auto* floor_transform = scene.get_component<seed::TransformComponent>(floor);
        floor_transform->position = {0.0f, -0.1f, -2.5f};
        floor_transform->scale = {6.0f, 0.1f, 6.0f};

        const auto shop = scene.create_entity("Corner Shop");
        scene.add_component<seed::TransformComponent>(shop);
        seed::ShopInventoryComponent inventory;
        inventory.products = {
            {"coffee", 12, 12},
            {"snack", 8, 8},
            {"card_pack", 5, 5}
        };
        scene.add_component<seed::ShopInventoryComponent>(shop, inventory);
        seed::CustomerCapacityComponent capacity;
        capacity.max_active_customers = 4;
        capacity.max_queue_length = 3;
        capacity.customers_per_upgrade = 1;
        capacity.arrival_rate_multiplier = 1.0f;
        capacity.daily_growth_rate = 0.05f;
        scene.add_component<seed::CustomerCapacityComponent>(shop, capacity);
        seed::WalletComponent wallet;
        wallet.balance = 250.0f;
        scene.add_component<seed::WalletComponent>(shop, wallet);
        scene.add_component<seed::ShopMetricsComponent>(shop);
        scene.add_component<seed::ShopHoursComponent>(shop);
        scene.add_component<seed::SimulationClockComponent>(shop);
        scene.add_component<seed::InteractableComponent>(shop, "Restock Shop", true);

        const auto add_product = [&](const char* id, const char* name, const char* category,
                                     float wholesale, float retail, int stock, seed::Vec3 position) {
            const auto product = create_cube(scene, cube_mesh, shader, name);
            auto* transform = scene.get_component<seed::TransformComponent>(product);
            transform->position = position;
            transform->scale = {0.65f, 0.65f, 0.65f};
            seed::ProductDefinitionComponent definition;
            definition.product_id = id;
            definition.display_name = name;
            definition.category = category;
            definition.wholesale_price = wholesale;
            definition.retail_price = retail;
            definition.max_stock = 50;
            scene.add_component<seed::ProductDefinitionComponent>(product, definition);
            scene.add_component<seed::InteractableComponent>(product, "Sell Product", true);
            (void)stock;
            return product;
        };
        add_product("coffee", "Coffee", "Food", 2.0f, 5.0f, 12, {-2.2f, 0.65f, -2.0f});
        add_product("snack", "Snack", "Food", 1.0f, 3.0f, 8, {0.0f, 0.65f, -2.0f});
        add_product("card_pack", "Card Pack", "TCG", 3.0f, 8.0f, 5, {2.2f, 0.65f, -2.0f});

        const auto checkout = create_cube(scene, cube_mesh, shader, "Checkout Counter");
        auto* checkout_transform = scene.get_component<seed::TransformComponent>(checkout);
        checkout_transform->position = {0.0f, 0.55f, 0.35f};
        checkout_transform->scale = {1.5f, 0.55f, 0.5f};
        scene.add_component<seed::CheckoutStationComponent>(checkout);
        scene.add_component<seed::InteractableComponent>(
            checkout,
            "Checkout Customer",
            true
        );

        const checkout_two = create_cube(scene, cube_mesh, shader, "Checkout Counter 2");
        auto* checkout_two_transform = scene.get_component<seed::TransformComponent>(checkout_two);
        checkout_two_transform->position = {2.0f, 0.55f, 0.35f};
        checkout_two_transform->scale = {1.5f, 0.55f, 0.5f};
        scene.add_component<seed::CheckoutStationComponent>(checkout_two);
        scene.add_component<seed::InteractableComponent>(
            checkout_two,
            "Checkout Customer",
            true
        );

        const auto upgrade_board = create_cube(scene, cube_mesh, shader, "Shop Upgrade Board");
        auto* upgrade_transform = scene.get_component<seed::TransformComponent>(upgrade_board);
        upgrade_transform->position = {3.6f, 0.8f, -1.0f};
        upgrade_transform->scale = {0.7f, 0.9f, 0.25f};
        scene.add_component<seed::ShopUpgradeComponent>(upgrade_board);
        scene.add_component<seed::InteractableComponent>(
            upgrade_board,
            "Upgrade Shop",
            true
        );

        const auto add_customer = [&](const char* name, seed::Vec3 position, float arrival_interval) {
            const auto customer = create_cube(scene, cube_mesh, shader, name);
            auto* customer_transform = scene.get_component<seed::TransformComponent>(customer);
            customer_transform->position = position;
            customer_transform->scale = {0.55f, 0.9f, 0.55f};
            seed::CustomerBehaviorComponent behavior;
            behavior.arrival_interval_seconds = arrival_interval;
            behavior.shopping_duration_seconds = 3.0f;
            behavior.leave_duration_seconds = 4.0f;
            scene.add_component<seed::CustomerBehaviorComponent>(customer, behavior);
            seed::CustomerOrderComponent order;
            order.order_id = "customer-1";
            order.customer_name = name;
            order.lines = {{"coffee", 1, 0, 5.0f}};
            scene.add_component<seed::CustomerOrderComponent>(customer, order);
            return customer;
        };
        const auto customer_a = add_customer(
            "Customer A",
            {-4.0f, 0.9f, -4.0f},
            5.0f
        );
        add_customer("Customer B", {-5.0f, 0.9f, -3.2f}, 10.0f);
        seed::CustomerSpawnerComponent spawner;
        spawner.max_customers = 8;
        spawner.spawn_interval_seconds = 8.0f;
        spawner.next_customer_number = 3;
        spawner.customer_template_persistent_id = scene.entity_persistent_id(customer_a);
        spawner.customer_name_prefix = "Customer";
        scene.add_component<seed::CustomerSpawnerComponent>(shop, spawner);
        return;
    }

    if (template_id == "third_person_adventure") {
        const auto target = create_cube(scene, cube_mesh, shader, "Training Enemy");
        auto* target_transform = scene.get_component<seed::TransformComponent>(target);
        target_transform->position = {0.0f, 0.75f, -2.5f};
        target_transform->scale = {1.0f, 1.5f, 1.0f};
        scene.add_component<seed::HealthComponent>(target);
        scene.add_component<seed::AttackComponent>(target);
        scene.add_component<seed::EnemyAIComponent>(target);
        return;
    }

    const auto key = create_cube(scene, cube_mesh, shader, "Starter Key");
    auto* key_transform = scene.get_component<seed::TransformComponent>(key);
    key_transform->position = {2.0f, 0.35f, -2.0f};
    key_transform->scale = {0.35f, 0.35f, 0.35f};
    scene.add_component<seed::InteractableComponent>(
        key,
        "Pick Up Key",
        true
    );
    seed::PickupComponent pickup;
    pickup.item_id = "Key";
    pickup.display_name = "Key";
    scene.add_component<seed::PickupComponent>(key, pickup);

    const auto door = create_cube(scene, cube_mesh, shader, "Locked Door");
    auto* door_transform = scene.get_component<seed::TransformComponent>(door);
    door_transform->position = {0.0f, 1.0f, -6.0f};
    door_transform->scale = {1.0f, 2.0f, 0.2f};
    scene.add_component<seed::BoxColliderComponent>(door);
    scene.add_component<seed::InteractableComponent>(
        door,
        "Open Locked Door",
        true
    );
    seed::DoorComponent door_component;
    door_component.required_item = "Key";
    door_component.open_amount = 90.0f;
    scene.add_component<seed::DoorComponent>(door, door_component);

    const auto quest_entity = scene.create_entity("Starter Quest");
    seed::QuestComponent quest;
    quest.quest_id = "collect_key";
    quest.title = "Find the Key";
    quest.description = "Find the key and open the locked door.";
    quest.required_item = "Key";
    quest.required_quantity = 1;
    quest.status = seed::QuestStatus::Active;
    scene.add_component<seed::QuestComponent>(quest_entity, quest);
}

void bind_builtin_resources(
    seed::Scene& scene,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle default_shader
) {
    scene.for_each<seed::MeshComponent>([&](seed::EntityId, seed::MeshComponent& mesh) {
        if (mesh.asset_id == "builtin:cube") mesh.mesh = cube_mesh;
        else if (!seed::AssetRuntime::is_model_asset_id(mesh.asset_id)) mesh.mesh = {};
    });

    scene.for_each<seed::MaterialComponent>([&](seed::EntityId, seed::MaterialComponent& material) {
        if (material.asset_id == "builtin:seed_default") {
            material.shader = default_shader;
            material.base_color_texture = {};
            material.metallic_roughness_texture = {};
            material.normal_texture = {};
            material.use_asset_defaults = false;
        } else if (!seed::AssetRuntime::is_model_asset_id(material.asset_id) &&
                   !seed::AssetRuntime::is_character_asset_id(material.asset_id)) {
            material.shader = {};
            material.base_color_texture = {};
            material.metallic_roughness_texture = {};
            material.normal_texture = {};
        }
    });
}

seed::EntityId first_content_entity(seed::Scene& scene) {
    seed::EntityId result = seed::InvalidEntity;
    scene.for_each_entity([&](seed::EntityId entity, const std::string&) {
        if (result != seed::InvalidEntity) return;
        const auto* camera = scene.get_component<seed::CameraComponent>(entity);
        if (camera == nullptr || !camera->editor_only) result = entity;
    });
    return result;
}

bool is_editor_only_entity(seed::Scene& scene, seed::EntityId entity) {
    if (entity == seed::InvalidEntity || !scene.is_alive(entity)) return false;
    const auto* camera = scene.get_component<seed::CameraComponent>(entity);
    return camera != nullptr && camera->editor_only;
}

seed::EntityId duplicate_entity(seed::Scene& scene, seed::EntityId source) {
    if (source == seed::InvalidEntity || !scene.is_alive(source) || is_editor_only_entity(scene, source)) {
        return seed::InvalidEntity;
    }

    const auto copy = scene.create_entity(scene.entity_name(source) + " Copy");
    if (const auto* v = scene.get_component<seed::TransformComponent>(source)) {
        scene.add_component<seed::TransformComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::CameraComponent>(source)) {
        auto camera = *v;
        camera.primary = false;
        camera.editor_only = false;
        scene.add_component<seed::CameraComponent>(copy, camera);
    }
    if (const auto* v = scene.get_component<seed::DirectionalLightComponent>(source)) {
        scene.add_component<seed::DirectionalLightComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::AmbientLightComponent>(source)) {
        scene.add_component<seed::AmbientLightComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::SkyComponent>(source)) {
        scene.add_component<seed::SkyComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::MeshComponent>(source)) {
        scene.add_component<seed::MeshComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::MaterialComponent>(source)) {
        scene.add_component<seed::MaterialComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::BoxColliderComponent>(source)) {
        scene.add_component<seed::BoxColliderComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::CharacterBodyComponent>(source)) {
        scene.add_component<seed::CharacterBodyComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::InteractableComponent>(source)) {
        scene.add_component<seed::InteractableComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::DoorComponent>(source)) {
        scene.add_component<seed::DoorComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::PickupComponent>(source)) {
        scene.add_component<seed::PickupComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::DialogueComponent>(source)) {
        scene.add_component<seed::DialogueComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::QuestComponent>(source)) {
        scene.add_component<seed::QuestComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::HealthComponent>(source)) {
        scene.add_component<seed::HealthComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::AttackComponent>(source)) {
        scene.add_component<seed::AttackComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::EnemyAIComponent>(source)) {
        scene.add_component<seed::EnemyAIComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::GameplayVariablesComponent>(source)) {
        scene.add_component<seed::GameplayVariablesComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::TriggerVolumeComponent>(source)) {
        auto trigger = *v;
        trigger.fired = false;
        scene.add_component<seed::TriggerVolumeComponent>(copy, trigger);
    }
    if (const auto* v = scene.get_component<seed::SpawnPointComponent>(source)) {
        scene.add_component<seed::SpawnPointComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::InventoryComponent>(source)) {
        scene.add_component<seed::InventoryComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::PlayerControllerComponent>(source)) {
        scene.add_component<seed::PlayerControllerComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::ProductDefinitionComponent>(source)) {
        scene.add_component<seed::ProductDefinitionComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::ShopInventoryComponent>(source)) {
        scene.add_component<seed::ShopInventoryComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::WalletComponent>(source)) {
        scene.add_component<seed::WalletComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::ShopHoursComponent>(source)) {
        scene.add_component<seed::ShopHoursComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::SimulationClockComponent>(source)) {
        scene.add_component<seed::SimulationClockComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::CustomerOrderComponent>(source)) {
        scene.add_component<seed::CustomerOrderComponent>(copy, *v);
    }
    return copy;
}

bool prepare_standalone_build(const WorkspaceState& workspace, std::string* error) {
    if (workspace.project_file.empty()) {
        if (error != nullptr) *error = "Save the project before preparing a build.";
        return false;
    }
    try {
        const auto build_folder = workspace.project_file.parent_path() / "Build";
        std::filesystem::create_directories(build_folder);
        const auto project_name = workspace.project_file.filename().string();
        std::ofstream readme(build_folder / "BUILD_README.txt", std::ios::trunc);
        readme << "Seed standalone build\n\n"
               << "Run SeedRuntime with the project file below:\n"
               << "  " << project_name << "\n\n"
               << "Windows: run_game.bat\n"
               << "Linux/macOS: ./run_game.sh\n";
        std::ofstream windows(build_folder / "run_game.bat", std::ios::trunc);
        windows << "@echo off\nSeedRuntime --project=\"" << "%~dp0..\\" << project_name << "\"\npause\n";
        std::ofstream unix_script(build_folder / "run_game.sh", std::ios::trunc);
        unix_script << "#!/usr/bin/env sh\nSeedRuntime --project=\"" << "$(dirname \"$0\")/../" << project_name << "\"\n";
        return static_cast<bool>(readme) && static_cast<bool>(windows) && static_cast<bool>(unix_script);
    } catch (const std::exception& exception) {
        if (error != nullptr) *error = exception.what();
        return false;
    }
}

bool save_workspace(WorkspaceState& workspace, const seed::Scene& scene) {
    if (workspace.project_file.empty() || workspace.scene_file.empty()) {
        workspace.status_message = "Choose File > Save As... first.";
        return false;
    }

    std::string error;
    if (!seed::SceneSerializer::save(scene, workspace.scene_file, &error)) {
        workspace.status_message = "Scene save failed: " + error;
        return false;
    }
    if (!seed::ProjectSerializer::save(workspace.project, workspace.project_file, &error)) {
        workspace.status_message = "Project save failed: " + error;
        return false;
    }

    workspace.dirty = false;
    workspace.status_message = "Saved: " + workspace.project_file.string();
    return true;
}

bool copy_project_assets_for_save_as(
    const std::filesystem::path& old_root,
    const std::filesystem::path& new_root,
    std::string* error
) {
    if (old_root.empty() || old_root == new_root) return true;
    const auto old_assets = old_root / "Assets";
    if (!std::filesystem::exists(old_assets)) return true;

    try {
        const auto new_assets = new_root / "Assets";
        std::filesystem::create_directories(new_assets);
        std::filesystem::copy(
            old_assets,
            new_assets,
            std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing
        );
        return true;
    } catch (const std::exception& exception) {
        if (error) *error = exception.what();
        return false;
    }
}

bool save_workspace_as(
    WorkspaceState& workspace,
    const seed::Scene& scene,
    const std::string& folder_text,
    const std::string& project_name
) {
    try {
        const std::filesystem::path old_root = workspace.project_file.empty()
            ? std::filesystem::path{}
            : workspace.project_file.parent_path();
        const std::filesystem::path root = folder_text.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path{folder_text};

        std::string copy_error;
        if (!copy_project_assets_for_save_as(old_root, root, &copy_error)) {
            workspace.status_message = "Save As could not copy Assets/: " + copy_error;
            return false;
        }

        workspace.project = {};
        workspace.project.name = project_name.empty() ? "Untitled Seed Project" : project_name;
        workspace.project.startup_scene = std::filesystem::path{"Scenes/Main.seedscene"};
        workspace.project_file = root / (project_file_stem(workspace.project.name) + ".seedproject");
        workspace.scene_file = root / workspace.project.startup_scene;
        return save_workspace(workspace, scene);
    } catch (const std::exception& exception) {
        workspace.status_message = std::string{"Save As failed: "} + exception.what();
        return false;
    }
}

bool open_workspace(
    WorkspaceState& workspace,
    seed::Scene& scene,
    const std::string& project_file_text,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle default_shader,
    seed::ShaderHandle skinned_shader,
    seed::AssetRuntime& assets,
    seed::EntityId& editor_camera
) {
    const std::filesystem::path project_file{project_file_text};
    std::string error;
    const auto project = seed::ProjectSerializer::load(project_file, &error);
    if (!project) {
        workspace.status_message = "Open failed: " + error;
        return false;
    }

    const auto scene_file = project_file.parent_path() / project->startup_scene;
    seed::Scene loaded_scene;
    if (!seed::SceneSerializer::load(loaded_scene, scene_file, &error)) {
        workspace.status_message = "Scene open failed: " + error;
        return false;
    }

    assets.set_project_root(project_file.parent_path());
    bind_builtin_resources(loaded_scene, cube_mesh, default_shader);
    std::string asset_error;
    const bool assets_bound = assets.bind_scene(
        loaded_scene,
        default_shader,
        skinned_shader,
        &asset_error
    );

    editor_camera = ensure_editor_camera(loaded_scene);
    scene = std::move(loaded_scene);
    workspace.project = *project;
    workspace.project_file = project_file;
    workspace.scene_file = scene_file;
    workspace.dirty = false;
    workspace.status_message = assets_bound
        ? "Opened: " + project_file.string()
        : "Opened with asset warning: " + asset_error;
    return true;
}

std::filesystem::path locate_example_scene(const std::string& relative_path) {
    const std::filesystem::path requested{relative_path};
    const std::array<std::filesystem::path, 4> candidates{
        requested,
        std::filesystem::path{".."} / requested,
        std::filesystem::path{"../.."} / requested,
        std::filesystem::path{"../../.."} / requested,
    };
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return std::filesystem::absolute(candidate);
        }
    }
    return {};
}

bool open_example_scene(
    WorkspaceState& workspace,
    seed::Scene& scene,
    const std::string& relative_path,
    seed::MeshHandle cube_mesh,
    seed::ShaderHandle default_shader,
    seed::ShaderHandle skinned_shader,
    seed::AssetRuntime& assets,
    seed::EntityId& editor_camera
) {
    const auto scene_file = locate_example_scene(relative_path);
    if (scene_file.empty()) {
        workspace.status_message = "Example scene not found: " + relative_path;
        return false;
    }

    seed::Scene loaded_scene;
    std::string error;
    if (!seed::SceneSerializer::load(loaded_scene, scene_file, &error)) {
        workspace.status_message = "Example scene open failed: " + error;
        return false;
    }

    assets.set_project_root({});
    bind_builtin_resources(loaded_scene, cube_mesh, default_shader);
    std::string asset_error;
    const bool assets_bound = assets.bind_scene(
        loaded_scene,
        default_shader,
        skinned_shader,
        &asset_error
    );

    editor_camera = ensure_editor_camera(loaded_scene);
    scene = std::move(loaded_scene);
    workspace.project = {};
    workspace.project_file.clear();
    workspace.scene_file = scene_file;
    workspace.dirty = false;
    workspace.status_message = assets_bound
        ? "Opened example: " + scene_file.string()
        : "Opened example with asset warning: " + asset_error;
    return true;
}

bool safe_dependency_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& part : path) {
        if (part == "..") return false;
    }
    return true;
}

bool copy_file_if_needed(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    std::string* error
) {
    try {
        if (!std::filesystem::exists(source)) {
            if (error) *error = "Missing source dependency: " + source.string();
            return false;
        }
        std::filesystem::create_directories(destination.parent_path());
        std::error_code equivalent_error;
        if (std::filesystem::exists(destination) &&
            std::filesystem::equivalent(source, destination, equivalent_error) &&
            !equivalent_error) {
            return true;
        }
        std::filesystem::copy_file(
            source,
            destination,
            std::filesystem::copy_options::overwrite_existing
        );
        return true;
    } catch (const std::exception& exception) {
        if (error) *error = exception.what();
        return false;
    }
}

seed::EntityId import_animated_character_into_project(
    WorkspaceState& workspace,
    seed::Scene& scene,
    seed::AssetRuntime& assets,
    seed::ShaderHandle material_shader,
    seed::ShaderHandle skinned_material_shader,
    const std::string& source_text
) {
    if (workspace.project_file.empty()) return seed::InvalidEntity;

    const std::filesystem::path source_file{source_text};
    std::string import_error;
    const auto imported = seed::AssetImporter::import_animated_model(source_file, &import_error);
    if (!imported) return seed::InvalidEntity;

    const std::filesystem::path project_root = workspace.project_file.parent_path();
    const std::string folder_name = project_file_stem(source_file.stem().string());
    const auto destination_directory = project_root / "Assets" / "Imported" / folder_name;
    const auto destination_source = destination_directory / source_file.filename();

    std::string copy_error;
    if (!copy_file_if_needed(source_file, destination_source, &copy_error)) {
        workspace.status_message = "Import copy failed: " + copy_error;
        return seed::InvalidEntity;
    }
    for (const auto& dependency : imported->external_files) {
        if (!safe_dependency_path(dependency)) {
            workspace.status_message =
                "Import rejected unsafe glTF dependency path: " + dependency.string();
            return seed::InvalidEntity;
        }
        if (!copy_file_if_needed(
                source_file.parent_path() / dependency,
                destination_directory / dependency,
                &copy_error)) {
            workspace.status_message = "Import dependency copy failed: " + copy_error;
            return seed::InvalidEntity;
        }
    }

    std::filesystem::path relative_source;
    try {
        relative_source = std::filesystem::relative(destination_source, project_root);
    } catch (const std::exception& exception) {
        workspace.status_message = std::string{"Import path failed: "} + exception.what();
        return seed::InvalidEntity;
    }

    assets.set_project_root(project_root);
    const std::string asset_id = seed::AssetRuntime::make_character_asset_id(relative_source);
    std::string asset_error;
    const auto* resource = assets.load_character(asset_id, &asset_error);
    if (resource == nullptr || !resource->mesh) {
        workspace.status_message =
            "Imported character but could not create runtime asset: " + asset_error;
        return seed::InvalidEntity;
    }

    seed::EntityId empty_player = seed::InvalidEntity;
    scene.for_each<seed::TransformComponent, seed::PlayerControllerComponent, seed::SkinnedMeshComponent>(
        [&](seed::EntityId candidate, seed::TransformComponent&, seed::PlayerControllerComponent& controller,
            seed::SkinnedMeshComponent& character) {
            if (empty_player != seed::InvalidEntity ||
                controller.view_mode != seed::PlayerViewMode::ThirdPerson ||
                !character.asset_id.empty()) {
                return;
            }
            empty_player = candidate;
        }
    );

    auto apply_character = [&](seed::EntityId entity) {
        auto* character = scene.get_component<seed::SkinnedMeshComponent>(entity);
        if (character == nullptr) {
            character = &scene.add_component<seed::SkinnedMeshComponent>(entity);
        }
        character->mesh = resource->mesh;
        character->asset_id = asset_id;
        character->runtime_asset = resource;

        auto* animation = scene.get_component<seed::AnimationPlayerComponent>(entity);
        if (animation == nullptr) {
            animation = &scene.add_component<seed::AnimationPlayerComponent>(entity);
        }
        animation->locomotion_enabled = true;
        animation->blend_duration_seconds = 0.15f;

        auto* material = scene.get_component<seed::MaterialComponent>(entity);
        if (material == nullptr) {
            material = &scene.add_component<seed::MaterialComponent>(entity);
        }
        material->shader = skinned_material_shader;
        material->base_color_texture = resource->base_color_texture;
        material->metallic_roughness_texture = resource->metallic_roughness_texture;
        material->normal_texture = resource->normal_texture;
        material->base_color = resource->base_color;
        material->metallic = resource->metallic;
        material->roughness = resource->roughness;
        material->normal_scale = resource->normal_scale;
        material->use_base_color_texture = static_cast<bool>(resource->base_color_texture);
        material->use_metallic_roughness_texture = static_cast<bool>(resource->metallic_roughness_texture);
        material->use_normal_texture = static_cast<bool>(resource->normal_texture);
        material->asset_id = asset_id;
        material->use_asset_defaults = false;
    };

    if (empty_player != seed::InvalidEntity) {
        apply_character(empty_player);
        workspace.dirty = true;
        workspace.status_message = "Attached animated character to Third-Person Player: " + asset_id;
        (void)material_shader;
        return empty_player;
    }

    const auto entity = scene.create_entity(
        imported->name.empty() ? source_file.stem().string() : imported->name
    );
    scene.add_component<seed::TransformComponent>(entity);

    seed::SkinnedMeshComponent mesh;
    mesh.mesh = resource->mesh;
    mesh.asset_id = asset_id;
    mesh.runtime_asset = resource;
    scene.add_component<seed::SkinnedMeshComponent>(entity, mesh);

    seed::AnimationPlayerComponent animation;
    animation.locomotion_enabled = true;
    animation.blend_duration_seconds = 0.15f;
    scene.add_component<seed::AnimationPlayerComponent>(entity, animation);

    seed::MaterialComponent material;
    material.shader = skinned_material_shader;
    material.base_color_texture = resource->base_color_texture;
    material.metallic_roughness_texture = resource->metallic_roughness_texture;
    material.normal_texture = resource->normal_texture;
    material.base_color = resource->base_color;
    material.metallic = resource->metallic;
    material.roughness = resource->roughness;
    material.normal_scale = resource->normal_scale;
    material.use_base_color_texture = static_cast<bool>(resource->base_color_texture);
    material.use_metallic_roughness_texture = static_cast<bool>(resource->metallic_roughness_texture);
    material.use_normal_texture = static_cast<bool>(resource->normal_texture);
    material.asset_id = asset_id;
    material.use_asset_defaults = false;
    scene.add_component<seed::MaterialComponent>(entity, material);

    workspace.dirty = true;
    workspace.status_message = "Imported animated character: " + asset_id;
    (void)material_shader;
    return entity;
}

seed::EntityId import_model_into_project(
    WorkspaceState& workspace,
    seed::Scene& scene,
    seed::AssetRuntime& assets,
    seed::ShaderHandle material_shader,
    const std::string& source_text
) {
    if (workspace.project_file.empty()) {
        workspace.status_message = "Save the Seed project before importing assets.";
        return seed::InvalidEntity;
    }

    const std::filesystem::path source_file{source_text};
    std::string import_error;
    const auto imported = seed::AssetImporter::import_static_model(source_file, &import_error);
    if (!imported) {
        workspace.status_message = "Import failed: " + import_error;
        return seed::InvalidEntity;
    }

    const std::filesystem::path project_root = workspace.project_file.parent_path();
    const std::string folder_name = project_file_stem(source_file.stem().string());
    const auto destination_directory = project_root / "Assets" / "Imported" / folder_name;
    const auto destination_source = destination_directory / source_file.filename();

    std::string copy_error;
    if (!copy_file_if_needed(source_file, destination_source, &copy_error)) {
        workspace.status_message = "Import copy failed: " + copy_error;
        return seed::InvalidEntity;
    }

    for (const auto& dependency : imported->external_files) {
        if (!safe_dependency_path(dependency)) {
            workspace.status_message =
                "Import rejected unsafe glTF dependency path: " + dependency.string();
            return seed::InvalidEntity;
        }
        if (!copy_file_if_needed(
                source_file.parent_path() / dependency,
                destination_directory / dependency,
                &copy_error)) {
            workspace.status_message = "Import dependency copy failed: " + copy_error;
            return seed::InvalidEntity;
        }
    }

    std::filesystem::path relative_source;
    try {
        relative_source = std::filesystem::relative(destination_source, project_root);
    } catch (const std::exception& exception) {
        workspace.status_message = std::string{"Import path failed: "} + exception.what();
        return seed::InvalidEntity;
    }

    assets.set_project_root(project_root);
    const std::string asset_id = seed::AssetRuntime::make_model_asset_id(relative_source);
    std::string asset_error;
    const auto resource = assets.load_model(asset_id, &asset_error);
    if (!resource) {
        workspace.status_message =
            "Imported source but could not create runtime asset: " + asset_error;
        return seed::InvalidEntity;
    }

    const auto entity = scene.create_entity(
        imported->name.empty() ? source_file.stem().string() : imported->name
    );
    scene.add_component<seed::TransformComponent>(entity);

    seed::MeshComponent mesh;
    mesh.mesh = resource->mesh;
    mesh.visible = true;
    mesh.asset_id = asset_id;
    scene.add_component<seed::MeshComponent>(entity, mesh);

    seed::MaterialComponent material;
    material.shader = material_shader;
    material.base_color_texture = resource->base_color_texture;
    material.metallic_roughness_texture = resource->metallic_roughness_texture;
    material.normal_texture = resource->normal_texture;
    material.base_color = resource->base_color;
    material.metallic = resource->metallic;
    material.roughness = resource->roughness;
    material.normal_scale = resource->normal_scale;
    material.use_base_color_texture = static_cast<bool>(resource->base_color_texture);
    material.use_metallic_roughness_texture =
        static_cast<bool>(resource->metallic_roughness_texture);
    material.use_normal_texture = static_cast<bool>(resource->normal_texture);
    material.asset_id = asset_id;
    material.use_asset_defaults = false;
    scene.add_component<seed::MaterialComponent>(entity, material);

    workspace.dirty = true;
    workspace.status_message = "Imported PBR model: " + asset_id;
    return entity;
}

seed::studio::StudioDocumentInfo document_info(
    const WorkspaceState& workspace,
    const seed::Scene& editor_scene,
    bool playing,
    const seed::GameplayRuntime& gameplay,
    const seed::Scene* play_scene,
    seed::EntityId play_player
) {
    seed::studio::StudioDocumentInfo info;
    info.project_name = workspace.project_file.empty() ? "Untitled" : workspace.project.name;
    info.scene_name = workspace.scene_file.empty()
        ? "Main.seedscene"
        : workspace.scene_file.filename().string();
    info.status_message = workspace.status_message;
    info.dirty = workspace.dirty;
    info.has_project = !workspace.project_file.empty();
    info.playing = playing;
    if (info.has_project) {
        info.revisions = seed::SceneRevisionStore(workspace.project_file.parent_path()).list();
    }

    std::set<std::string> imported_assets;
    editor_scene.for_each<seed::MeshComponent>([&](seed::EntityId, const seed::MeshComponent& mesh) {
        if (seed::AssetRuntime::is_model_asset_id(mesh.asset_id)) {
            imported_assets.insert(mesh.asset_id);
        }
    });
    editor_scene.for_each<seed::SkinnedMeshComponent>(
        [&](seed::EntityId, const seed::SkinnedMeshComponent& mesh) {
            if (seed::AssetRuntime::is_character_asset_id(mesh.asset_id)) {
                imported_assets.insert(mesh.asset_id);
            }
        }
    );
    info.project_assets.assign(imported_assets.begin(), imported_assets.end());

    if (info.has_project) {
        try {
            const auto prefab_directory = workspace.project_file.parent_path() / "Prefabs";
            if(std::filesystem::exists(prefab_directory)) {
                for(const auto& entry : std::filesystem::recursive_directory_iterator(prefab_directory)) {
                    if(entry.is_regular_file() && entry.path().extension() == ".seedprefab") {
                        info.project_prefabs.push_back(
                            std::filesystem::relative(entry.path(), workspace.project_file.parent_path()).generic_string()
                        );
                    }
                }
                std::sort(info.project_prefabs.begin(), info.project_prefabs.end());
            }
        } catch(const std::exception&) {
            info.project_prefabs.clear();
        }
    }

    if (info.has_project) {
        try {
            const auto asset_directory = workspace.project_file.parent_path() / "Assets";
            if(std::filesystem::exists(asset_directory)) {
                for(const auto& entry : std::filesystem::recursive_directory_iterator(asset_directory)) {
                    if(!entry.is_regular_file()) continue;
                    std::string extension = entry.path().extension().string();
                    std::transform(extension.begin(), extension.end(), extension.begin(),
                        [](unsigned char value){ return static_cast<char>(std::tolower(value)); });
                    if(extension == ".glb" || extension == ".gltf" ||
                       extension == ".png" || extension == ".jpg" ||
                       extension == ".jpeg" || extension == ".wav" ||
                       extension == ".ogg" || extension == ".mp3") {
                        info.project_asset_files.push_back(
                            std::filesystem::relative(entry.path(), workspace.project_file.parent_path()).generic_string()
                        );
                    }
                }
                std::sort(info.project_asset_files.begin(), info.project_asset_files.end());
            }
        } catch(const std::exception&) {
            info.project_asset_files.clear();
        }
    }

    if (playing && play_scene != nullptr) {
        const auto hud = seed::GameplayHUDSystem::capture(*play_scene, gameplay);
        info.gameplay_prompt = hud.interaction_prompt;
        info.gameplay_status = hud.status_message;
        info.gameplay_health_visible = hud.health_visible;
        info.gameplay_health_current = hud.health_current;
        info.gameplay_health_maximum = hud.health_maximum;
        info.gameplay_dialogue_visible = hud.dialogue_visible;
        info.gameplay_objectives = hud.active_objectives;
        for (const auto& [item_id, quantity] : hud.inventory) {
            info.gameplay_inventory.emplace_back(item_id, quantity);
        }
        std::sort(info.gameplay_inventory.begin(), info.gameplay_inventory.end());

        play_scene->for_each<seed::ShopInventoryComponent, seed::WalletComponent>(
            [&](seed::EntityId entity, const seed::ShopInventoryComponent& inventory,
                const seed::WalletComponent& wallet) {
                info.gameplay_objectives.push_back(
                    "SHOP  Wallet " + std::to_string(wallet.balance) +
                    "  Revenue " + std::to_string(wallet.lifetime_revenue) +
                    "  Expenses " + std::to_string(wallet.lifetime_expenses)
                );
                if (const auto* metrics =
                        play_scene->get_component<seed::ShopMetricsComponent>(entity)) {
                    info.gameplay_objectives.push_back(
                        "TODAY  Sales " + std::to_string(metrics->daily_revenue) +
                        "  Customers " + std::to_string(metrics->customers_served_today) +
                        "  Products " + std::to_string(metrics->products_sold_today) +
                        "  Satisfaction " + std::to_string(metrics->daily_satisfaction)
                    );
                }
                if (const auto* clock = play_scene->get_component<seed::SimulationClockComponent>(entity)) {
                    const auto* hours = play_scene->get_component<seed::ShopHoursComponent>(entity);
                    const bool open = hours == nullptr ||
                        seed::SimulationSystem::is_shop_open(*hours, *clock);
                    info.gameplay_objectives.push_back(
                        "DAY " + std::to_string(clock->day) +
                        "  " + std::to_string(clock->hour) +
                        "h  " + (open ? "OPEN" : "CLOSED")
                    );
                }
                for (const auto& stock : inventory.products) {
                    info.gameplay_objectives.push_back(
                        "STOCK  " + stock.product_id + "  x" +
                        std::to_string(stock.quantity) + "/" +
                        std::to_string(stock.shelf_capacity)
                    );
                }
            }
        );

        play_scene->for_each<seed::ShopUpgradeComponent>(
            [&](seed::EntityId, const seed::ShopUpgradeComponent& upgrade) {
                const float next_cost = upgrade.level >= upgrade.max_level
                    ? 0.0f
                    : upgrade.base_upgrade_cost * std::pow(
                        std::max(1.0f, upgrade.cost_multiplier),
                        static_cast<float>(std::max(0, upgrade.level - 1))
                    );
                info.gameplay_objectives.push_back(
                    "SHOP LEVEL  " + std::to_string(upgrade.level) + "/" +
                    std::to_string(upgrade.max_level) + "  NEXT UPGRADE " +
                    (upgrade.level >= upgrade.max_level
                        ? "MAX"
                        : std::to_string(next_cost))
                );
            }
        );

        play_scene->for_each<seed::CustomerCapacityComponent>(
            [&](seed::EntityId entity, const seed::CustomerCapacityComponent& capacity) {
                const auto* clock = play_scene->get_component<seed::SimulationClockComponent>(entity);
                const int day = clock == nullptr ? 1 : std::max(1, clock->day);
                const float demand_growth =
                    std::max(0.1f, capacity.arrival_rate_multiplier) *
                    (1.0f + std::max(0, day - 1) *
                        std::max(0.0f, capacity.daily_growth_rate));
                info.gameplay_objectives.push_back(
                    "CUSTOMERS  MAX " + std::to_string(capacity.max_active_customers) +
                    "  QUEUE " + std::to_string(capacity.max_queue_length) +
                    "  DEMAND X" + std::to_string(demand_growth)
                );
            }
        );

        play_scene->for_each<seed::CustomerSpawnerComponent>(
            [&](seed::EntityId entity, const seed::CustomerSpawnerComponent& spawner) {
                int customers = 0;
                play_scene->for_each<seed::CustomerBehaviorComponent>(
                    [&](seed::EntityId, const seed::CustomerBehaviorComponent&) {
                        ++customers;
                    }
                );
                info.gameplay_objectives.push_back(
                    "POPULATION  " + std::to_string(customers) + "/" +
                    std::to_string(spawner.max_customers) + "  SPAWNER " +
                    (spawner.enabled ? "ON" : "OFF")
                );
                (void)entity;
            }
        );

        play_scene->for_each<seed::CustomerOrderComponent>(
            [&](seed::EntityId, const seed::CustomerOrderComponent& order) {
                int fulfilled = 0;
                int requested = 0;
                for (const auto& line : order.lines) {
                    fulfilled += std::min(line.fulfilled_quantity, line.quantity);
                    requested += line.quantity;
                }
                info.gameplay_objectives.push_back(
                    "ORDER  " + order.customer_name + "  " +
                    std::to_string(fulfilled) + "/" +
                    std::to_string(requested) + " units  " +
                    (order.fulfilled ? "FULFILLED" : "IN PROGRESS")
                );
            }
        );
    }
    return info;
}

} // namespace

int main(int argc, char** argv) {
    const std::uint64_t frame_limit = parse_frame_limit(argc, argv);
    seed::Engine engine({
        .application_name = "Seed Studio",
        .window_width = 1440,
        .window_height = 900,
        .window_resizable = true,
        .create_window = true,
        .renderer_backend = seed::RendererBackend::OpenGL,
        .vsync = true,
        .clear_color = {.r = 0.040f, .g = 0.070f, .b = 0.050f, .a = 1.0f},
    });
    if (!engine.start()) return 1;

    auto* renderer = engine.renderer();
    if (!renderer || !engine.platform()) {
        engine.shutdown();
        return 2;
    }

    const auto mesh_shader = renderer->create_shader({
        .vertex_source = seed::studio::SeedMeshVertexShader,
        .fragment_source = seed::studio::SeedMeshFragmentShader,
    });
    const auto skinned_mesh_shader = renderer->create_shader({
        .vertex_source = seed::SeedSkinnedMeshVertexShader,
        .fragment_source = seed::studio::SeedMeshFragmentShader,
    });

    constexpr std::array<seed::VertexPositionColor, 24> cube_vertices{{
        {.position={-0.75f,-0.75f,-0.75f},.color={0.20f,0.95f,0.46f},.normal={0,0,-1}},
        {.position={0.75f,-0.75f,-0.75f},.color={0.18f,0.58f,1},.normal={0,0,-1}},
        {.position={0.75f,0.75f,-0.75f},.color={1,0.78f,0.18f},.normal={0,0,-1}},
        {.position={-0.75f,0.75f,-0.75f},.color={0.72f,0.35f,1},.normal={0,0,-1}},
        {.position={-0.75f,-0.75f,0.75f},.color={0.10f,0.80f,0.72f},.normal={0,0,1}},
        {.position={0.75f,-0.75f,0.75f},.color={1,0.35f,0.28f},.normal={0,0,1}},
        {.position={0.75f,0.75f,0.75f},.color={0.95f,0.90f,0.28f},.normal={0,0,1}},
        {.position={-0.75f,0.75f,0.75f},.color={0.28f,0.72f,1},.normal={0,0,1}},
        {.position={-0.75f,-0.75f,-0.75f},.color={0.20f,0.95f,0.46f},.normal={0,-1,0}},
        {.position={-0.75f,-0.75f,0.75f},.color={0.10f,0.80f,0.72f},.normal={0,-1,0}},
        {.position={0.75f,-0.75f,0.75f},.color={1,0.35f,0.28f},.normal={0,-1,0}},
        {.position={0.75f,-0.75f,-0.75f},.color={0.18f,0.58f,1},.normal={0,-1,0}},
        {.position={-0.75f,0.75f,-0.75f},.color={0.72f,0.35f,1},.normal={0,1,0}},
        {.position={0.75f,0.75f,-0.75f},.color={1,0.78f,0.18f},.normal={0,1,0}},
        {.position={0.75f,0.75f,0.75f},.color={0.95f,0.90f,0.28f},.normal={0,1,0}},
        {.position={-0.75f,0.75f,0.75f},.color={0.28f,0.72f,1},.normal={0,1,0}},
        {.position={0.75f,-0.75f,-0.75f},.color={0.18f,0.58f,1},.normal={1,0,0}},
        {.position={0.75f,-0.75f,0.75f},.color={1,0.35f,0.28f},.normal={1,0,0}},
        {.position={0.75f,0.75f,0.75f},.color={0.95f,0.90f,0.28f},.normal={1,0,0}},
        {.position={0.75f,0.75f,-0.75f},.color={1,0.78f,0.18f},.normal={1,0,0}},
        {.position={-0.75f,-0.75f,-0.75f},.color={0.20f,0.95f,0.46f},.normal={-1,0,0}},
        {.position={-0.75f,0.75f,-0.75f},.color={0.72f,0.35f,1},.normal={-1,0,0}},
        {.position={-0.75f,0.75f,0.75f},.color={0.28f,0.72f,1},.normal={-1,0,0}},
        {.position={-0.75f,-0.75f,0.75f},.color={0.10f,0.80f,0.72f},.normal={-1,0,0}},
    }};
    constexpr std::array<std::uint32_t, 36> cube_indices{{
        0,1,2,2,3,0,4,6,5,6,4,7,8,9,10,10,11,8,
        12,13,14,14,15,12,16,17,18,18,19,16,20,21,22,22,23,20
    }};
    const auto cube_mesh = renderer->create_mesh({
        .vertices = std::span<const seed::VertexPositionColor>{cube_vertices},
        .indices = std::span<const std::uint32_t>{cube_indices},
    });

    if (!mesh_shader || !skinned_mesh_shader || !cube_mesh) {
        if (cube_mesh) renderer->destroy_mesh(cube_mesh);
        if (skinned_mesh_shader) renderer->destroy_shader(skinned_mesh_shader);
        if (mesh_shader) renderer->destroy_shader(mesh_shader);
        engine.shutdown();
        return 3;
    }

    seed::AssetRuntime asset_runtime(*renderer);
    auto& scene = engine.scene();
    seed::EntityId editor_camera = create_editor_camera(scene);
    const auto cube_entity = create_cube(scene, cube_mesh, mesh_shader);
    create_default_lighting(scene);
    WorkspaceState workspace;

    seed::studio::StudioUI studio_ui;
    if (!studio_ui.initialize(engine.platform()->window_handle())) {
        renderer->destroy_mesh(cube_mesh);
        renderer->destroy_shader(skinned_mesh_shader);
        renderer->destroy_shader(mesh_shader);
        engine.shutdown();
        return 4;
    }
    studio_ui.select_entity(cube_entity);

    CameraInputState camera_input{};
    seed::Scene play_scene;
    seed::GameplayRuntime gameplay;
    seed::EntityId play_player = seed::InvalidEntity;
    std::string temporary_play_player_persistent_id;
    bool playing = false;

    seed::SceneDiff pending_play_diff;
    bool has_pending_play_changes = false;
    seed::studio::PlayChangesReview play_changes_review;
    seed::studio::SemanticMergeReview semantic_merge_review;
    bool has_pending_semantic_merge = false;

    auto clear_play_review = [&]() {
        play_scene.clear();
        pending_play_diff = {};
        has_pending_play_changes = false;
        play_changes_review.reset();
        temporary_play_player_persistent_id.clear();
        play_player = seed::InvalidEntity;
        studio_ui.select_entity(first_content_entity(scene));
    };

    auto stop_play_mode = [&]() {
        if (!playing) return;

        gameplay.stop();
        remove_temporary_play_artifacts(
            play_scene,
            scene,
            temporary_play_player_persistent_id
        );
        temporary_play_player_persistent_id.clear();

        pending_play_diff = seed::SceneDiffer::compare(scene, play_scene);
        play_player = seed::InvalidEntity;
        playing = false;
        clear_movement_input(camera_input);
        camera_input.looking = false;
        studio_ui.select_entity(first_content_entity(scene));

        if (pending_play_diff.empty()) {
            play_scene.clear();
            workspace.status_message = "Play Mode stopped. No authored changes detected.";
            return;
        }

        has_pending_play_changes = true;
        play_changes_review.open(pending_play_diff);
        workspace.status_message =
            "Play Mode stopped — review " + std::to_string(pending_play_diff.size()) +
            " semantic change" + (pending_play_diff.size() == 1 ? "." : "s.");
    };

    auto start_play_mode = [&]() {
        if (playing) return;
        if (has_pending_play_changes) {
            workspace.status_message =
                "Review or discard the pending Play changes before starting again.";
            return;
        }

        play_scene = scene;
        play_player = find_authored_player(play_scene);
        const bool authored = play_player != seed::InvalidEntity;
        if (!authored) {
            play_player = create_temporary_play_player(
                play_scene,
                ensure_editor_camera(play_scene)
            );
            temporary_play_player_persistent_id =
                play_scene.entity_persistent_id(play_player);
        } else {
            temporary_play_player_persistent_id.clear();
        }

        if (!gameplay.start(play_scene, play_player)) {
            play_scene.clear();
            play_player = seed::InvalidEntity;
            temporary_play_player_persistent_id.clear();
            workspace.status_message = "Could not start Play Mode.";
            return;
        }

        playing = true;
        clear_movement_input(camera_input);
        camera_input.looking = false;
        studio_ui.select_entity(play_player);
        workspace.status_message = authored
            ? "Play Mode started with the authored Player."
            : "Play Mode started with a temporary Player. Create > Player > First Person to author one.";
    };

    std::cout
        << "[SeedStudio] PBR materials + sky environment + directional shadows + GPU skinning active.\n";

    while (engine.tick()) {
        studio_ui.begin_frame();
        const bool ui_wants_mouse = studio_ui.wants_mouse();
        const bool ui_wants_keyboard = studio_ui.wants_keyboard();
        auto* camera_transform = scene.get_component<seed::TransformComponent>(editor_camera);
        if (!playing && !camera_transform) {
            editor_camera = ensure_editor_camera(scene);
            camera_transform = scene.get_component<seed::TransformComponent>(editor_camera);
        }
        bool stop_requested = false;

        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key &&
                event.key == seed::KeyCode::Escape &&
                event.button_state == seed::ButtonState::Pressed) {
                if (playing) {
                    stop_requested = true;
                } else if (!has_pending_play_changes && !has_pending_semantic_merge) {
                    engine.request_exit();
                }
                continue;
            }

            if (has_pending_play_changes || has_pending_semantic_merge) continue;

            if (playing) {
                const bool keyboard_event = event.type == seed::PlatformEventType::Key;
                const bool mouse_event =
                    event.type == seed::PlatformEventType::MouseButton ||
                    event.type == seed::PlatformEventType::MouseMove ||
                    event.type == seed::PlatformEventType::MouseWheel;
                if ((!keyboard_event || !ui_wants_keyboard) &&
                    (!mouse_event || !ui_wants_mouse)) {
                    gameplay.handle_event(event);
                } else if (event.type == seed::PlatformEventType::WindowFocusChanged) {
                    gameplay.handle_event(event);
                }
                continue;
            }

            if (event.type == seed::PlatformEventType::Key && !ui_wants_keyboard) {
                set_key_state(
                    camera_input,
                    event.key,
                    event.button_state != seed::ButtonState::Released
                );
            }
            if (event.type == seed::PlatformEventType::MouseButton &&
                event.mouse_button == seed::MouseButton::Right && !ui_wants_mouse) {
                camera_input.looking = event.button_state != seed::ButtonState::Released;
                camera_input.has_mouse_position = false;
            }
            if (event.type == seed::PlatformEventType::MouseMove &&
                camera_input.looking && !ui_wants_mouse && camera_transform) {
                if (camera_input.has_mouse_position) {
                    camera_transform->rotation_degrees.y +=
                        (event.x - camera_input.last_mouse_x) * 0.12f;
                    camera_transform->rotation_degrees.x -=
                        (event.y - camera_input.last_mouse_y) * 0.12f;
                    camera_transform->rotation_degrees.x = std::clamp(
                        camera_transform->rotation_degrees.x,
                        -89.0f,
                        89.0f
                    );
                }
                camera_input.last_mouse_x = event.x;
                camera_input.last_mouse_y = event.y;
                camera_input.has_mouse_position = true;
            }
            if (event.type == seed::PlatformEventType::MouseWheel && !ui_wants_mouse) {
                camera_input.move_speed = std::clamp(
                    camera_input.move_speed + event.y * 0.45f,
                    0.5f,
                    20.0f
                );
            }
            if (event.type == seed::PlatformEventType::WindowFocusChanged && !event.focused) {
                clear_movement_input(camera_input);
                camera_input.looking = false;
            }
        }

        if (stop_requested) stop_play_mode();

        if (playing) {
            play_scene.for_each<seed::SimulationClockComponent>(
                [&](seed::EntityId entity, seed::SimulationClockComponent& clock) {
                    seed::SimulationSystem::advance_clock(
                        clock,
                        static_cast<float>(engine.delta_seconds())
                    );
                    if (auto* metrics =
                            play_scene.get_component<seed::ShopMetricsComponent>(entity);
                        metrics != nullptr && metrics->tracked_day != clock.day) {
                        metrics->tracked_day = clock.day;
                        metrics->customers_served_today = 0;
                        metrics->orders_completed_today = 0;
                        metrics->products_sold_today = 0;
                        metrics->daily_revenue = 0.0f;
                        metrics->daily_satisfaction = 100.0f;
                        metrics->satisfaction_samples = 0;
                    }
                }
            );
            auto& gameplay_events = engine.gameplay_events();
            gameplay.update(play_scene, engine.delta_seconds(), &gameplay_events);
            seed::AnimationSystem::update_scene(
                play_scene,
                static_cast<float>(engine.delta_seconds()),
                &gameplay_events
            );
            seed::GameplayRulesSystem::consume_events(play_scene, gameplay_events);
            seed::CombatSystem::consume_events(play_scene, gameplay_events);
            gameplay_events.clear();
        } else if (!has_pending_play_changes && !has_pending_semantic_merge) {
            if (ui_wants_keyboard) clear_movement_input(camera_input);
            if (camera_transform) {
                update_camera_movement(
                    *camera_transform,
                    camera_input,
                    engine.delta_seconds()
                );
            }
        }

        seed::Scene& ui_scene = playing ? play_scene : scene;
        studio_ui.draw(
            ui_scene,
            document_info(
                workspace,
                scene,
                playing,
                gameplay,
                playing ? &play_scene : nullptr,
                play_player
            )
        );

        if (!playing && studio_ui.consume_scene_edited()) {
            workspace.dirty = true;
            workspace.status_message = "Scene modified.";
        } else if (playing) {
            (void)studio_ui.consume_scene_edited();
        }

        if (has_pending_play_changes) {
            const seed::studio::PlayChangesReviewResult review_result =
                play_changes_review.draw(pending_play_diff);

            if (review_result.decision == seed::studio::PlayChangesDecision::ApplySelected) {
                const seed::SceneApplyResult apply_result = seed::SceneDiffer::apply(
                    scene,
                    play_scene,
                    pending_play_diff,
                    review_result.selected_change_indices
                );

                if (apply_result.applied > 0) {
                    workspace.dirty = true;
                    bind_builtin_resources(scene, cube_mesh, mesh_shader);
                    std::string rebind_error;
                    const bool rebound = asset_runtime.bind_scene(
                        scene,
                        mesh_shader,
                        skinned_mesh_shader,
                        &rebind_error
                    );
                    editor_camera = ensure_editor_camera(scene);
                    if (!rebound && apply_result.failed == 0) {
                        workspace.status_message =
                            "Applied " + std::to_string(apply_result.applied) +
                            " Play change(s), but asset rebind reported: " + rebind_error;
                    } else if (apply_result.failed > 0) {
                        workspace.status_message =
                            "Applied " + std::to_string(apply_result.applied) +
                            " Play change(s); " + std::to_string(apply_result.failed) +
                            " could not be applied.";
                    } else {
                        workspace.status_message =
                            "Applied " + std::to_string(apply_result.applied) +
                            " selected Play change(s).";
                    }
                } else if (apply_result.failed > 0) {
                    workspace.status_message =
                        "Selected Play changes could not be applied.";
                } else {
                    workspace.status_message = "No Play changes were applied.";
                }
                clear_play_review();
            } else if (review_result.decision == seed::studio::PlayChangesDecision::Discard) {
                clear_play_review();
                workspace.status_message = "Play changes discarded.";
            }
        }

        if (has_pending_semantic_merge) {
            const seed::studio::SemanticMergeReviewResult merge_result =
                semantic_merge_review.draw();

            if (merge_result.decision == seed::studio::SemanticMergeReviewDecision::Apply) {
                studio_ui.begin_external_history_transaction();
                scene = merge_result.merged_scene;
                const seed::SceneRevisionStore revisions(workspace.project_file.parent_path());
                const auto saved_merge = revisions.create_revision(scene, "Merged revision");
                bind_builtin_resources(scene, cube_mesh, mesh_shader);
                std::string rebind_error;
                const bool rebound = asset_runtime.bind_scene(
                    scene,
                    mesh_shader,
                    skinned_mesh_shader,
                    &rebind_error
                );
                editor_camera = ensure_editor_camera(scene);
                workspace.dirty = true;
                has_pending_semantic_merge = false;
                if (!saved_merge.saved) {
                    workspace.status_message =
                        "Semantic merge applied, but merged revision could not be saved: " +
                        saved_merge.error;
                } else if (!rebound) {
                    workspace.status_message =
                        "Merged revision " + saved_merge.revision.id +
                        " applied, but asset rebind reported: " + rebind_error;
                } else {
                    workspace.status_message =
                        "Merged revision " + saved_merge.revision.id + " applied.";
                }
                studio_ui.select_entity(first_content_entity(scene));
            } else if (merge_result.decision == seed::studio::SemanticMergeReviewDecision::Cancel) {
                has_pending_semantic_merge = false;
                workspace.status_message = "Semantic merge cancelled.";
            }
        }

        const auto action = studio_ui.take_action();
        if (!(playing && action.type != seed::studio::StudioActionType::Stop &&
              action.type != seed::studio::StudioActionType::Play &&
              action.type != seed::studio::StudioActionType::None)) {
            switch (action.type) {
            case seed::studio::StudioActionType::Play:
                if (!playing) start_play_mode();
                break;
            case seed::studio::StudioActionType::Stop:
                stop_play_mode();
                break;
            case seed::studio::StudioActionType::NewProject: {
                if (has_pending_play_changes) break;
                asset_runtime.clear();
                build_project_template(scene, action.path2, cube_mesh, mesh_shader);
                editor_camera = ensure_editor_camera(scene);
                studio_ui.select_entity(first_content_entity(scene));
                workspace = {};
                if (save_workspace_as(workspace, scene, action.path, action.project_name)) {
                    asset_runtime.set_project_root(workspace.project_file.parent_path());
                    ensure_beginner_prefab_library(workspace.project_file.parent_path(), cube_mesh, mesh_shader);
                    workspace.status_message = "Created project with beginner prefab library.";
                }
                break;
            }
            case seed::studio::StudioActionType::OpenSemanticMerge: {
                if (playing || has_pending_play_changes || has_pending_semantic_merge) break;
                if (workspace.project_file.empty()) {
                    workspace.status_message = "Save the Seed project before merging revisions.";
                    break;
                }
                seed::Scene base_scene;
                seed::Scene ours_scene;
                seed::Scene theirs_scene;
                std::string error;
                const seed::SceneRevisionStore revisions(workspace.project_file.parent_path());
                if (!revisions.load_revision(base_scene, action.path, &error) ||
                    !revisions.load_revision(ours_scene, action.path2, &error) ||
                    !revisions.load_revision(theirs_scene, action.path3, &error)) {
                    workspace.status_message = "Semantic merge could not load revisions: " + error;
                    break;
                }
                semantic_merge_review.open(base_scene, ours_scene, theirs_scene);
                has_pending_semantic_merge = true;
                workspace.status_message = "Semantic merge review opened.";
                break;
            }
            case seed::studio::StudioActionType::OpenExample:
                if (has_pending_play_changes || has_pending_semantic_merge) break;
                if (open_example_scene(
                        workspace,
                        scene,
                        action.path,
                        cube_mesh,
                        mesh_shader,
                        skinned_mesh_shader,
                        asset_runtime,
                        editor_camera)) {
                    studio_ui.select_entity(first_content_entity(scene));
                    clear_movement_input(camera_input);
                    camera_input.looking = false;
                }
                break;
            case seed::studio::StudioActionType::OpenProject:
                if (has_pending_play_changes || has_pending_semantic_merge) break;
                if (open_workspace(
                        workspace,
                        scene,
                        action.path,
                        cube_mesh,
                        mesh_shader,
                        skinned_mesh_shader,
                        asset_runtime,
                        editor_camera)) {
                    ensure_beginner_prefab_library(workspace.project_file.parent_path(), cube_mesh, mesh_shader);
                    studio_ui.select_entity(first_content_entity(scene));
                    clear_movement_input(camera_input);
                    camera_input.looking = false;
                }
                break;
            case seed::studio::StudioActionType::SaveRevision: {
                if (has_pending_play_changes || has_pending_semantic_merge ||
                    workspace.project_file.empty()) {
                    workspace.status_message = "Save the Seed project before creating a revision.";
                    break;
                }
                const seed::SceneRevisionStore revisions(workspace.project_file.parent_path());
                const auto saved = revisions.create_revision(scene, action.path);
                workspace.status_message = saved.saved
                    ? "Saved revision " + saved.revision.id + ": " + saved.revision.label
                    : "Revision save failed: " + saved.error;
                break;
            }
            case seed::studio::StudioActionType::LoadRevision: {
                if (playing || has_pending_play_changes || has_pending_semantic_merge ||
                    workspace.project_file.empty()) {
                    break;
                }
                seed::SceneRevisionStore revisions(workspace.project_file.parent_path());
                seed::Scene restored;
                std::string error;
                if (!revisions.load_revision(restored, action.path, &error)) {
                    workspace.status_message = "Revision restore failed: " + error;
                    break;
                }
                studio_ui.begin_external_history_transaction();
                scene = std::move(restored);
                bind_builtin_resources(scene, cube_mesh, mesh_shader);
                std::string rebind_error;
                const bool rebound = asset_runtime.bind_scene(
                    scene,
                    mesh_shader,
                    skinned_mesh_shader,
                    &rebind_error
                );
                editor_camera = ensure_editor_camera(scene);
                workspace.dirty = true;
                studio_ui.select_entity(first_content_entity(scene));
                workspace.status_message = rebound
                    ? "Restored revision " + action.path + "."
                    : "Restored revision " + action.path +
                        ", but asset rebind reported: " + rebind_error;
                break;
            }
            case seed::studio::StudioActionType::BuildGame: {
                if (has_pending_play_changes || workspace.project_file.empty()) break;
                const auto findings = seed::SeedDoctor::inspect(scene);
                std::size_t errors = 0;
                for (const auto& finding : findings) {
                    if (finding.severity == seed::DoctorSeverity::Error) ++errors;
                }
                if (errors > 0) {
                    workspace.status_message = "Build blocked: Seed Doctor found " + std::to_string(errors) + " error(s). Fix them before exporting.";
                    break;
                }
                if (!save_workspace(workspace, scene)) break;
                std::string build_error;
                workspace.status_message = prepare_standalone_build(workspace, &build_error)
                    ? "Standalone build prepared in " + (workspace.project_file.parent_path() / "Build").string()
                    : "Build preparation failed: " + build_error;
                break;
            }
            case seed::studio::StudioActionType::RefreshLinkedPrefabs: {
                if (has_pending_play_changes || workspace.project_file.empty()) break;
                std::filesystem::path prefab_path = action.path;
                if (prefab_path.is_relative()) prefab_path = workspace.project_file.parent_path() / prefab_path;
                std::size_t refreshed = 0;
                std::string prefab_error;
                if (!seed::PrefabSerializer::refresh_linked_instances(
                        scene,
                        prefab_path,
                        &refreshed,
                        &prefab_error)) {
                    workspace.status_message = "Prefab refresh failed: " + prefab_error;
                    break;
                }
                if (refreshed > 0) {
                    workspace.dirty = true;
                    std::string rebind_error;
                    const bool rebound = asset_runtime.bind_scene(
                        scene,
                        mesh_shader,
                        skinned_mesh_shader,
                        &rebind_error
                    );
                    workspace.status_message = rebound
                        ? "Refreshed " + std::to_string(refreshed) + " linked prefab instance(s)."
                        : "Refreshed " + std::to_string(refreshed) +
                            " instance(s), but asset rebind reported: " + rebind_error;
                } else {
                    workspace.status_message = "No linked instances found for " + prefab_path.string() + ".";
                }
                break;
            }
            case seed::studio::StudioActionType::SavePrefab: {
                if (has_pending_play_changes || workspace.project_file.empty()) break;
                if (studio_ui.selected_entity() == seed::InvalidEntity ||
                    !scene.is_alive(studio_ui.selected_entity())) {
                    workspace.status_message = "Select an entity before saving a prefab.";
                    break;
                }
                std::filesystem::path prefab_path = action.path;
                if (prefab_path.is_relative()) prefab_path = workspace.project_file.parent_path() / prefab_path;
                if (prefab_path.extension() != ".seedprefab") prefab_path += ".seedprefab";
                std::string prefab_error;
                workspace.status_message = seed::PrefabSerializer::save(
                    scene,
                    studio_ui.selected_entity(),
                    prefab_path,
                    &prefab_error
                )
                    ? "Saved prefab: " + prefab_path.string()
                    : "Prefab save failed: " + prefab_error;
                break;
            }
            case seed::studio::StudioActionType::InstantiatePrefab: {
                if (has_pending_play_changes || workspace.project_file.empty()) break;
                std::filesystem::path prefab_path = action.path;
                if (prefab_path.is_relative()) prefab_path = workspace.project_file.parent_path() / prefab_path;
                seed::EntityId created = seed::InvalidEntity;
                std::string prefab_error;
                if (!seed::PrefabSerializer::instantiate(scene, prefab_path, &created, &prefab_error)) {
                    workspace.status_message = "Prefab instantiate failed: " + prefab_error;
                    break;
                }
                studio_ui.select_entity(created);
                workspace.dirty = true;
                std::string rebind_error;
                const bool rebound = asset_runtime.bind_scene(
                    scene,
                    mesh_shader,
                    skinned_mesh_shader,
                    &rebind_error
                );
                workspace.status_message = rebound
                    ? "Instantiated prefab: " + prefab_path.string()
                    : "Instantiated prefab, but asset rebind reported: " + rebind_error;
                break;
            }
            case seed::studio::StudioActionType::Save:
                if (!has_pending_play_changes) save_workspace(workspace, scene);
                break;
            case seed::studio::StudioActionType::SaveAs:
                if (has_pending_play_changes) break;
                if (save_workspace_as(workspace, scene, action.path, action.project_name)) {
                    asset_runtime.set_project_root(workspace.project_file.parent_path());
                    ensure_beginner_prefab_library(workspace.project_file.parent_path(), cube_mesh, mesh_shader);
                    bind_builtin_resources(scene, cube_mesh, mesh_shader);
                    std::string error;
                    if (!asset_runtime.bind_scene(
                            scene,
                            mesh_shader,
                            skinned_mesh_shader,
                            &error)) {
                        workspace.status_message =
                            "Saved, but asset rebind failed: " + error;
                    }
                }
                break;
            case seed::studio::StudioActionType::NewScene:
                if (has_pending_play_changes) break;
                scene.clear();
                editor_camera = create_editor_camera(scene);
                create_default_lighting(scene);
                studio_ui.select_entity(seed::InvalidEntity);
                workspace.dirty = true;
                workspace.status_message =
                    "New empty scene with default Seed Sun, Ambient Light, and Sky.";
                clear_movement_input(camera_input);
                camera_input.looking = false;
                break;
            case seed::studio::StudioActionType::ImportModel: {
                if (has_pending_play_changes) break;
                auto entity = import_animated_character_into_project(
                    workspace,
                    scene,
                    asset_runtime,
                    mesh_shader,
                    skinned_mesh_shader,
                    action.path
                );
                if (entity == seed::InvalidEntity) {
                    entity = import_model_into_project(
                        workspace,
                        scene,
                        asset_runtime,
                        mesh_shader,
                        action.path
                    );
                }
                if (entity != seed::InvalidEntity) studio_ui.select_entity(entity);
                break;
            }
            case seed::studio::StudioActionType::CreateEmptyEntity: {
                if (has_pending_play_changes) break;
                const auto entity = scene.create_entity("New Entity");
                scene.add_component<seed::TransformComponent>(entity);
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message = "Created New Entity.";
                break;
            }
            case seed::studio::StudioActionType::CreateCube: {
                if (has_pending_play_changes) break;
                const auto entity = create_cube(scene, cube_mesh, mesh_shader, "Cube");
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message = "Created Cube.";
                break;
            }
            case seed::studio::StudioActionType::CreateDirectionalLight: {
                if (has_pending_play_changes) break;
                const auto entity = scene.create_entity("Directional Light");
                auto& transform = scene.add_component<seed::TransformComponent>(entity);
                transform.rotation_degrees = {-45.0f, 30.0f, 0.0f};
                scene.add_component<seed::DirectionalLightComponent>(entity);
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message = "Created Directional Light with shadows.";
                break;
            }
            case seed::studio::StudioActionType::CreateAmbientLight: {
                if (has_pending_play_changes) break;
                const auto entity = scene.create_entity("Ambient Light");
                scene.add_component<seed::AmbientLightComponent>(entity);
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message = "Created Ambient Light.";
                break;
            }
            case seed::studio::StudioActionType::CreateSky: {
                if (has_pending_play_changes) break;
                const auto entity = scene.create_entity("Sky");
                scene.add_component<seed::SkyComponent>(entity);
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message = "Created Sky environment.";
                break;
            }
            case seed::studio::StudioActionType::CreateBeginnerPreset: {
                if (has_pending_play_changes) break;
                const auto entity = create_beginner_preset(
                    scene,
                    action.path,
                    editor_camera,
                    cube_mesh,
                    mesh_shader
                );
                if (entity != seed::InvalidEntity) {
                    studio_ui.select_entity(entity);
                    workspace.dirty = true;
                    workspace.status_message = "Created beginner preset: " + action.path + ".";
                }
                break;
            }
            case seed::studio::StudioActionType::CreateFirstPersonPlayer: {
                if (has_pending_play_changes) break;
                const auto entity = create_first_person_player(
                    scene,
                    editor_camera,
                    "Player",
                    true
                );
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message =
                    "Created First-Person Player with Camera + Controller + Inventory + Character Body.";
                break;
            }
            case seed::studio::StudioActionType::CreateThirdPersonPlayer: {
                if (has_pending_play_changes) break;
                const auto entity = create_third_person_player(scene, editor_camera);
                studio_ui.select_entity(entity);
                workspace.dirty = true;
                workspace.status_message =
                    "Created Third-Person Player with child camera, Character Body, Animation Player, and locomotion.";
                break;
            }
            case seed::studio::StudioActionType::DuplicateSelected: {
                if (has_pending_play_changes) break;
                const auto copy = duplicate_entity(scene, studio_ui.selected_entity());
                if (copy != seed::InvalidEntity) {
                    studio_ui.select_entity(copy);
                    workspace.dirty = true;
                    workspace.status_message =
                        "Duplicated entity with a new persistent ID.";
                } else {
                    workspace.status_message =
                        "Editor-only entities cannot be duplicated.";
                }
                break;
            }
            case seed::studio::StudioActionType::DeleteSelected: {
                if (has_pending_play_changes) break;
                const auto selected = studio_ui.selected_entity();
                if (is_editor_only_entity(scene, selected)) {
                    workspace.status_message =
                        "The Seed Studio editor camera cannot be deleted.";
                } else if (selected != seed::InvalidEntity && scene.destroy_entity(selected)) {
                    studio_ui.select_entity(seed::InvalidEntity);
                    workspace.dirty = true;
                    workspace.status_message = "Deleted entity.";
                }
                break;
            }
            case seed::studio::StudioActionType::None:
            default:
                break;
            }
        }

        engine.begin_frame();
        if (playing && play_player != seed::InvalidEntity) {
            seed::RenderSystem::render(play_scene, *renderer, play_player);
        } else {
            seed::RenderSystem::render(scene, *renderer, editor_camera);
        }
        studio_ui.render();
        engine.end_frame();

        if (frame_limit > 0 && engine.frame_index() >= frame_limit) {
            engine.request_exit();
        }
    }

    gameplay.stop();
    studio_ui.shutdown();
    asset_runtime.clear();
    renderer->destroy_mesh(cube_mesh);
    renderer->destroy_shader(skinned_mesh_shader);
    renderer->destroy_shader(mesh_shader);
    engine.shutdown();
    return 0;
}
