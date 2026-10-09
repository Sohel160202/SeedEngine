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
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_render_persistence_test";
    const auto scene_file = root / "Render.seedscene";
    std::filesystem::remove_all(root);

    seed::Scene scene;

    const auto object = scene.create_entity("PBR Object");
    scene.add_component<seed::TransformComponent>(object);
    seed::MeshComponent mesh;
    mesh.asset_id = "builtin:cube";
    mesh.cast_shadows = false;
    mesh.receive_shadows = true;
    scene.add_component<seed::MeshComponent>(object, mesh);
    seed::MaterialComponent material;
    material.base_color = {0.2f, 0.4f, 0.8f, 1.0f};
    material.metallic = 0.72f;
    material.roughness = 0.18f;
    scene.add_component<seed::MaterialComponent>(object, material);

    const auto sun = scene.create_entity("Sun");
    auto& sun_transform = scene.add_component<seed::TransformComponent>(sun);
    sun_transform.rotation_degrees = {-55.0f, 25.0f, 0.0f};
    seed::DirectionalLightComponent directional;
    directional.color = {1.0f, 0.88f, 0.72f};
    directional.intensity = 3.0f;
    directional.casts_shadows = true;
    directional.shadow_distance = 52.0f;
    scene.add_component<seed::DirectionalLightComponent>(sun, directional);

    const auto sky_entity = scene.create_entity("Sky");
    seed::SkyComponent sky;
    sky.zenith_color = {0.02f, 0.08f, 0.22f};
    sky.horizon_color = {0.75f, 0.42f, 0.26f};
    sky.intensity = 1.35f;
    scene.add_component<seed::SkyComponent>(sky_entity, sky);

    std::string error;
    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    assert(loaded.entity_count() == 3);

    seed::EntityId loaded_object = seed::InvalidEntity;
    seed::EntityId loaded_sun = seed::InvalidEntity;
    seed::EntityId loaded_sky = seed::InvalidEntity;
    loaded.for_each_entity([&](seed::EntityId entity, const std::string& name) {
        if (name == "PBR Object") loaded_object = entity;
        if (name == "Sun") loaded_sun = entity;
        if (name == "Sky") loaded_sky = entity;
    });

    const auto* loaded_mesh = loaded.get_component<seed::MeshComponent>(loaded_object);
    const auto* loaded_material = loaded.get_component<seed::MaterialComponent>(loaded_object);
    const auto* loaded_directional = loaded.get_component<seed::DirectionalLightComponent>(loaded_sun);
    const auto* loaded_sky_component = loaded.get_component<seed::SkyComponent>(loaded_sky);

    assert(loaded_mesh != nullptr);
    assert(!loaded_mesh->cast_shadows);
    assert(loaded_mesh->receive_shadows);

    assert(loaded_material != nullptr);
    assert(nearly_equal(loaded_material->base_color.x, 0.2f));
    assert(nearly_equal(loaded_material->base_color.z, 0.8f));
    assert(nearly_equal(loaded_material->metallic, 0.72f));
    assert(nearly_equal(loaded_material->roughness, 0.18f));
    assert(!loaded_material->use_asset_defaults);

    assert(loaded_directional != nullptr);
    assert(loaded_directional->casts_shadows);
    assert(nearly_equal(loaded_directional->shadow_distance, 52.0f));
    assert(nearly_equal(loaded_directional->intensity, 3.0f));

    assert(loaded_sky_component != nullptr);
    assert(nearly_equal(loaded_sky_component->zenith_color.z, 0.22f));
    assert(nearly_equal(loaded_sky_component->horizon_color.x, 0.75f));
    assert(nearly_equal(loaded_sky_component->intensity, 1.35f));

    std::filesystem::remove_all(root);
    std::cout << "SeedRenderPersistenceTests passed.\n";
    return 0;
}
