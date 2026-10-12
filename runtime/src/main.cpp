#include "seed/animation/AnimationSystem.h"
#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/gameplay/CombatSystem.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/gameplay/AudioSystem.h"
#include "seed/gameplay/GameplayFeedbackSystem.h"
#include "seed/gameplay/GameplayRulesSystem.h"
#include "seed/project/ProjectSerializer.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderSystem.h"
#include "seed/save/SaveGameService.h"
#include "RuntimeUI.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {

std::uint64_t parse_frame_limit(int argc, char** argv) {
    constexpr std::string_view prefix{"--frames="};

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (!argument.starts_with(prefix)) continue;

        std::uint64_t value = 0;
        const auto number = argument.substr(prefix.size());
        const auto result = std::from_chars(number.data(), number.data() + number.size(), value);
        if (result.ec == std::errc{} && result.ptr == number.data() + number.size()) return value;
    }

    return 0;
}

std::filesystem::path parse_project_file(int argc, char** argv) {
    constexpr std::string_view prefix{"--project="};

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (argument.starts_with(prefix)) {
            return std::filesystem::path{argument.substr(prefix.size())};
        }
    }

    return {};
}

bool load_project_scene(
    seed::Scene& scene,
    const std::filesystem::path& project_file,
    seed::EntityId& player_entity
) {
    std::string error;
    const auto project = seed::ProjectSerializer::load(project_file, &error);
    if (!project.has_value()) {
        std::cerr << "[SeedRuntime] " << error << '\n';
        return false;
    }

    const std::filesystem::path scene_file =
        project_file.parent_path() / project->startup_scene;
    if (!seed::SceneSerializer::load(scene, scene_file, &error)) {
        std::cerr << "[SeedRuntime] " << error << '\n';
        return false;
    }

    player_entity = scene.find_entity_by_persistent_id("player");
    if (player_entity == seed::InvalidEntity) {
        std::cerr << "[SeedRuntime] Project startup scene has no entity with id 'player'.\n";
        return false;
    }

    std::cout << "[SeedRuntime] Loaded project '" << project->name
              << "' from " << project_file << ".\n";
    std::cout << "[SeedRuntime] Startup scene: " << scene_file << ".\n";
    return true;
}

