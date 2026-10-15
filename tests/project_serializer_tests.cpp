#include "seed/project/ProjectSerializer.h"

#include <cassert>
#include <filesystem>
#include <string>

int main() {
    const auto path =
        std::filesystem::temp_directory_path() / "seed_project_settings_test.seedproject";

    seed::ProjectDescriptor project;
    project.name = "Shop Simulator";
    project.startup_scene = "Scenes/Shop.seedscene";
    project.window_width = 1600;
    project.window_height = 900;
    project.window_resizable = false;
    project.vsync = false;
    project.window_icon = "Assets/shop.ico";
    project.input_profile = "shop_controls";
    project.build_profile = "Release";

    std::string error;
    assert(seed::ProjectSerializer::save(project, path, &error));

    const auto loaded = seed::ProjectSerializer::load(path, &error);
    assert(loaded.has_value());
    assert(loaded->name == project.name);
    assert(loaded->startup_scene == project.startup_scene);
    assert(loaded->window_width == 1600);
    assert(loaded->window_height == 900);
    assert(!loaded->window_resizable);
    assert(!loaded->vsync);
    assert(loaded->window_icon == project.window_icon);
    assert(loaded->input_profile == project.input_profile);
    assert(loaded->build_profile == project.build_profile);

    std::filesystem::remove(path);
    return 0;
}
