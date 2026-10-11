#pragma once

#include "seed/core/Types.h"
#include "seed/render/RenderResources.h"

#include <filesystem>
#include <string>

namespace seed {
class AssetRuntime;
class Scene;
}

namespace seed::studio {

struct CharacterImportResult {
    bool success{false};
    EntityId entity{InvalidEntity};
    std::string asset_id;
    std::string status_message;
    std::size_t animation_count{0};
};

// Beginner-facing animated-character import workflow used by Seed Studio.
// This owns project-file placement + semantic entity authoring, while the
// engine-owned AssetImporter and AssetRuntime remain responsible for parsing,
// CPU animation data and GPU/runtime resources.
CharacterImportResult import_animated_character_into_project(
    const std::filesystem::path& project_root,
    Scene& scene,
    AssetRuntime& assets,
    ShaderHandle skinned_material_shader,
    const std::filesystem::path& source_file
);

} // namespace seed::studio
