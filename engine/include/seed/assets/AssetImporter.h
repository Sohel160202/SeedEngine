#pragma once

#include "seed/math/Math.h"
#include "seed/render/RenderResources.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace seed {

struct ImportedTextureData {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::vector<std::uint8_t> rgba8_pixels;

    bool valid() const noexcept {
        return width > 0 && height > 0 &&
            rgba8_pixels.size() >= static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height) * 4u;
    }
};

// Seed-owned CPU representation of an imported static model. TinyGLTF types
// never cross this boundary.
struct ImportedModelData {
    std::string name;
    std::vector<VertexPositionColor> vertices;
    std::vector<std::uint32_t> indices;

    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic{0.0f};
    float roughness{0.65f};
    float normal_scale{1.0f};

    std::optional<ImportedTextureData> base_color_texture;
    std::optional<ImportedTextureData> metallic_roughness_texture;
    std::optional<ImportedTextureData> normal_texture;

    // Relative URI dependencies used by .gltf files. .glb normally has none.
    // Studio uses this list when copying an imported source into Assets/.
    std::vector<std::filesystem::path> external_files;
};

class AssetImporter {
public:
    static bool supports_static_model(const std::filesystem::path& source_file);

    // Import v1 intentionally uses the first triangle primitive. It supports
    // positions, UVs, vertex colors, normals (or generated normals), glTF PBR
    // factors, base-color maps, metallic/roughness maps and normal maps.
    // Multi-primitive assets and skinning remain future Seed milestones.
    static std::optional<ImportedModelData> import_static_model(
        const std::filesystem::path& source_file,
        std::string* error = nullptr
    );
};

} // namespace seed
