#pragma once

#include <cstdint>
#include <string_view>

namespace seed {

enum class RendererBackend {
    None,
    OpenGL
};

struct ClearColor {
    float r{0.055f};
    float g{0.075f};
    float b{0.060f};
    float a{1.0f};
};

struct RendererConfig {
    std::uint32_t width{1280};
    std::uint32_t height{720};
    bool vsync{true};
    ClearColor clear_color{};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(void* window_handle, const RendererConfig& config) = 0;
    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual std::string_view backend_name() const = 0;
    virtual void shutdown() = 0;
};

} // namespace seed
