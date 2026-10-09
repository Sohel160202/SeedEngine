#pragma once

#include <filesystem>
#include <string>

namespace seed {

class Scene;

class SceneSerializer {
public:
    static bool save(
        const Scene& scene,
        const std::filesystem::path& scene_file,
        std::string* error = nullptr
    );

    static bool load(
        Scene& scene,
        const std::filesystem::path& scene_file,
        std::string* error = nullptr
    );
};

} // namespace seed
