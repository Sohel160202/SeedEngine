#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <charconv>
#include <cstdint>
#include <iostream>
#include <string_view>

namespace {

std::uint64_t parse_frame_limit(int argc, char** argv) {
    constexpr std::string_view prefix{"--frames="};

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (!argument.starts_with(prefix)) {
            continue;
        }

        std::uint64_t value = 0;
        const auto number = argument.substr(prefix.size());
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
        if (result.ec == std::errc{} && result.ptr == number.data() + number.size()) {
            return value;
        }
    }

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    const std::uint64_t frame_limit = parse_frame_limit(argc, argv);

    seed::Engine engine({
        .application_name = "Seed Runtime",
        .window_width = 1280,
        .window_height = 720,
        .window_resizable = true,
        .create_window = true,
        .renderer_backend = seed::RendererBackend::OpenGL,
        .vsync = true,
        .clear_color = {.r = 0.035f, .g = 0.055f, .b = 0.040f, .a = 1.0f},
    });

    if (!engine.start()) {
        return 1;
    }

    auto& scene = engine.scene();
    const auto player = scene.create_entity("Player");
    scene.add_component<seed::TransformComponent>(player);
    scene.add_component<seed::InventoryComponent>(player);

    const auto door = scene.create_entity("Cabin Door");
    scene.add_component<seed::TransformComponent>(door);
    scene.add_component<seed::InteractableComponent>(door, "Open Cabin Door", true);
    scene.add_component<seed::DoorComponent>(
        door,
        seed::DoorMotion::Rotate,
        90.0f,
        0.75f,
        "CabinKey",
        false,
        false
    );

    std::cout << "[SeedRuntime] Scene booted with " << scene.entity_count() << " entities.\n";
    std::cout << "[SeedRuntime] Seed renderer active. Press Escape to close.\n";

    while (engine.tick()) {
        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key &&
                event.key == seed::KeyCode::Escape &&
                event.button_state == seed::ButtonState::Pressed) {
                engine.request_exit();
            }
        }

        engine.begin_frame();
        // Future game-world rendering is submitted here.
        engine.end_frame();

        if (frame_limit > 0 && engine.frame_index() >= frame_limit) {
            engine.request_exit();
        }
    }

    engine.shutdown();
    return 0;
}
