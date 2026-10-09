#include "seed/render/RendererFactory.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <iostream>
#include <memory>

namespace seed {
namespace {

constexpr unsigned int GlColorBufferBit = 0x00004000;
constexpr unsigned int GlDepthBufferBit = 0x00000100;

using GlClearColorFn = void (*)(float, float, float, float);
using GlClearFn = void (*)(unsigned int);
using GlViewportFn = void (*)(int, int, int, int);

class OpenGLRenderer final : public IRenderer {
public:
    bool initialize(void* window_handle, const RendererConfig& config) override {
        if (window_handle == nullptr) {
            std::cerr << "[SeedRenderer] Cannot initialize without a window.\n";
            return false;
        }

        m_window = static_cast<GLFWwindow*>(window_handle);
        m_config = config;

        glfwMakeContextCurrent(m_window);

        m_clear_color = reinterpret_cast<GlClearColorFn>(glfwGetProcAddress("glClearColor"));
        m_clear = reinterpret_cast<GlClearFn>(glfwGetProcAddress("glClear"));
        m_viewport = reinterpret_cast<GlViewportFn>(glfwGetProcAddress("glViewport"));

        if (!m_clear_color || !m_clear || !m_viewport) {
            std::cerr << "[SeedRenderer] Failed to load required OpenGL functions.\n";
            shutdown();
            return false;
        }

        glfwSwapInterval(config.vsync ? 1 : 0);

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
        resize(
            static_cast<std::uint32_t>(framebuffer_width > 0 ? framebuffer_width : 1),
            static_cast<std::uint32_t>(framebuffer_height > 0 ? framebuffer_height : 1)
        );

        std::cout << "[SeedRenderer] OpenGL backend initialized.\n";
        return true;
    }

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.width = width;
        m_config.height = height;

        if (m_viewport) {
            m_viewport(0, 0, static_cast<int>(width), static_cast<int>(height));
        }
    }

    void begin_frame() override {
        if (!m_window || !m_clear_color || !m_clear) {
            return;
        }

        glfwMakeContextCurrent(m_window);
        m_clear_color(
            m_config.clear_color.r,
            m_config.clear_color.g,
            m_config.clear_color.b,
            m_config.clear_color.a
        );
        m_clear(GlColorBufferBit | GlDepthBufferBit);
    }

    void end_frame() override {
        if (m_window) {
            glfwSwapBuffers(m_window);
        }
    }

    std::string_view backend_name() const override {
        return "Seed OpenGL";
    }

    void shutdown() override {
        if (m_window && glfwGetCurrentContext() == m_window) {
            glfwMakeContextCurrent(nullptr);
        }

        m_window = nullptr;
        m_clear_color = nullptr;
        m_clear = nullptr;
        m_viewport = nullptr;
    }

    ~OpenGLRenderer() override {
        shutdown();
    }

private:
    GLFWwindow* m_window{nullptr};
    RendererConfig m_config{};
    GlClearColorFn m_clear_color{nullptr};
    GlClearFn m_clear{nullptr};
    GlViewportFn m_viewport{nullptr};
};

} // namespace

std::unique_ptr<IRenderer> create_renderer(RendererBackend backend) {
    switch (backend) {
    case RendererBackend::OpenGL:
        return std::make_unique<OpenGLRenderer>();
    case RendererBackend::None:
    default:
        return nullptr;
    }
}

} // namespace seed
