#include "StudioUI.h"

#include "seed/core/Engine.h"
#include "seed/gameplay/Components.h"
#include "seed/render/RenderComponents.h"
#include "seed/render/RenderSystem.h"

#include <algorithm>
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

constexpr std::string_view SeedMeshVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

uniform mat4 uModel;
uniform mat4 uViewProjection;

out vec3 vColor;

void main() {
    gl_Position = uViewProjection * uModel * vec4(aPosition, 1.0);
    vColor = aColor;
}
)GLSL";

constexpr std::string_view SeedMeshFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)GLSL";

struct CameraInputState {
    bool forward{false};
    bool backward{false};
    bool left{false};
    bool right{false};
    bool down{false};
    bool up{false};
    bool fast{false};
    bool looking{false};
    bool has_mouse_position{false};
    float last_mouse_x{0.0f};
    float last_mouse_y{0.0f};
    float move_speed{3.5f};
};

void set_key_state(CameraInputState& input, seed::KeyCode key, bool down) {
    switch (key) {
    case seed::KeyCode::W: input.forward = down; break;
    case seed::KeyCode::S: input.backward = down; break;
    case seed::KeyCode::A: input.left = down; break;
    case seed::KeyCode::D: input.right = down; break;
    case seed::KeyCode::Q: input.down = down; break;
    case seed::KeyCode::E: input.up = down; break;
    case seed::KeyCode::LeftShift:
    case seed::KeyCode::RightShift: input.fast = down; break;
    default: break;
    }
}

void clear_movement_input(CameraInputState& input) {
    input.forward = false;
    input.backward = false;
    input.left = false;
    input.right = false;
    input.down = false;
    input.up = false;
    input.fast = false;
}

