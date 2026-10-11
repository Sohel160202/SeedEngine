#include "seed/animation/AnimationComponents.h"
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

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_animation_persistence_test";
    const auto scene_file = root / "Character.seedscene";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    seed::Scene source;
    const seed::EntityId entity = source.create_entity("Hero", "hero-1");
    source.add_component<seed::TransformComponent>(entity);

    seed::SkinnedMeshComponent mesh;
    mesh.mesh = {99};
    mesh.asset_id = "character:Assets/Hero.gltf";
    mesh.visible = false;
    mesh.cast_shadows = false;
    mesh.receive_shadows = true;
    mesh.runtime_asset = reinterpret_cast<const seed::AnimatedModelAssetResource*>(0x1);
    source.add_component<seed::SkinnedMeshComponent>(entity, mesh);

    seed::AnimationPlayerComponent animation;
    animation.clip_name = "Run";
    animation.time_seconds = 12.5f;
    animation.playback_speed = 1.35f;
    animation.playing = false;
    animation.pose.local_transforms.resize(3);
    animation.pose.global_matrices.resize(3, seed::Mat4::identity());
    animation.pose.skin_matrices.resize(3, seed::Mat4::identity());
    source.add_component<seed::AnimationPlayerComponent>(entity, animation);

    seed::MaterialComponent material;
    material.asset_id = mesh.asset_id;
    material.base_color = {0.8f, 0.7f, 0.6f, 1.0f};
    material.metallic = 0.2f;
    material.roughness = 0.45f;
    source.add_component<seed::MaterialComponent>(entity, material);

    std::string error;
    assert(seed::SceneSerializer::save(source, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    const seed::EntityId loaded_entity = loaded.find_entity_by_persistent_id("hero-1");
    assert(loaded_entity != seed::InvalidEntity);

    const auto* loaded_mesh = loaded.get_component<seed::SkinnedMeshComponent>(loaded_entity);
    const auto* loaded_animation = loaded.get_component<seed::AnimationPlayerComponent>(loaded_entity);
    const auto* loaded_material = loaded.get_component<seed::MaterialComponent>(loaded_entity);
    assert(loaded_mesh != nullptr);
    assert(loaded_animation != nullptr);
    assert(loaded_material != nullptr);

    assert(loaded_mesh->asset_id == "character:Assets/Hero.gltf");
    assert(!loaded_mesh->visible);
    assert(!loaded_mesh->cast_shadows);
    assert(loaded_mesh->receive_shadows);
    assert(!loaded_mesh->mesh);
    assert(loaded_mesh->runtime_asset == nullptr);

    assert(loaded_animation->clip_name == "Run");
    assert(near(loaded_animation->playback_speed, 1.35f));
    assert(!loaded_animation->playing);

    // Runtime playback state is intentionally rebuilt, never persisted.
    assert(near(loaded_animation->time_seconds, 0.0f));
    assert(loaded_animation->pose.local_transforms.empty());
    assert(loaded_animation->pose.global_matrices.empty());
    assert(loaded_animation->pose.skin_matrices.empty());

    assert(loaded_material->asset_id == "character:Assets/Hero.gltf");
    assert(near(loaded_material->roughness, 0.45f));

    std::filesystem::remove_all(root);
    std::cout << "Seed skinned character scene persistence tests passed.\n";
    return 0;
}
