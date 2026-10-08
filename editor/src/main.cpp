#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <iostream>

int main() {
    seed::Engine engine({.application_name = "Seed Studio"});
    engine.start();

    auto& scene = engine.scene();
    const auto preview_entity = scene.create_entity("Seed Studio Preview Entity");
    scene.add_component<seed::TransformComponent>(preview_entity);

    std::cout << "[SeedStudio] Editor bootstrap ready.\n";
    std::cout << "[SeedStudio] Next: platform window + renderer + visual scene viewport.\n";

    engine.shutdown();
    return 0;
}
