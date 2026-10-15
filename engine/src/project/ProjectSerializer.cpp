#include "seed/project/ProjectSerializer.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

bool ProjectSerializer::save(
    const ProjectDescriptor& project,
    const std::filesystem::path& project_file,
    std::string* error
) {
    try {
        if (!project_file.parent_path().empty()) {
            std::filesystem::create_directories(project_file.parent_path());
        }

        nlohmann::json document = {
            {"seed_format", "seedproject"},
            {"format_version", project.format_version},
            {"name", project.name},
            {"startup_scene", project.startup_scene.generic_string()},
            {"window_width", project.window_width},
            {"window_height", project.window_height},
            {"window_resizable", project.window_resizable},
            {"vsync", project.vsync},
            {"window_icon", project.window_icon.generic_string()},
            {"input_profile", project.input_profile},
            {"build_profile", project.build_profile},
        };

        std::ofstream output(project_file, std::ios::binary | std::ios::trunc);
        if (!output) {
            set_error(error, "Could not open project file for writing: " + project_file.string());
            return false;
        }

        output << document.dump(2) << '\n';
        return static_cast<bool>(output);
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

std::optional<ProjectDescriptor> ProjectSerializer::load(
    const std::filesystem::path& project_file,
    std::string* error
) {
    try {
        std::ifstream input(project_file, std::ios::binary);
        if (!input) {
            set_error(error, "Could not open project file: " + project_file.string());
            return std::nullopt;
        }

        nlohmann::json document;
        input >> document;

        if (document.value("seed_format", std::string{}) != "seedproject") {
            set_error(error, "File is not a Seed project: " + project_file.string());
            return std::nullopt;
        }

        ProjectDescriptor project;
        project.format_version = document.value("format_version", 1u);
        project.name = document.value("name", std::string{"Untitled Seed Project"});
        project.startup_scene = document.value("startup_scene", std::string{"Scenes/Main.seedscene"});
        project.window_width = document.value("window_width", 1280u);
        project.window_height = document.value("window_height", 720u);
        project.window_resizable = document.value("window_resizable", true);
        project.vsync = document.value("vsync", true);
        project.window_icon = document.value("window_icon", std::string{});
        project.input_profile = document.value("input_profile", std::string{"default"});
        project.build_profile = document.value("build_profile", std::string{"Development"});
        return project;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return std::nullopt;
    }
}

} // namespace seed
