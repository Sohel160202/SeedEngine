#include "CharacterImportWorkflow.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/animation/AnimationSystem.h"
#include "seed/assets/AssetImporter.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace seed::studio {
namespace {

std::string safe_stem(std::string value) {
    for (char& c : value) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (!std::isalnum(u) && c != '-' && c != '_') c = '_';
    }
    while (!value.empty() && value.front() == '_') value.erase(value.begin());
    while (!value.empty() && value.back() == '_') value.pop_back();
    return value.empty() ? "Character" : value;
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
    std::string& error
) {
    try {
        if (!std::filesystem::exists(source)) {
            error = "Missing source dependency: " + source.string();
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
        error = exception.what();
        return false;
    }
}

} // namespace

CharacterImportResult import_animated_character_into_project(
    const std::filesystem::path& project_root,
    Scene& scene,
    AssetRuntime& assets,
    ShaderHandle skinned_material_shader,
    const std::filesystem::path& source_file
) {
    CharacterImportResult result;

    if (project_root.empty()) {
        result.status_message = "Save the Seed project before importing a character.";
        return result;
    }
    if (source_file.empty()) {
        result.status_message = "Choose a glTF or GLB character file first.";
        return result;
    }
    if (!AssetImporter::supports_animated_model(source_file)) {
        result.status_message = "Seed animated-character import currently supports glTF 2.0 .gltf/.glb files.";
        return result;
    }

    std::string import_error;
    const auto imported = AssetImporter::import_animated_model(source_file, &import_error);
    if (!imported.has_value()) {
        result.status_message = "Character import failed: " + import_error;
        return result;
    }

    if (imported->skeleton.joints.empty()) {
        result.status_message = "Character import failed: the file has no usable skin/skeleton.";
        return result;
    }

    const std::string folder_name = safe_stem(source_file.stem().string());
    const std::filesystem::path destination_directory =
        project_root / "Assets" / "Characters" / folder_name;
    const std::filesystem::path destination_source =
        destination_directory / source_file.filename();

    std::string copy_error;
    if (!copy_file_if_needed(source_file, destination_source, copy_error)) {
        result.status_message = "Character import copy failed: " + copy_error;
        return result;
    }

    for (const auto& dependency : imported->external_files) {
        if (!safe_dependency_path(dependency)) {
            result.status_message =
                "Character import rejected unsafe glTF dependency path: " + dependency.string();
            return result;
        }
        if (!copy_file_if_needed(
                source_file.parent_path() / dependency,
                destination_directory / dependency,
                copy_error)) {
            result.status_message = "Character dependency copy failed: " + copy_error;
            return result;
        }
    }

    std::filesystem::path relative_source;
    try {
        relative_source = std::filesystem::relative(destination_source, project_root);
    } catch (const std::exception& exception) {
        result.status_message = std::string{"Character import path failed: "} + exception.what();
        return result;
    }

    assets.set_project_root(project_root);
    const std::string asset_id = AssetRuntime::make_character_asset_id(relative_source);
    std::string asset_error;
    const AnimatedModelAssetResource* resource = assets.load_character(asset_id, &asset_error);
    if (resource == nullptr) {
        result.status_message =
            "Character source was copied, but Seed could not create the runtime asset: " + asset_error;
        return result;
    }
    if (!resource->mesh) {
        result.status_message =
            "Character source was imported, but the active renderer could not create its skinned GPU mesh.";
        return result;
    }

    const std::string entity_name = imported->name.empty()
        ? source_file.stem().string()
        : imported->name;
    const EntityId entity = scene.create_entity(entity_name.empty() ? "Animated Character" : entity_name);
    scene.add_component<TransformComponent>(entity);

    SkinnedMeshComponent mesh;
    mesh.mesh = resource->mesh;
    mesh.asset_id = asset_id;
    mesh.visible = true;
    mesh.cast_shadows = true;
    mesh.receive_shadows = true;
    mesh.runtime_asset = resource;
    scene.add_component<SkinnedMeshComponent>(entity, mesh);

    MaterialComponent material;
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
    scene.add_component<MaterialComponent>(entity, material);

    AnimationPlayerComponent animation;
    if (!resource->animations.empty()) animation.clip_name = resource->animations.front().name;
    animation.time_seconds = 0.0f;
    animation.playback_speed = 1.0f;
    animation.playing = true;
    AnimationSystem::initialize_bind_pose(resource->skeleton, animation.pose);
    scene.add_component<AnimationPlayerComponent>(entity, std::move(animation));

    result.success = true;
    result.entity = entity;
    result.asset_id = asset_id;
    result.animation_count = resource->animations.size();
    result.status_message = "Imported animated character: " + asset_id;
    if (resource->animations.empty()) {
        result.status_message += " (skinned character has no animation clips)";
    } else {
        result.status_message += " (" + std::to_string(resource->animations.size()) +
            (resource->animations.size() == 1 ? " clip)" : " clips)");
    }
    return result;
}

} // namespace seed::studio
