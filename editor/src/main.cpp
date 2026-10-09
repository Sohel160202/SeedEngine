#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <iostream>

int main() {
    seed::Engine engine({
        .application_name = "Seed Studio",
        .window_width = 1440,
        .window_height = 900,
        .window_resizable = true,
        .create_window = true,
        .renderer_backend = seed::RendererBackend::OpenGL,
        .vsync = true,
        .clear_color = {.r = 0.040f, .g = 0.070f, .b = 0.050f, .a = 1.0f},
    });

    if (!engine.start()) {
        return 1;
    }

    auto& scene = engine.scene();
    const auto preview_entity = scene.create_entity("Seed Studio Preview Entity");
    scene.add_component<seed::TransformComponent>(preview_entity);

    std::cout << "[SeedStudio] Native editor window active.\n";
    std::cout << "[SeedStudio] Seed renderer active. Press Escape to close.\n";

    while (engine.tick()) {
        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key &&
                event.key == seed::KeyCode::Escape &&
                event.button_state == seed::ButtonState::Pressed) {
                engine.request_exit();
            }
        }

        engine.begin_frame();
        // The future Seed Studio viewport and editor UI render here.
        engine.end_frame();
    }

    engine.shutdown();
    return 0;
}
