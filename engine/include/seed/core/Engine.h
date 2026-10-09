#pragma once

#include "seed/platform/IPlatform.h"
#include "seed/render/IRenderer.h"
#include "seed/scene/Scene.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace seed {

struct EngineConfig {
    std::string application_name{"Seed Application"};
    std::uint32_t window_width{1280};
    std::uint32_t window_height{720};
    bool window_resizable{true};
    bool create_window{true};
    RendererBackend renderer_backend{RendererBackend::OpenGL};
    bool vsync{true};
    ClearColor clear_color{};
};

class Engine {
public:
    explicit Engine(EngineConfig config = {});
    ~Engine();

    bool start();
    bool tick();
    void begin_frame();
    void end_frame();
    void request_exit() noexcept;
    void shutdown();

    bool is_running() const noexcept { return m_running; }
    bool exit_requested() const noexcept { return m_exit_requested; }
    const EngineConfig& config() const noexcept { return m_config; }

    double delta_seconds() const noexcept { return m_delta_seconds; }
    std::uint64_t frame_index() const noexcept { return m_frame_index; }

    const std::vector<PlatformEvent>& frame_events() const noexcept { return m_frame_events; }

    IPlatform* platform() noexcept { return m_platform.get(); }
    const IPlatform* platform() const noexcept { return m_platform.get(); }

    IRenderer* renderer() noexcept { return m_renderer.get(); }
    const IRenderer* renderer() const noexcept { return m_renderer.get(); }

    Scene& scene() noexcept { return m_scene; }
    const Scene& scene() const noexcept { return m_scene; }

private:
    using Clock = std::chrono::steady_clock;

    EngineConfig m_config;
    Scene m_scene;
    std::unique_ptr<IPlatform> m_platform;
    std::unique_ptr<IRenderer> m_renderer;
    std::vector<PlatformEvent> m_frame_events;
    Clock::time_point m_last_tick{};
    double m_delta_seconds{0.0};
    std::uint64_t m_frame_index{0};
    bool m_running{false};
    bool m_exit_requested{false};
};

} // namespace seed
