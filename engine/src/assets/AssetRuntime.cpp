#include "seed/assets/AssetRuntime.h"

#include "seed/assets/AssetImporter.h"
#include "seed/render/IRenderer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <system_error>

namespace seed {
namespace {

constexpr std::string_view ModelAssetPrefix{"model:"};

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

AssetRuntime::AssetRuntime(IRenderer& renderer)
    : m_renderer(&renderer) {}

AssetRuntime::~AssetRuntime() {
    clear();
}

void AssetRuntime::set_project_root(std::filesystem::path project_root) {
    if (project_root == m_project_root) {
        return;
    }
    clear();
    m_project_root = std::move(project_root);
}

std::string AssetRuntime::make_model_asset_id(const std::filesystem::path& project_relative_path) {
    return std::string{ModelAssetPrefix} + project_relative_path.generic_string();
}

bool AssetRuntime::is_model_asset_id(const std::string& asset_id) {
    return std::string_view{asset_id}.starts_with(ModelAssetPrefix);
}

std::optional<std::filesystem::path> AssetRuntime::model_asset_path(const std::string& asset_id) {
    if (!is_model_asset_id(asset_id)) {
        return std::nullopt;
    }

    const std::string_view value{asset_id};
    const std::string_view relative = value.substr(ModelAssetPrefix.size());
    if (relative.empty()) {
        return std::nullopt;
    }
    return std::filesystem::path{relative};
}

std::optional<ModelAssetResource> AssetRuntime::load_model(
    const std::string& asset_id,
    std::string* error
) {
    if (const auto found = m_models.find(asset_id); found != m_models.end()) {
        return found->second;
    }

    if (m_renderer == nullptr) {
        set_error(error, "Seed AssetRuntime has no renderer.");
        return std::nullopt;
    }
    if (m_project_root.empty()) {
        set_error(error, "Seed AssetRuntime has no project root.");
        return std::nullopt;
    }

    const auto relative_path = model_asset_path(asset_id);
    if (!relative_path.has_value() || relative_path->is_absolute()) {
        set_error(error, "Invalid Seed model asset ID: " + asset_id);
        return std::nullopt;
    }

    const std::filesystem::path source_file = m_project_root / *relative_path;
    std::string import_error;
    auto imported = AssetImporter::import_static_model(source_file, &import_error);
    if (!imported.has_value()) {
        set_error(error, import_error.empty() ? "Could not import model asset." : import_error);
        return std::nullopt;
    }

    ModelAssetResource resource;
    resource.mesh = m_renderer->create_mesh({
        .vertices = imported->vertices,
        .indices = imported->indices,
    });
    if (!resource.mesh) {
        set_error(error, "Renderer could not create mesh for " + asset_id);
        return std::nullopt;
    }

    resource.base_color = imported->base_color;
    if (imported->base_color_texture.has_value() && imported->base_color_texture->valid()) {
        const auto& texture = *imported->base_color_texture;
        resource.base_color_texture = m_renderer->create_texture({
            .width = texture.width,
            .height = texture.height,
            .rgba8_pixels = texture.rgba8_pixels,
        });
        if (!resource.base_color_texture) {
            m_renderer->destroy_mesh(resource.mesh);
            set_error(error, "Renderer could not create base-color texture for " + asset_id);
            return std::nullopt;
        }
    }

    m_models.emplace(asset_id, resource);
    return resource;
}

bool AssetRuntime::bind_scene(Scene& scene, ShaderHandle material_shader, std::string* error) {
    bool success = true;
    std::string first_error;

    scene.for_each<MeshComponent>([&](EntityId entity, MeshComponent& mesh) {
        if (!is_model_asset_id(mesh.asset_id)) {
            return;
        }

        std::string load_error;
        const auto resource = load_model(mesh.asset_id, &load_error);
        if (!resource.has_value()) {
            success = false;
            if (first_error.empty()) {
                first_error = load_error;
            }
            mesh.mesh = {};
            return;
        }

        mesh.mesh = resource->mesh;
        if (auto* material = scene.get_component<MaterialComponent>(entity)) {
            material->shader = material_shader;
            material->asset_id = mesh.asset_id;
            material->base_color = resource->base_color;
            material->base_color_texture = resource->base_color_texture;
        }
    });

    if (!success) {
        set_error(error, first_error.empty() ? "One or more Seed model assets failed to bind." : first_error);
    }
    return success;
}

void AssetRuntime::clear() {
    if (m_renderer != nullptr) {
        for (const auto& [asset_id, resource] : m_models) {
            (void)asset_id;
            if (resource.base_color_texture) {
                m_renderer->destroy_texture(resource.base_color_texture);
            }
            if (resource.mesh) {
                m_renderer->destroy_mesh(resource.mesh);
            }
        }
    }
    m_models.clear();
}

} // namespace seed
