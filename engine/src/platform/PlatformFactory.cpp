#include "seed/platform/PlatformFactory.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdint>
#include <deque>
#include <iostream>
#include <memory>

namespace seed {
namespace {

ButtonState translate_button_state(int action) {
    switch (action) {
    case GLFW_PRESS: return ButtonState::Pressed;
    case GLFW_REPEAT: return ButtonState::Repeated;
    case GLFW_RELEASE:
    default: return ButtonState::Released;
    }
}

std::uint8_t translate_modifiers(int mods) {
    std::uint8_t result = ModifierNone;
    if ((mods & GLFW_MOD_SHIFT) != 0) result |= ModifierShift;
    if ((mods & GLFW_MOD_CONTROL) != 0) result |= ModifierControl;
    if ((mods & GLFW_MOD_ALT) != 0) result |= ModifierAlt;
    if ((mods & GLFW_MOD_SUPER) != 0) result |= ModifierSuper;
    if ((mods & GLFW_MOD_CAPS_LOCK) != 0) result |= ModifierCapsLock;
    if ((mods & GLFW_MOD_NUM_LOCK) != 0) result |= ModifierNumLock;
    return result;
}

MouseButton translate_mouse_button(int button) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT: return MouseButton::Left;
    case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
    case GLFW_MOUSE_BUTTON_4: return MouseButton::Button4;
    case GLFW_MOUSE_BUTTON_5: return MouseButton::Button5;
    case GLFW_MOUSE_BUTTON_6: return MouseButton::Button6;
    case GLFW_MOUSE_BUTTON_7: return MouseButton::Button7;
    case GLFW_MOUSE_BUTTON_8: return MouseButton::Button8;
    default: return MouseButton::Unknown;
    }
}

