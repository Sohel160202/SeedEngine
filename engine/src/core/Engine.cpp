#include "seed/core/Engine.h"
#include "seed/platform/PlatformFactory.h"
#include "seed/render/RendererFactory.h"

#include <algorithm>
#include <iostream>
#include <utility>

namespace seed {

Engine::Engine(EngineConfig config)
    : m_config(std::move(config)) {
}

Engine::~Engine() {
    shutdown();
}

bool Engine::start() {
    if (m_running) {
        return true;
    }

    m_exit_requested = false;
    m_frame_index = 0;
    m_delta_seconds = 0.0;
    m_frame_events.clear();

    if (m_config.create_window) {
        m_platform = create_platform();

        PlatformConfig platform_config;
        platform_config.title = m_config.application_name;
        platform_config.width = m_config.window_width;
        platform_config.height = m_config.window_height;
        platform_config.resizable = m_config.window_resizable;
        platform_config.graphics_api =
            m_config.renderer_backend == RendererBackend::OpenGL
                ? PlatformGraphicsApi::OpenGL
                : PlatformGraphicsApi::None;

        if (!m_platform || !m_platform->initialize(platform_config)) {
            std::cerr << "[Seed] Failed to initialize platform for "
                      << m_config.application_name << '\n';
            m_platform.reset();
            return false;
        }

        if (m_config.renderer_backend != RendererBackend::None) {
            m_renderer = create_renderer(m_config.renderer_backend);

            RendererConfig renderer_config;
            renderer_config.width = m_config.window_width;
            renderer_config.height = m_config.window_height;
            renderer_config.vsync = m_config.vsync;
            renderer_config.clear_color = m_config.clear_color;

            if (!m_renderer ||
                !m_renderer->initialize(m_platform->window_handle(), renderer_config)) {
                std::cerr << "[Seed] Failed to initialize renderer for "
                          << m_config.application_name << '\n';
                if (m_renderer) {
                    m_renderer->shutdown();
                    m_renderer.reset();
                }
                m_platform->shutdown();
                m_platform.reset();
                return false;
            }
        }
    }

    m_last_tick = Clock::now();
    m_running = true;

    std::cout << "[Seed] Starting " << m_config.application_name;
    if (m_config.create_window) {
        std::cout << " (" << m_config.window_width << 'x' << m_config.window_height << ")";
        if (m_renderer) {
            std::cout << " using " << m_renderer->backend_name();
        }
    } else {
        std::cout << " (headless)";
    }
    std::cout << '\n';

    return true;
}

bool Engine::tick() {
    if (!m_running || m_exit_requested) {
        return false;
    }

    m_frame_events.clear();

    if (m_platform) {
        m_platform->pump_events();

        PlatformEvent event;
        while (m_platform->poll_event(event)) {
            m_frame_events.push_back(event);

            if (event.type == PlatformEventType::QuitRequested) {
                m_exit_requested = true;
            }

            if (event.type == PlatformEventType::WindowResized && m_renderer &&
                event.width > 0 && event.height > 0) {
                m_renderer->resize(
                    static_cast<std::uint32_t>(event.width),
                    static_cast<std::uint32_t>(event.height)
                );
            }
        }

        if (m_platform->should_close()) {
            m_exit_requested = true;
        }
    }

    const auto now = Clock::now();
    m_delta_seconds = std::chrono::duration<double>(now - m_last_tick).count();
    m_delta_seconds = std::clamp(m_delta_seconds, 0.0, 0.25);
    m_last_tick = now;
    ++m_frame_index;

    return !m_exit_requested;
}

void Engine::begin_frame() {
    if (m_renderer) {
        m_renderer->begin_frame();
    }
}

void Engine::end_frame() {
    if (m_renderer) {
        m_renderer->end_frame();
    }
}

void Engine::request_exit() noexcept {
    m_exit_requested = true;
}

void Engine::shutdown() {
    if (!m_running && !m_platform && !m_renderer) {
        return;
    }

    if (m_renderer) {
        m_renderer->shutdown();
        m_renderer.reset();
    }

    if (m_platform) {
        m_platform->shutdown();
        m_platform.reset();
    }

    if (m_running) {
        std::cout << "[Seed] Shutting down " << m_config.application_name << '\n';
    }

    m_frame_events.clear();
    m_running = false;
    m_exit_requested = false;
    m_delta_seconds = 0.0;
}

} // namespace seed