void update_camera_movement(
    seed::TransformComponent& transform,
    const CameraInputState& input,
    double delta_seconds
) {
    const float multiplier = input.fast ? 3.0f : 1.0f;
    const float distance = input.move_speed * multiplier * static_cast<float>(delta_seconds);
    const seed::Vec3 forward = seed::forward_from_euler(transform.rotation_degrees);
    const seed::Vec3 right = seed::right_from_euler(transform.rotation_degrees);
    const seed::Vec3 world_up{0.0f, 1.0f, 0.0f};

    if (input.forward) transform.position += forward * distance;
    if (input.backward) transform.position += forward * -distance;
    if (input.right) transform.position += right * distance;
    if (input.left) transform.position += right * -distance;
    if (input.up) transform.position += world_up * distance;
    if (input.down) transform.position += world_up * -distance;
}

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

    auto* renderer = engine.renderer();
    if (renderer == nullptr || engine.platform() == nullptr) {
        std::cerr << "[SeedStudio] Renderer or platform unavailable.\n";
        engine.shutdown();
        return 2;
    }

    const auto mesh_shader = renderer->create_shader({
        .vertex_source = SeedMeshVertexShader,
        .fragment_source = SeedMeshFragmentShader,
    });

    constexpr std::array<seed::VertexPositionColor, 8> cube_vertices{{
        {.position = {-0.75f, -0.75f, -0.75f}, .color = {0.20f, 0.95f, 0.46f}},
        {.position = { 0.75f, -0.75f, -0.75f}, .color = {0.18f, 0.58f, 1.00f}},
        {.position = { 0.75f,  0.75f, -0.75f}, .color = {1.00f, 0.78f, 0.18f}},
        {.position = {-0.75f,  0.75f, -0.75f}, .color = {0.72f, 0.35f, 1.00f}},
        {.position = {-0.75f, -0.75f,  0.75f}, .color = {0.10f, 0.80f, 0.72f}},
        {.position = { 0.75f, -0.75f,  0.75f}, .color = {1.00f, 0.35f, 0.28f}},
        {.position = { 0.75f,  0.75f,  0.75f}, .color = {0.95f, 0.90f, 0.34f}},
        {.position = {-0.75f,  0.75f,  0.75f}, .color = {0.28f, 0.78f, 1.00f}},
    }};

    constexpr std::array<std::uint32_t, 36> cube_indices{{
        0, 1, 2, 2, 3, 0,
        4, 6, 5, 6, 4, 7,
        0, 4, 5, 5, 1, 0,
        3, 2, 6, 6, 7, 3,
        1, 5, 6, 6, 2, 1,
        0, 3, 7, 7, 4, 0,
    }};

    const auto cube_mesh = renderer->create_mesh({
        .vertices = std::span<const seed::VertexPositionColor>{cube_vertices},
        .indices = std::span<const std::uint32_t>{cube_indices},
    });

    if (!mesh_shader || !cube_mesh) {
        std::cerr << "[SeedStudio] Failed to create Seed 3D resources.\n";
        if (cube_mesh) renderer->destroy_mesh(cube_mesh);
        if (mesh_shader) renderer->destroy_shader(mesh_shader);
        engine.shutdown();
        return 3;
    }

    auto& scene = engine.scene();

    const auto camera_entity = scene.create_entity("Editor Camera");
    auto& camera_transform = scene.add_component<seed::TransformComponent>(camera_entity);
    camera_transform.position = {0.0f, 0.0f, 4.2f};
    camera_transform.rotation_degrees = {-8.0f, 0.0f, 0.0f};
    scene.add_component<seed::CameraComponent>(camera_entity);

    const auto cube_entity = scene.create_entity("Seed Cube");
    auto& cube_transform = scene.add_component<seed::TransformComponent>(cube_entity);
    cube_transform.position = {0.0f, 0.0f, 0.0f};
    cube_transform.rotation_degrees = {20.0f, 32.0f, 0.0f};
    scene.add_component<seed::MeshComponent>(cube_entity, cube_mesh, true);
    scene.add_component<seed::MaterialComponent>(cube_entity, mesh_shader);

    seed::studio::StudioUI studio_ui;
    if (!studio_ui.initialize(engine.platform()->window_handle())) {
        std::cerr << "[SeedStudio] Failed to initialize the Seed Studio UI layer.\n";
        renderer->destroy_mesh(cube_mesh);
        renderer->destroy_shader(mesh_shader);
        engine.shutdown();
        return 4;
    }
    studio_ui.select_entity(cube_entity);

    CameraInputState camera_input{};

    std::cout << "[SeedStudio] Native editor window active.\n";
    std::cout << "[SeedStudio] Seed Studio v0 shell active: World + Viewport + Inspector + Assets.\n";
    std::cout << "[SeedStudio] Select entities and edit Transform values live.\n";

    while (engine.tick()) {
        studio_ui.begin_frame();

        const bool ui_wants_mouse = studio_ui.wants_mouse();
        const bool ui_wants_keyboard = studio_ui.wants_keyboard();

        for (const auto& event : engine.frame_events()) {
            if (event.type == seed::PlatformEventType::Key) {
                if (event.key == seed::KeyCode::Escape &&
                    event.button_state == seed::ButtonState::Pressed) {
                    engine.request_exit();
                }

                if (!ui_wants_keyboard) {
                    const bool key_down = event.button_state != seed::ButtonState::Released;
                    set_key_state(camera_input, event.key, key_down);
                }
            }

            if (event.type == seed::PlatformEventType::MouseButton &&
                event.mouse_button == seed::MouseButton::Right &&
                !ui_wants_mouse) {
                camera_input.looking = event.button_state != seed::ButtonState::Released;
                camera_input.has_mouse_position = false;
            }

            if (event.type == seed::PlatformEventType::MouseMove &&
                camera_input.looking && !ui_wants_mouse) {
                if (camera_input.has_mouse_position) {
                    constexpr float look_sensitivity = 0.12f;
                    const float delta_x = event.x - camera_input.last_mouse_x;
                    const float delta_y = event.y - camera_input.last_mouse_y;
                    camera_transform.rotation_degrees.y += delta_x * look_sensitivity;
                    camera_transform.rotation_degrees.x -= delta_y * look_sensitivity;
                    camera_transform.rotation_degrees.x = std::clamp(
                        camera_transform.rotation_degrees.x,
                        -89.0f,
                        89.0f
                    );
                }
                camera_input.last_mouse_x = event.x;
                camera_input.last_mouse_y = event.y;
                camera_input.has_mouse_position = true;
            }

            if (event.type == seed::PlatformEventType::MouseWheel && !ui_wants_mouse) {
                camera_input.move_speed = std::clamp(
                    camera_input.move_speed + event.y * 0.45f,
                    0.5f,
                    20.0f
                );
            }

            if (event.type == seed::PlatformEventType::WindowFocusChanged && !event.focused) {
                clear_movement_input(camera_input);
                camera_input.looking = false;
            }
        }

        if (ui_wants_keyboard) {
            clear_movement_input(camera_input);
        }
        update_camera_movement(camera_transform, camera_input, engine.delta_seconds());

        // Editor widgets mutate Seed Scene data directly. The renderer sees the
        // updated components later in this same frame.
        studio_ui.draw(scene);

        engine.begin_frame();
        seed::RenderSystem::render(scene, *renderer);
        studio_ui.render();
        engine.end_frame();

        if (frame_limit > 0 && engine.frame_index() >= frame_limit) {
            engine.request_exit();
        }
    }

    studio_ui.shutdown();
    renderer->destroy_mesh(cube_mesh);
    renderer->destroy_shader(mesh_shader);

    engine.shutdown();
    return 0;
}