KeyCode translate_key(int key) {
    switch (key) {
    case GLFW_KEY_SPACE: return KeyCode::Space;
    case GLFW_KEY_APOSTROPHE: return KeyCode::Apostrophe;
    case GLFW_KEY_COMMA: return KeyCode::Comma;
    case GLFW_KEY_MINUS: return KeyCode::Minus;
    case GLFW_KEY_PERIOD: return KeyCode::Period;
    case GLFW_KEY_SLASH: return KeyCode::Slash;
    case GLFW_KEY_0: return KeyCode::Num0;
    case GLFW_KEY_1: return KeyCode::Num1;
    case GLFW_KEY_2: return KeyCode::Num2;
    case GLFW_KEY_3: return KeyCode::Num3;
    case GLFW_KEY_4: return KeyCode::Num4;
    case GLFW_KEY_5: return KeyCode::Num5;
    case GLFW_KEY_6: return KeyCode::Num6;
    case GLFW_KEY_7: return KeyCode::Num7;
    case GLFW_KEY_8: return KeyCode::Num8;
    case GLFW_KEY_9: return KeyCode::Num9;
    case GLFW_KEY_SEMICOLON: return KeyCode::Semicolon;
    case GLFW_KEY_EQUAL: return KeyCode::Equal;
    case GLFW_KEY_A: return KeyCode::A;
    case GLFW_KEY_B: return KeyCode::B;
    case GLFW_KEY_C: return KeyCode::C;
    case GLFW_KEY_D: return KeyCode::D;
    case GLFW_KEY_E: return KeyCode::E;
    case GLFW_KEY_F: return KeyCode::F;
    case GLFW_KEY_G: return KeyCode::G;
    case GLFW_KEY_H: return KeyCode::H;
    case GLFW_KEY_I: return KeyCode::I;
    case GLFW_KEY_J: return KeyCode::J;
    case GLFW_KEY_K: return KeyCode::K;
    case GLFW_KEY_L: return KeyCode::L;
    case GLFW_KEY_M: return KeyCode::M;
    case GLFW_KEY_N: return KeyCode::N;
    case GLFW_KEY_O: return KeyCode::O;
    case GLFW_KEY_P: return KeyCode::P;
    case GLFW_KEY_Q: return KeyCode::Q;
    case GLFW_KEY_R: return KeyCode::R;
    case GLFW_KEY_S: return KeyCode::S;
    case GLFW_KEY_T: return KeyCode::T;
    case GLFW_KEY_U: return KeyCode::U;
    case GLFW_KEY_V: return KeyCode::V;
    case GLFW_KEY_W: return KeyCode::W;
    case GLFW_KEY_X: return KeyCode::X;
    case GLFW_KEY_Y: return KeyCode::Y;
    case GLFW_KEY_Z: return KeyCode::Z;
    case GLFW_KEY_LEFT_BRACKET: return KeyCode::LeftBracket;
    case GLFW_KEY_BACKSLASH: return KeyCode::Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return KeyCode::RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return KeyCode::GraveAccent;
    case GLFW_KEY_ESCAPE: return KeyCode::Escape;
    case GLFW_KEY_ENTER: return KeyCode::Enter;
    case GLFW_KEY_TAB: return KeyCode::Tab;
    case GLFW_KEY_BACKSPACE: return KeyCode::Backspace;
    case GLFW_KEY_INSERT: return KeyCode::Insert;
    case GLFW_KEY_DELETE: return KeyCode::Delete;
    case GLFW_KEY_RIGHT: return KeyCode::Right;
    case GLFW_KEY_LEFT: return KeyCode::Left;
    case GLFW_KEY_DOWN: return KeyCode::Down;
    case GLFW_KEY_UP: return KeyCode::Up;
    case GLFW_KEY_PAGE_UP: return KeyCode::PageUp;
    case GLFW_KEY_PAGE_DOWN: return KeyCode::PageDown;
    case GLFW_KEY_HOME: return KeyCode::Home;
    case GLFW_KEY_END: return KeyCode::End;
    case GLFW_KEY_CAPS_LOCK: return KeyCode::CapsLock;
    case GLFW_KEY_SCROLL_LOCK: return KeyCode::ScrollLock;
    case GLFW_KEY_NUM_LOCK: return KeyCode::NumLock;
    case GLFW_KEY_PRINT_SCREEN: return KeyCode::PrintScreen;
    case GLFW_KEY_PAUSE: return KeyCode::Pause;
    case GLFW_KEY_F1: return KeyCode::F1;
    case GLFW_KEY_F2: return KeyCode::F2;
    case GLFW_KEY_F3: return KeyCode::F3;
    case GLFW_KEY_F4: return KeyCode::F4;
    case GLFW_KEY_F5: return KeyCode::F5;
    case GLFW_KEY_F6: return KeyCode::F6;
    case GLFW_KEY_F7: return KeyCode::F7;
    case GLFW_KEY_F8: return KeyCode::F8;
    case GLFW_KEY_F9: return KeyCode::F9;
    case GLFW_KEY_F10: return KeyCode::F10;
    case GLFW_KEY_F11: return KeyCode::F11;
    case GLFW_KEY_F12: return KeyCode::F12;
    case GLFW_KEY_LEFT_SHIFT: return KeyCode::LeftShift;
    case GLFW_KEY_LEFT_CONTROL: return KeyCode::LeftControl;
    case GLFW_KEY_LEFT_ALT: return KeyCode::LeftAlt;
    case GLFW_KEY_LEFT_SUPER: return KeyCode::LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT: return KeyCode::RightShift;
    case GLFW_KEY_RIGHT_CONTROL: return KeyCode::RightControl;
    case GLFW_KEY_RIGHT_ALT: return KeyCode::RightAlt;
    case GLFW_KEY_RIGHT_SUPER: return KeyCode::RightSuper;
    case GLFW_KEY_MENU: return KeyCode::Menu;
    default: return KeyCode::Unknown;
    }
}

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
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

        switch (config.graphics_api) {
        case PlatformGraphicsApi::OpenGL:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
            break;
        case PlatformGraphicsApi::None:
        default:
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            break;
        }

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
                .width = width,
                .height = height,
            });
        });

        glfwSetWindowFocusCallback(m_window, [](GLFWwindow* window, int focused) {
            self(window).push({
                .type = PlatformEventType::WindowFocusChanged,
                .focused = focused == GLFW_TRUE,
            });
        });

        glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int, int action, int mods) {
            self(window).push({
                .type = PlatformEventType::Key,
                .key = translate_key(key),
                .button_state = translate_button_state(action),
                .modifiers = translate_modifiers(mods),
            });
        });

        glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods) {
            self(window).push({
                .type = PlatformEventType::MouseButton,
                .mouse_button = translate_mouse_button(button),
                .button_state = translate_button_state(action),
                .modifiers = translate_modifiers(mods),
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
