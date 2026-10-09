#pragma once

#include "seed/math/Math.h"
#include "seed/render/RenderResources.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace seed {

class IRenderer;
class Scene;

struct ModelAssetResource {
    MeshHandle mesh{};
    TextureHandle base_color_texture{};
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.65f};
};

// Resolves persistent Seed model asset IDs into renderer resources. The cache is
// engine-level so Studio Play Mode and the standalone Runtime can share the same
// asset-loading behavior.
class AssetRuntime {
public:
    explicit AssetRuntime(IRenderer& renderer);
    ~AssetRuntime();

    AssetRuntime(const AssetRuntime&) = delete;
    AssetRuntime& operator=(const AssetRuntime&) = delete;

    void set_project_root(std::filesystem::path project_root);
    const std::filesystem::path& project_root() const noexcept { return m_project_root; }

    static std::string make_model_asset_id(const std::filesystem::path& project_relative_path);
    static bool is_model_asset_id(const std::string& asset_id);
    static std::optional<std::filesystem::path> model_asset_path(const std::string& asset_id);

    std::optional<ModelAssetResource> load_model(
        const std::string& asset_id,
        std::string* error = nullptr
    );

    // Rebind imported model resources after scene load or Play-mode cloning.
    // The supplied shader remains Seed-owned; source glTF shaders are not used.
    bool bind_scene(Scene& scene, ShaderHandle material_shader, std::string* error = nullptr);

    void clear();

private:
    IRenderer* m_renderer{nullptr};
    std::filesystem::path m_project_root;
    std::unordered_map<std::string, ModelAssetResource> m_models;
};

} // namespace seed
