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

// Seed's first imported static-model representation. The importer deliberately
// stops at Seed-owned CPU data; renderer handles and TinyGLTF types never leak
// through this interface.
struct ImportedModelData {
    std::string name;
    std::vector<VertexPositionColor> vertices;
    std::vector<std::uint32_t> indices;
    Vec4 base_color{1.0f, 1.0f, 1.0f, 1.0f};
    std::optional<ImportedTextureData> base_color_texture;

    // Relative URI dependencies used by .gltf files. .glb normally has none.
    // Studio uses this list when copying an imported source into Assets/.
    std::vector<std::filesystem::path> external_files;
};

class AssetImporter {
public:
    static bool supports_static_model(const std::filesystem::path& source_file);

    // Physics/rendering v0 intentionally imports the first triangle primitive
    // from a glTF/GLB file. Multi-primitive models, skinning and full PBR arrive
    // in later asset milestones without changing Seed's public scene model.
    static std::optional<ImportedModelData> import_static_model(
        const std::filesystem::path& source_file,
        std::string* error = nullptr
    );
};

} // namespace seed
