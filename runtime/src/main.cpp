#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <iostream>

int main() {
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
    }

    engine.shutdown();
    return 0;
}