void ensure_player_dependencies(seed::Scene& scene, seed::EntityId player) {
    if (!scene.has_component<seed::TransformComponent>(player)) {
        scene.add_component<seed::TransformComponent>(player);
    }
    if (!scene.has_component<seed::PlayerControllerComponent>(player)) {
        seed::PlayerControllerComponent controller;
        controller.view_mode = seed::PlayerViewMode::ThirdPerson;
        scene.add_component<seed::PlayerControllerComponent>(player, controller);
    }
    if (!scene.has_component<seed::InventoryComponent>(player)) {
        scene.add_component<seed::InventoryComponent>(player);
    }
    if (!scene.has_component<seed::HealthComponent>(player)) {
        scene.add_component<seed::HealthComponent>(player);
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::uint64_t frame_limit = parse_frame_limit(argc, argv);
    const std::filesystem::path project_file = parse_project_file(argc, argv);

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

    if (!engine.start()) return 1;

    auto& scene = engine.scene();
    seed::EntityId player = seed::InvalidEntity;
    if (!project_file.empty()) {
        if (!load_project_scene(scene, project_file, player)) {
            engine.shutdown();
            return 1;
        }
    } else {
        player = scene.create_entity("Player", "player");
        scene.add_component<seed::TransformComponent>(player);
        scene.add_component<seed::InventoryComponent>(player);
        scene.add_component<seed::HealthComponent>(player);

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
    }
    ensure_player_dependencies(scene, player);

    seed::SaveGameService save_service(
        project_file.empty()
            ? std::filesystem::path{"Saves"}
            : project_file.parent_path() / "Saves"
    );
    const bool save_enabled = scene.find_entity_by_persistent_id("player") != seed::InvalidEntity;

    seed::runtime::RuntimeUI runtime_ui;
    const bool runtime_ui_available =
        runtime_ui.initialize(engine.platform()->window_handle());

    seed::GameplayRuntime gameplay_runtime;
    if (!gameplay_runtime.start(scene, player)) {
        engine.shutdown();
        return 1;
    }

    engine.register_update_system(
        "Input",
        seed::SystemPhase::Input,
        [&](seed::Scene&, double, seed::GameplayEventBus&) {
            for (const auto& event : engine.frame_events()) gameplay_runtime.handle_event(event);
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
    seed::MiniaudioOutput miniaudio_output;
    seed::SilentAudioOutput silent_audio_output;
    seed::AudioOutput* audio_output = &silent_audio_output;
    if (miniaudio_output.initialize()) {
        audio_output = &miniaudio_output;
        std::cout << "[SeedRuntime] Miniaudio audio output initialized.\n";
    } else {
        std::cout << "[SeedRuntime] Audio device unavailable; using silent output.\n";
    }

    engine.register_update_system(
        "Gameplay Rules",
        seed::SystemPhase::EventDispatch,
        [](seed::Scene& runtime_scene, double, seed::GameplayEventBus& events) {
            seed::GameplayRulesSystem::consume_events(runtime_scene, events);
        }
    );
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
        [&](seed::Scene& runtime_scene, double, seed::GameplayEventBus& events) {
            seed::GameplayFeedbackSystem::consume_events(events, feedback, &runtime_scene);
        }
    );
    engine.register_update_system(
        "Audio",
        seed::SystemPhase::EventDispatch,
        [&](seed::Scene&, double, seed::GameplayEventBus&) {
            seed::AudioSystem::consume_feedback(feedback, *audio_output);
        }
    );
    engine.register_update_system(
        "FeedbackCleanup",
        seed::SystemPhase::EventDispatch,
        [&](seed::Scene&, double, seed::GameplayEventBus&) { feedback.clear(); }
    );

    std::cout << "[SeedRuntime] Scene booted with " << scene.entity_count() << " entities.\n";
    std::cout << "[SeedRuntime] Press Escape to close.\n";

    while (engine.tick()) {
        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key &&
                event.button_state == seed::ButtonState::Pressed) {
                if (event.key == seed::KeyCode::Escape) {
                    engine.request_exit();
                } else if (event.key == seed::KeyCode::F5 && save_enabled) {
                    const auto result = save_service.save_slot(scene, "autosave");
                    if (result.saved) {
                        std::cout << "[SeedRuntime] Autosaved progress.\\n";
                    } else {
                        std::cout << "[SeedRuntime] Autosave failed: "
                                  << result.error << "\\n";
                    }
                } else if (event.key == seed::KeyCode::F9 && save_enabled) {
                    const auto result = save_service.load_slot(scene, "autosave");
                    if (result.loaded) {
                        std::cout << "[SeedRuntime] Loaded autosave progress.\\n";
                    } else {
                        std::cout << "[SeedRuntime] Load failed: "
                                  << result.error << "\\n";
                    }
                }
            }
        }

        engine.update();
        if (runtime_ui_available) runtime_ui.begin_frame();

        engine.begin_frame();
        if (engine.renderer() != nullptr) {
            seed::RenderSystem::render(
                scene,
                *engine.renderer(),
                gameplay_runtime.camera_entity()
            );
        }
        if (runtime_ui_available) {
            const auto hud = seed::GameplayHUDSystem::capture(scene, gameplay_runtime);
            runtime_ui.draw(hud, save_enabled);
            runtime_ui.render();
        }
        engine.end_frame();

        if (frame_limit > 0 && engine.frame_index() >= frame_limit) {
            engine.request_exit();
        }
    }

    runtime_ui.shutdown();
    engine.shutdown();
    return 0;
}
