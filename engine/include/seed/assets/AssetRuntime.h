#pragma once

#include "seed/animation/AnimationData.h"
#include "seed/math/Math.h"
#include "seed/render/RenderResources.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace seed {

class IRenderer;
class Scene;

struct ModelAssetResource {
    MeshHandle mesh{};
    TextureHandle base_color_texture{};
    TextureHandle metallic_roughness_texture{};
    TextureHandle normal_texture{};
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.65f};
    float normal_scale{1.0f};
};

// Runtime-ready CPU/GPU data for an animated Seed character. The current
// renderer does not yet upload the skinned vertex stream as a GPU mesh, but the
// asset model is complete and renderer-agnostic: skeletons/clips live in Seed,
// while material textures use ordinary renderer handles.
struct AnimatedModelAssetResource {
    std::vector<SkinnedVertex> vertices;
    std::vector<std::uint32_t> indices;
    Skeleton skeleton;
    std::vector<AnimationClip> animations;

    TextureHandle base_color_texture{};
    TextureHandle metallic_roughness_texture{};
    TextureHandle normal_texture{};
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.65f};
    float normal_scale{1.0f};
};

// Resolves persistent Seed asset IDs into runtime resources. The cache is
// engine-level so Studio Play Mode and standalone Runtime share identical asset
// loading behavior.
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

    static std::string make_character_asset_id(const std::filesystem::path& project_relative_path);
    static bool is_character_asset_id(const std::string& asset_id);
    static std::optional<std::filesystem::path> character_asset_path(const std::string& asset_id);

    std::optional<ModelAssetResource> load_model(
        const std::string& asset_id,
        std::string* error = nullptr
    );

    // Loads skinned vertices, skeleton, clips and PBR material resources. GPU
    // skinned-mesh upload is intentionally a renderer milestone, so this object
    // currently remains CPU-resident except for ordinary material textures.
    const AnimatedModelAssetResource* load_character(
        const std::string& asset_id,
        std::string* error = nullptr
    );

    // Rebind imported static model resources after scene load or Play cloning.
    bool bind_scene(Scene& scene, ShaderHandle material_shader, std::string* error = nullptr);

    void clear();

private:
    IRenderer* m_renderer{nullptr};
    std::filesystem::path m_project_root;
    std::unordered_map<std::string, ModelAssetResource> m_models;
    std::unordered_map<std::string, AnimatedModelAssetResource> m_characters;
};

} // namespace seed
