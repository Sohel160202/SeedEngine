#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <iostream>
#include <span>
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

constexpr std::string_view SeedTriangleVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

out vec3 vColor;

void main() {
    gl_Position = vec4(aPosition, 1.0);
    vColor = aColor;
}
)GLSL";

constexpr std::string_view SeedTriangleFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)GLSL";

} // namespace

int main(int argc, char** argv) {
    const std::uint64_t frame_limit = parse_frame_limit(argc, argv);

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

    auto* renderer = engine.renderer();
    if (renderer == nullptr) {
        std::cerr << "[SeedStudio] Renderer unavailable.\n";
        engine.shutdown();
        return 2;
    }

    const auto triangle_shader = renderer->create_shader({
        .vertex_source = SeedTriangleVertexShader,
        .fragment_source = SeedTriangleFragmentShader,
    });

    constexpr std::array<seed::VertexPositionColor, 3> triangle_vertices{{
        {.position = {-0.62f, -0.48f, 0.0f}, .color = {0.20f, 0.95f, 0.46f}},
        {.position = { 0.62f, -0.48f, 0.0f}, .color = {0.18f, 0.58f, 1.00f}},
        {.position = { 0.00f,  0.62f, 0.0f}, .color = {1.00f, 0.78f, 0.18f}},
    }};

    constexpr std::array<std::uint32_t, 3> triangle_indices{{0, 1, 2}};

    const auto triangle_mesh = renderer->create_mesh({
        .vertices = std::span<const seed::VertexPositionColor>{triangle_vertices},
        .indices = std::span<const std::uint32_t>{triangle_indices},
    });

    if (!triangle_shader || !triangle_mesh) {
        std::cerr << "[SeedStudio] Failed to create the first Seed triangle resources.\n";
        if (triangle_mesh) renderer->destroy_mesh(triangle_mesh);
        if (triangle_shader) renderer->destroy_shader(triangle_shader);
        engine.shutdown();
        return 3;
    }

    std::cout << "[SeedStudio] Native editor window active.\n";
    std::cout << "[SeedStudio] First Seed triangle ready. Press Escape to close.\n";

    while (engine.tick()) {
        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key &&
                event.key == seed::KeyCode::Escape &&
                event.button_state == seed::ButtonState::Pressed) {
                engine.request_exit();
            }
        }

        engine.begin_frame();
        renderer->draw_mesh(triangle_mesh, triangle_shader);
        engine.end_frame();

        if (frame_limit > 0 && engine.frame_index() >= frame_limit) {
            engine.request_exit();
        }
    }

    renderer->destroy_mesh(triangle_mesh);
    renderer->destroy_shader(triangle_shader);

    engine.shutdown();
    return 0;
}
