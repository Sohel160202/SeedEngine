#pragma once

#include "seed/core/Types.h"

#include <filesystem>
#include <string>

namespace seed {

class Scene;

class PrefabSerializer {
public:
    static bool save(
        const Scene& scene,
        EntityId entity,
        const std::filesystem::path& prefab_file,
        std::string* error = nullptr
    );

    static bool instantiate(
        Scene& scene,
        const std::filesystem::path& prefab_file,
        EntityId* created_entity = nullptr,
        std::string* error = nullptr
    );
};

} // namespace seed
