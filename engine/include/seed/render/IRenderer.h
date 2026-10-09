#pragma once

#include "seed/math/Math.h"
#include "seed/render/RenderResources.h"

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

    virtual std::uint32_t width() const noexcept = 0;
    virtual std::uint32_t height() const noexcept = 0;

    virtual ShaderHandle create_shader(const ShaderDesc& desc) = 0;
    virtual void destroy_shader(ShaderHandle shader) = 0;

    virtual MeshHandle create_mesh(const MeshDesc& desc) = 0;
    virtual void destroy_mesh(MeshHandle mesh) = 0;

    virtual TextureHandle create_texture(const TextureDesc& desc) = 0;
    virtual void destroy_texture(TextureHandle texture) = 0;

    virtual void begin_frame() = 0;
    virtual void draw_mesh(
        MeshHandle mesh,
        ShaderHandle shader,
        TextureHandle texture,
        const Vec4& base_color,
        const Mat4& model,
        const Mat4& view_projection
    ) = 0;
    virtual void end_frame() = 0;

    virtual std::string_view backend_name() const = 0;
    virtual void shutdown() = 0;
};

} // namespace seed
