#pragma once

#include <cstdint>
#include <string>

namespace seed {

enum class PlatformEventType {
    None,
    QuitRequested,
    WindowResized,
    WindowFocusChanged,
    Key,
    MouseButton,
    MouseMove,
    MouseWheel,
    Gamepad
};

struct PlatformEvent {
    PlatformEventType type{PlatformEventType::None};
    std::int32_t a{0};
    std::int32_t b{0};
    float x{0.0f};
    float y{0.0f};
};

struct PlatformConfig {
    std::string title{"Seed"};
    std::uint32_t width{1280};
    std::uint32_t height{720};
    bool resizable{true};
};

class IPlatform {
public:
    virtual ~IPlatform() = default;

    virtual bool initialize(const PlatformConfig& config) = 0;
    virtual bool poll_event(PlatformEvent& event) = 0;
    virtual void* native_window_handle() const = 0;
    virtual double time_seconds() const = 0;
    virtual void shutdown() = 0;
};

} // namespace seed
