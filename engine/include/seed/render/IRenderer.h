#pragma once

#include <cstdint>
#include <string_view>

namespace seed {

struct RendererConfig {
    std::uint32_t width{1280};
    std::uint32_t height{720};
    bool vsync{true};
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool initialize(void* native_window_handle, const RendererConfig& config) = 0;
    virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void begin_frame() = 0;
    virtual void end_frame() = 0;
    virtual std::string_view backend_name() const = 0;
    virtual void shutdown() = 0;
};

} // namespace seed
