#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/math/Math.h"
#include "seed/project/ProjectSerializer.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    {
        seed::Scene scene;

        const auto door = scene.create_entity("Door");
        assert(door != seed::InvalidEntity);
        assert(scene.is_alive(door));
        assert(scene.entity_name(door) == "Door");
        assert(!scene.entity_persistent_id(door).empty());

        auto& transform = scene.add_component<seed::TransformComponent>(door);
        transform.position = {1.0f, 2.0f, 3.0f};

        scene.add_component<seed::InteractableComponent>(door, "Open Door", true);
        scene.add_component<seed::DoorComponent>(
            door,
            seed::DoorMotion::Rotate,
            90.0f,
            0.5f,
            "GoldenKey",
            false,
            false
        );

        assert(scene.has_component<seed::TransformComponent>(door));
        assert(scene.has_component<seed::DoorComponent>(door));
        assert(scene.get_component<seed::TransformComponent>(door)->position.x == 1.0f);
        assert(scene.get_component<seed::DoorComponent>(door)->required_item == "GoldenKey");

        int transform_count = 0;
        scene.for_each<seed::TransformComponent>(
            [&](seed::EntityId entity, seed::TransformComponent& component) {
                assert(entity == door);
                assert(component.position.y == 2.0f);
                ++transform_count;
            }
        );
        assert(transform_count == 1);

        assert(scene.remove_component<seed::InteractableComponent>(door));
        assert(!scene.has_component<seed::InteractableComponent>(door));

        assert(scene.destroy_entity(door));
        assert(!scene.is_alive(door));
        assert(scene.entity_count() == 0);
    }

    {
        const seed::Vec3 default_forward = seed::forward_from_euler({0.0f, 0.0f, 0.0f});
        assert(nearly_equal(default_forward.x, 0.0f));
        assert(nearly_equal(default_forward.y, 0.0f));
        assert(nearly_equal(default_forward.z, -1.0f));

        const seed::Vec3 right = seed::right_from_euler({0.0f, 0.0f, 0.0f});
        assert(nearly_equal(right.x, 1.0f));
        assert(nearly_equal(right.y, 0.0f));
        assert(nearly_equal(right.z, 0.0f));

        const seed::Mat4 translation = seed::translation_matrix({2.0f, 3.0f, 4.0f});
        assert(nearly_equal(translation.values[12], 2.0f));
        assert(nearly_equal(translation.values[13], 3.0f));
        assert(nearly_equal(translation.values[14], 4.0f));
    }

    {
        seed::Scene scene;
        const auto camera = scene.create_entity("Camera");
        scene.add_component<seed::TransformComponent>(camera);
        scene.add_component<seed::CameraComponent>(camera);

        int camera_count = 0;
        scene.for_each<seed::TransformComponent, seed::CameraComponent>(
            [&](seed::EntityId entity, seed::TransformComponent&, seed::CameraComponent& component) {
                assert(entity == camera);
                assert(component.primary);
                ++camera_count;
            }
        );
        assert(camera_count == 1);
    }

    {
        const auto test_root = std::filesystem::temp_directory_path() / "seed_engine_persistence_test";
        const auto project_file = test_root / "PersistenceTest.seedproject";
        const auto scene_file = test_root / "Scenes" / "Main.seedscene";
        std::filesystem::remove_all(test_root);

        seed::ProjectDescriptor project;
        project.name = "Persistence Test";
        project.startup_scene = std::filesystem::path{"Scenes/Main.seedscene"};

        std::string error;
        assert(seed::ProjectSerializer::save(project, project_file, &error));
        const auto loaded_project = seed::ProjectSerializer::load(project_file, &error);
        assert(loaded_project.has_value());
        assert(loaded_project->name == "Persistence Test");
        assert(loaded_project->startup_scene.generic_string() == "Scenes/Main.seedscene");

        seed::Scene scene;

        const auto editor_camera = scene.create_entity("Editor Camera");
        scene.add_component<seed::TransformComponent>(editor_camera);
        seed::CameraComponent editor_camera_component;
        editor_camera_component.editor_only = true;
        scene.add_component<seed::CameraComponent>(editor_camera, editor_camera_component);

        const auto cube = scene.create_entity("Saved Cube");
        const std::string original_persistent_id = scene.entity_persistent_id(cube);

        auto& transform = scene.add_component<seed::TransformComponent>(cube);
        transform.position = {2.0f, -2.4f, 1.55f};
        transform.rotation_degrees = {48.0f, 88.5f, -27.5f};
        transform.scale = {3.9f, 1.95f, 1.41f};

        seed::MeshComponent mesh;
        mesh.asset_id = "builtin:cube";
        scene.add_component<seed::MeshComponent>(cube, mesh);

        seed::MaterialComponent material;
        material.asset_id = "builtin:seed_default";
        scene.add_component<seed::MaterialComponent>(cube, material);

        scene.add_component<seed::HealthComponent>(cube, 150.0f, 125.0f, false);
        assert(scene.entity_count() == 2);

        assert(seed::SceneSerializer::save(scene, scene_file, &error));

        seed::Scene loaded_scene;
        assert(seed::SceneSerializer::load(loaded_scene, scene_file, &error));
        assert(loaded_scene.entity_count() == 1);

        seed::EntityId loaded_cube = seed::InvalidEntity;
        loaded_scene.for_each_entity([&](seed::EntityId entity, const std::string& name) {
            assert(name != "Editor Camera");
            if (name == "Saved Cube") {
                loaded_cube = entity;
            }
        });

        assert(loaded_cube != seed::InvalidEntity);
        assert(loaded_scene.entity_persistent_id(loaded_cube) == original_persistent_id);

        const auto* loaded_transform = loaded_scene.get_component<seed::TransformComponent>(loaded_cube);
        assert(loaded_transform != nullptr);
        assert(nearly_equal(loaded_transform->position.x, 2.0f));
        assert(nearly_equal(loaded_transform->position.y, -2.4f));
        assert(nearly_equal(loaded_transform->scale.z, 1.41f));

        const auto* loaded_mesh = loaded_scene.get_component<seed::MeshComponent>(loaded_cube);
        const auto* loaded_material = loaded_scene.get_component<seed::MaterialComponent>(loaded_cube);
        assert(loaded_mesh != nullptr && loaded_mesh->asset_id == "builtin:cube");
        assert(loaded_material != nullptr && loaded_material->asset_id == "builtin:seed_default");
        assert(!loaded_mesh->mesh);
        assert(!loaded_material->shader);

        const auto* loaded_health = loaded_scene.get_component<seed::HealthComponent>(loaded_cube);
        assert(loaded_health != nullptr);
        assert(nearly_equal(loaded_health->maximum, 150.0f));
        assert(nearly_equal(loaded_health->current, 125.0f));

        std::filesystem::remove_all(test_root);
    }

    {
        seed::Engine engine({
            .application_name = "Seed Headless Test",
            .create_window = false,
        });

        assert(engine.start());
        assert(engine.is_running());
        assert(engine.tick());
        assert(engine.frame_index() == 1);
        assert(engine.delta_seconds() >= 0.0);

        engine.request_exit();
        assert(!engine.tick());
        engine.shutdown();
        assert(!engine.is_running());
    }

    std::cout << "SeedCoreTests passed.\n";
    return 0;
}
