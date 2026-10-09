#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace seed {

struct ProjectDescriptor {
    std::uint32_t format_version{1};
    std::string name{"Untitled Seed Project"};
    std::filesystem::path startup_scene{"Scenes/Main.seedscene"};
};

class ProjectSerializer {
public:
    static bool save(
        const ProjectDescriptor& project,
        const std::filesystem::path& project_file,
        std::string* error = nullptr
    );

    static std::optional<ProjectDescriptor> load(
        const std::filesystem::path& project_file,
        std::string* error = nullptr
    );
};

} // namespace seed
