#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    seed::Engine engine({
        .application_name = "Seed Studio",
        .window_width = 1440,
        .window_height = 900,
        .window_resizable = true,
        .create_window = true,
    });

    if (!engine.start()) {
        return 1;
    }

    auto& scene = engine.scene();
    const auto preview_entity = scene.create_entity("Seed Studio Preview Entity");
    scene.add_component<seed::TransformComponent>(preview_entity);

    std::cout << "[SeedStudio] Native editor window active.\n";
    std::cout << "[SeedStudio] Press Escape to close. Next milestone: renderer clear frame.\n";

    while (engine.tick()) {
        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key &&
                event.key == seed::KeyCode::Escape &&
                event.button_state == seed::ButtonState::Pressed) {
                engine.request_exit();
            }
        }

        // Temporary Phase 1 pacing until Seed's renderer controls presentation.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    engine.shutdown();
    return 0;
}
