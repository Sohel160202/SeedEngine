#include "PlayChangesReview.h"
#include "StudioShaders.h"
#include "StudioUI.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/animation/AnimationSystem.h"
#include "seed/assets/AssetImporter.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/project/ProjectSerializer.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderComponents.h"
#include "seed/render/RenderSystem.h"
#include "seed/render/SkinnedShaders.h"
#include "seed/scene/SceneDiff.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
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
    if (const auto* v = scene.get_component<seed::HealthComponent>(source)) {
        scene.add_component<seed::HealthComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::InventoryComponent>(source)) {
        scene.add_component<seed::InventoryComponent>(copy, *v);
    }
    if (const auto* v = scene.get_component<seed::PlayerControllerComponent>(source)) {
        scene.add_component<seed::PlayerControllerComponent>(copy, *v);
    }
    return copy;
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

    if (playing) {
        info.gameplay_prompt = gameplay.interaction_prompt();
        info.gameplay_status = gameplay.status_message();
        if (play_scene && play_player != seed::InvalidEntity && play_scene->is_alive(play_player)) {
            if (const auto* inventory = play_scene->get_component<seed::InventoryComponent>(play_player)) {
                for (const auto& [item_id, quantity] : inventory->items) {
                    info.gameplay_inventory.emplace_back(item_id, quantity);
                }
                std::sort(info.gameplay_inventory.begin(), info.gameplay_inventory.end());
            }
        }
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
                } else if (!has_pending_play_changes) {
                    engine.request_exit();
                }
                continue;
            }

            if (has_pending_play_changes) continue;

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
            gameplay.update(play_scene, engine.delta_seconds());
            seed::AnimationSystem::update_scene(
                play_scene,
                static_cast<float>(engine.delta_seconds())
            );
        } else if (!has_pending_play_changes) {
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
                scene.clear();
                editor_camera = create_editor_camera(scene);
                const auto cube = create_cube(scene, cube_mesh, mesh_shader);
                create_default_lighting(scene);
                studio_ui.select_entity(cube);
                workspace = {};
                if (save_workspace_as(workspace, scene, action.path, action.project_name)) {
                    asset_runtime.set_project_root(workspace.project_file.parent_path());
                }
                break;
            }
            case seed::studio::StudioActionType::OpenProject:
                if (has_pending_play_changes) break;
                if (open_workspace(
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
            case seed::studio::StudioActionType::Save:
                if (!has_pending_play_changes) save_workspace(workspace, scene);
                break;
            case seed::studio::StudioActionType::SaveAs:
                if (has_pending_play_changes) break;
                if (save_workspace_as(workspace, scene, action.path, action.project_name)) {
                    asset_runtime.set_project_root(workspace.project_file.parent_path());
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
                const auto entity = import_model_into_project(
                    workspace,
                    scene,
                    asset_runtime,
                    mesh_shader,
                    action.path
                );
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
