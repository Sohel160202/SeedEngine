#include "seed/animation/AnimationSystem.h"
#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/CombatSystem.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/gameplay/GameplayFeedbackSystem.h"

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

    seed::GameplayRuntime gameplay_runtime;
    if (!gameplay_runtime.start(scene, player)) {
        engine.shutdown();
        return 1;
    }

    engine.register_update_system(
        "Input",
        seed::SystemPhase::Input,
        [&](seed::Scene&, double, seed::GameplayEventBus&) {
            for (const auto& event : engine.frame_events()) {
                gameplay_runtime.handle_event(event);
            }
        }
    );
    engine.register_update_system(
        "Gameplay",
        seed::SystemPhase::Gameplay,
        [&](seed::Scene& runtime_scene, double delta_seconds, seed::GameplayEventBus& events) {
            gameplay_runtime.update(runtime_scene, delta_seconds, &events);
        }
    );
    engine.register_update_system(
        "Animation",
        seed::SystemPhase::Animation,
        [](seed::Scene& runtime_scene, double delta_seconds, seed::GameplayEventBus& events) {
            seed::AnimationSystem::update_scene(
                runtime_scene,
                static_cast<float>(delta_seconds),
                &events
            );
        }
    );
    seed::GameplayFeedbackBus feedback;

    engine.register_update_system(
        "Combat",
        seed::SystemPhase::EventDispatch,
        [](seed::Scene& runtime_scene, double, seed::GameplayEventBus& events) {
            seed::CombatSystem::consume_events(runtime_scene, events);
        }
    );
    engine.register_update_system(
        "Feedback",
        seed::SystemPhase::EventDispatch,
        [&](seed::Scene&, double, seed::GameplayEventBus& events) {
            seed::GameplayFeedbackSystem::consume_events(events, feedback, &runtime_scene);
        }
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

        engine.update();
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
