#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    const auto test_root = std::filesystem::temp_directory_path() / "seed_lighting_persistence_test";
    const auto scene_file = test_root / "Lighting.seedscene";
    std::filesystem::remove_all(test_root);

    seed::Scene scene;

    const auto sun = scene.create_entity("Sun");
    auto& sun_transform = scene.add_component<seed::TransformComponent>(sun);
    sun_transform.rotation_degrees = {-42.0f, 28.0f, 0.0f};
    seed::DirectionalLightComponent directional;
    directional.color = {1.0f, 0.82f, 0.64f};
    directional.intensity = 1.75f;
    directional.enabled = true;
    scene.add_component<seed::DirectionalLightComponent>(sun, directional);

    const auto ambient = scene.create_entity("Sky Light");
    seed::AmbientLightComponent fill;
    fill.color = {0.35f, 0.55f, 1.0f};
    fill.intensity = 0.31f;
    fill.enabled = true;
    scene.add_component<seed::AmbientLightComponent>(ambient, fill);

    std::string error;
    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    assert(loaded.entity_count() == 2);

    seed::EntityId loaded_sun = seed::InvalidEntity;
    seed::EntityId loaded_ambient = seed::InvalidEntity;
    loaded.for_each_entity([&](seed::EntityId entity, const std::string& name) {
        if (name == "Sun") {
            loaded_sun = entity;
        } else if (name == "Sky Light") {
            loaded_ambient = entity;
        }
    });

    assert(loaded_sun != seed::InvalidEntity);
    assert(loaded_ambient != seed::InvalidEntity);

    const auto* loaded_transform = loaded.get_component<seed::TransformComponent>(loaded_sun);
    const auto* loaded_directional = loaded.get_component<seed::DirectionalLightComponent>(loaded_sun);
    const auto* loaded_fill = loaded.get_component<seed::AmbientLightComponent>(loaded_ambient);

    assert(loaded_transform != nullptr);
    assert(nearly_equal(loaded_transform->rotation_degrees.x, -42.0f));
    assert(nearly_equal(loaded_transform->rotation_degrees.y, 28.0f));

    assert(loaded_directional != nullptr);
    assert(nearly_equal(loaded_directional->color.x, 1.0f));
    assert(nearly_equal(loaded_directional->color.y, 0.82f));
    assert(nearly_equal(loaded_directional->color.z, 0.64f));
    assert(nearly_equal(loaded_directional->intensity, 1.75f));
    assert(loaded_directional->enabled);

    assert(loaded_fill != nullptr);
    assert(nearly_equal(loaded_fill->color.x, 0.35f));
    assert(nearly_equal(loaded_fill->color.y, 0.55f));
    assert(nearly_equal(loaded_fill->color.z, 1.0f));
    assert(nearly_equal(loaded_fill->intensity, 0.31f));
    assert(loaded_fill->enabled);

    std::filesystem::remove_all(test_root);
    std::cout << "SeedLightingPersistenceTests passed.\n";
    return 0;
}
