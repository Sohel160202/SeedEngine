#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Hierarchy.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    seed::Scene scene;

    const auto player = scene.create_entity("Third Person Player");
    auto& player_transform = scene.add_component<seed::TransformComponent>(player);
    player_transform.position = {0.0f, 1.0f, 0.0f};

    seed::PlayerControllerComponent controller;
    controller.view_mode = seed::PlayerViewMode::ThirdPerson;
    controller.move_speed = 5.0f;
    controller.camera_distance = 5.0f;
    controller.camera_height = 1.7f;
    controller.camera_shoulder_offset = 0.35f;
    controller.camera_min_pitch = -50.0f;
    controller.camera_max_pitch = 65.0f;
    controller.orient_to_movement = true;
    scene.add_component<seed::PlayerControllerComponent>(player, controller);
    scene.add_component<seed::InventoryComponent>(player);

    const auto camera = scene.create_entity("Third Person Camera");
    seed::TransformComponent camera_transform;
    camera_transform.position = {0.0f, 1.7f, 5.0f};
    camera_transform.rotation_degrees = {-12.0f, 0.0f, 0.0f};
    scene.add_component<seed::TransformComponent>(camera, camera_transform);

    seed::CameraComponent camera_component;
    camera_component.primary = true;
    camera_component.enabled = true;
    camera_component.editor_only = false;
    scene.add_component<seed::CameraComponent>(camera, camera_component);
    assert(seed::set_parent(scene, camera, player, false));

    seed::GameplayRuntime runtime;
    assert(runtime.start(scene, player));
    assert(runtime.active());
    assert(runtime.player_entity() == player);
    assert(runtime.camera_entity() == camera);

    runtime.update(scene, 0.0);
    const auto* updated_camera = scene.get_component<seed::TransformComponent>(camera);
    assert(updated_camera != nullptr);
    assert(updated_camera->position.y > 1.0f);
    assert(updated_camera->position.z > 4.0f);

    seed::PlatformEvent press_w;
    press_w.type = seed::PlatformEventType::Key;
    press_w.key = seed::KeyCode::W;
    press_w.button_state = seed::ButtonState::Pressed;
    runtime.handle_event(press_w);
    runtime.update(scene, 0.2);

    const auto* moved_player = scene.get_component<seed::TransformComponent>(player);
    assert(moved_player != nullptr);
    assert(moved_player->position.z < -0.5f);
    assert(near(moved_player->rotation_degrees.y, 0.0f, 0.5f));

    seed::PlatformEvent release_w = press_w;
    release_w.button_state = seed::ButtonState::Released;
    runtime.handle_event(release_w);

    seed::PlatformEvent look_button;
    look_button.type = seed::PlatformEventType::MouseButton;
    look_button.mouse_button = seed::MouseButton::Right;
    look_button.button_state = seed::ButtonState::Pressed;
    runtime.handle_event(look_button);

    seed::PlatformEvent mouse_a;
    mouse_a.type = seed::PlatformEventType::MouseMove;
    mouse_a.x = 100.0f;
    mouse_a.y = 100.0f;
    runtime.handle_event(mouse_a);

    seed::PlatformEvent mouse_b = mouse_a;
    mouse_b.x = 200.0f;
    runtime.handle_event(mouse_b);
    runtime.update(scene, 0.0);

    const seed::TransformComponent camera_world = seed::world_transform(scene, camera);
    assert(camera_world.rotation_degrees.y > 5.0f);

    runtime.stop();

    const std::filesystem::path file =
        std::filesystem::temp_directory_path() / "seed_third_person_test.seedscene";
    std::string error;
    assert(seed::SceneSerializer::save(scene, file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, file, &error));
    std::filesystem::remove(file);

    const auto loaded_player = loaded.find_entity_by_persistent_id(scene.entity_persistent_id(player));
    const auto loaded_camera = loaded.find_entity_by_persistent_id(scene.entity_persistent_id(camera));
    assert(loaded_player != seed::InvalidEntity);
    assert(loaded_camera != seed::InvalidEntity);

    const auto* loaded_controller = loaded.get_component<seed::PlayerControllerComponent>(loaded_player);
    assert(loaded_controller != nullptr);
    assert(loaded_controller->view_mode == seed::PlayerViewMode::ThirdPerson);
    assert(near(loaded_controller->camera_distance, 5.0f));
    assert(near(loaded_controller->camera_height, 1.7f));
    assert(near(loaded_controller->camera_shoulder_offset, 0.35f));
    assert(loaded_controller->orient_to_movement);
    assert(seed::parent_entity(loaded, loaded_camera) == loaded_player);

    std::cout << "Seed third-person runtime + persistence tests passed.\n";
    return 0;
}
