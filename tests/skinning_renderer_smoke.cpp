#include "seed/core/Engine.h"
#include "seed/render/SkinnedShaders.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <span>

namespace {

constexpr std::string_view SkinningSmokeFragmentShader = R"GLSL(
#version 330 core
out vec4 FragColor;
void main() {
    FragColor = vec4(0.22, 0.90, 0.42, 1.0);
}
)GLSL";

} // namespace

int main() {
    seed::Engine engine({
        .application_name = "Seed Skinning Smoke",
        .window_width = 320,
        .window_height = 240,
        .window_resizable = false,
        .create_window = true,
        .renderer_backend = seed::RendererBackend::OpenGL,
        .vsync = false,
    });

    if (!engine.start()) {
        std::cerr << "Could not start Seed skinning smoke engine.\n";
        return 1;
    }

    auto* renderer = engine.renderer();
    if (renderer == nullptr) {
        engine.shutdown();
        return 2;
    }

    constexpr std::array<seed::SkinnedVertex, 3> vertices{{
        {
            .position = {-0.6f, -0.5f, 0.0f},
            .color = {1.0f, 1.0f, 1.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .joints = {0.0f, 0.0f, 0.0f, 0.0f},
            .weights = {1.0f, 0.0f, 0.0f, 0.0f},
        },
        {
            .position = {0.6f, -0.5f, 0.0f},
            .color = {1.0f, 1.0f, 1.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .joints = {0.0f, 0.0f, 0.0f, 0.0f},
            .weights = {1.0f, 0.0f, 0.0f, 0.0f},
        },
        {
            .position = {0.0f, 0.55f, 0.0f},
            .color = {1.0f, 1.0f, 1.0f},
            .normal = {0.0f, 0.0f, 1.0f},
            .joints = {1.0f, 0.0f, 0.0f, 0.0f},
            .weights = {1.0f, 0.0f, 0.0f, 0.0f},
        },
    }};
    constexpr std::array<std::uint32_t, 3> indices{{0, 1, 2}};

    const seed::MeshHandle mesh = renderer->create_skinned_mesh({
        .vertices = std::span<const seed::SkinnedVertex>{vertices},
        .indices = std::span<const std::uint32_t>{indices},
    });
    if (!mesh) {
        std::cerr << "OpenGL backend did not create a skinned mesh.\n";
        engine.shutdown();
        return 3;
    }

    const seed::ShaderHandle shader = renderer->create_shader({
        .vertex_source = seed::SeedSkinnedMeshVertexShader,
        .fragment_source = SkinningSmokeFragmentShader,
    });
    if (!shader) {
        renderer->destroy_mesh(mesh);
        engine.shutdown();
        return 4;
    }

    std::array<seed::Mat4, 2> palette{
        seed::Mat4::identity(),
        seed::translation_matrix({0.18f, 0.12f, 0.0f}),
    };

    seed::SceneLighting lighting;
    lighting.light_view_projection = seed::Mat4::identity();
    lighting.shadows_enabled = false;

    for (int frame = 0; frame < 3 && engine.tick(); ++frame) {
        // Move the child joint slightly each frame so the smoke path exercises
        // changing palette uploads rather than a one-off bind pose.
        palette[1] = seed::translation_matrix({0.10f + 0.04f * static_cast<float>(frame), 0.12f, 0.0f});

        engine.begin_frame();
        renderer->draw_skinned_mesh(
            mesh,
            shader,
            {},
            {1.0f, 1.0f, 1.0f, 1.0f},
            {},
            false,
            seed::Mat4::identity(),
            seed::Mat4::identity(),
            lighting,
            palette
        );
        engine.end_frame();
    }

    renderer->destroy_shader(shader);
    renderer->destroy_mesh(mesh);
    engine.shutdown();
    std::cout << "Seed OpenGL GPU skinning smoke passed.\n";
    return 0;
}
