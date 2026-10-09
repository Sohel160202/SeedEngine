#include "seed/platform/PlatformFactory.h"

#include <GLFW/glfw3.h>

#include <deque>
#include <iostream>
#include <memory>

namespace seed {
namespace {

class GlfwPlatform final : public IPlatform {
public:
    bool initialize(const PlatformConfig& config) override {
        if (m_window != nullptr) {
            return true;
        }

        glfwSetErrorCallback([](int code, const char* description) {
            std::cerr << "[SeedPlatform] GLFW error " << code << ": "
                      << (description ? description : "Unknown error") << '\n';
        });

        if (glfwInit() != GLFW_TRUE) {
            std::cerr << "[SeedPlatform] Failed to initialize platform backend.\n";
            return false;
        }

        m_glfw_initialized = true;

        // Seed owns rendering separately. Phase 1 creates a native window with no
        // graphics API context so the renderer backend can be selected independently.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

        m_window = glfwCreateWindow(
            static_cast<int>(config.width),
            static_cast<int>(config.height),
            config.title.c_str(),
            nullptr,
            nullptr
        );

        if (m_window == nullptr) {
            std::cerr << "[SeedPlatform] Failed to create window.\n";
            shutdown();
            return false;
        }

        glfwSetWindowUserPointer(m_window, this);

        glfwSetWindowCloseCallback(m_window, [](GLFWwindow* window) {
            self(window).push({.type = PlatformEventType::QuitRequested});
        });

        glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height) {
            self(window).push({
                .type = PlatformEventType::WindowResized,
                .a = width,
                .b = height,
            });
        });

        glfwSetWindowFocusCallback(m_window, [](GLFWwindow* window, int focused) {
            self(window).push({
                .type = PlatformEventType::WindowFocusChanged,
                .a = focused,
            });
        });

        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            self(window).push({
                .type = PlatformEventType::Key,
                .a = key,
                .b = action,
                .c = mods,
                .x = static_cast<float>(scancode),
            });
        });

        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
            self(window).push({
                .type = PlatformEventType::MouseButton,
                .a = button,
                .b = action,
                .c = mods,
            });
        });

        glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double x, double y) {
            self(window).push({
                .type = PlatformEventType::MouseMove,
                .x = static_cast<float>(x),
                .y = static_cast<float>(y),
            });
        });

        glfwSetScrollCallback(m_window, [](GLFWwindow* window, double x, double y) {
            self(window).push({
                .type = PlatformEventType::MouseWheel,
                .x = static_cast<float>(x),
                .y = static_cast<float>(y),
            });
        });

        return true;
    }

    void pump_events() override {
        if (m_window != nullptr) {
            glfwPollEvents();
        }
    }

    bool poll_event(PlatformEvent& event) override {
        if (m_events.empty()) {
            return false;
        }

        event = m_events.front();
        m_events.pop_front();
        return true;
    }

    bool should_close() const override {
        return m_window == nullptr || glfwWindowShouldClose(m_window) == GLFW_TRUE;
    }

    void* window_handle() const override {
        return m_window;
    }

    double time_seconds() const override {
        return m_glfw_initialized ? glfwGetTime() : 0.0;
    }

    void shutdown() override {
        m_events.clear();

        if (m_window != nullptr) {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        if (m_glfw_initialized) {
            glfwTerminate();
            m_glfw_initialized = false;
        }
    }

    ~GlfwPlatform() override {
        shutdown();
    }

private:
    static GlfwPlatform& self(GLFWwindow* window) {
        return *static_cast<GlfwPlatform*>(glfwGetWindowUserPointer(window));
    }

    void push(PlatformEvent event) {
        m_events.push_back(event);
    }

    GLFWwindow* m_window{nullptr};
    std::deque<PlatformEvent> m_events;
    bool m_glfw_initialized{false};
};

} // namespace

std::unique_ptr<IPlatform> create_platform() {
    return std::make_unique<GlfwPlatform>();
}

} // namespace seed
