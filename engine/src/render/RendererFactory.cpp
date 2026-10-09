#include "seed/render/RendererFactory.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace seed {
namespace {

constexpr unsigned int GlColorBufferBit = 0x00004000;
constexpr unsigned int GlDepthBufferBit = 0x00000100;
constexpr unsigned int GlDepthTest = 0x0B71;
constexpr unsigned int GlArrayBuffer = 0x8892;
constexpr unsigned int GlElementArrayBuffer = 0x8893;
constexpr unsigned int GlStaticDraw = 0x88E4;
constexpr unsigned int GlFloat = 0x1406;
constexpr unsigned int GlUnsignedInt = 0x1405;
constexpr unsigned int GlUnsignedByte = 0x1401;
constexpr unsigned int GlTriangles = 0x0004;
constexpr unsigned int GlVertexShader = 0x8B31;
constexpr unsigned int GlFragmentShader = 0x8B30;
constexpr unsigned int GlCompileStatus = 0x8B81;
constexpr unsigned int GlLinkStatus = 0x8B82;
constexpr unsigned int GlInfoLogLength = 0x8B84;
constexpr unsigned int GlTexture2D = 0x0DE1;
constexpr unsigned int GlTexture0 = 0x84C0;
constexpr unsigned int GlTexture1 = 0x84C1;
constexpr unsigned int GlTexture2 = 0x84C2;
constexpr unsigned int GlTexture3 = 0x84C3;
constexpr unsigned int GlRgba = 0x1908;
constexpr unsigned int GlDepthComponent = 0x1902;
constexpr unsigned int GlDepthComponent24 = 0x81A6;
constexpr unsigned int GlTextureMinFilter = 0x2801;
constexpr unsigned int GlTextureMagFilter = 0x2800;
constexpr unsigned int GlTextureWrapS = 0x2802;
constexpr unsigned int GlTextureWrapT = 0x2803;
constexpr unsigned int GlTextureBorderColor = 0x1004;
constexpr unsigned int GlFramebuffer = 0x8D40;
constexpr unsigned int GlDepthAttachment = 0x8D00;
constexpr unsigned int GlFramebufferComplete = 0x8CD5;
constexpr unsigned int GlNone = 0;
constexpr int GlLinear = 0x2601;
constexpr int GlNearest = 0x2600;
constexpr int GlRepeat = 0x2901;
constexpr int GlClampToBorder = 0x812D;
constexpr unsigned char GlFalse = 0;
constexpr std::uint32_t ShadowMapSize = 2048;

using GlClearColorFn = void (*)(float, float, float, float);
using GlClearFn = void (*)(unsigned int);
using GlViewportFn = void (*)(int, int, int, int);
using GlEnableFn = void (*)(unsigned int);
using GlDisableFn = void (*)(unsigned int);
using GlGenVertexArraysFn = void (*)(int, unsigned int*);
using GlBindVertexArrayFn = void (*)(unsigned int);
using GlDeleteVertexArraysFn = void (*)(int, const unsigned int*);
using GlGenBuffersFn = void (*)(int, unsigned int*);
using GlBindBufferFn = void (*)(unsigned int, unsigned int);
using GlBufferDataFn = void (*)(unsigned int, std::ptrdiff_t, const void*, unsigned int);
using GlDeleteBuffersFn = void (*)(int, const unsigned int*);
using GlEnableVertexAttribArrayFn = void (*)(unsigned int);
using GlVertexAttribPointerFn = void (*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
using GlCreateShaderFn = unsigned int (*)(unsigned int);
using GlShaderSourceFn = void (*)(unsigned int, int, const char* const*, const int*);
using GlCompileShaderFn = void (*)(unsigned int);
using GlGetShaderivFn = void (*)(unsigned int, unsigned int, int*);
using GlGetShaderInfoLogFn = void (*)(unsigned int, int, int*, char*);
using GlDeleteShaderFn = void (*)(unsigned int);
using GlCreateProgramFn = unsigned int (*)();
using GlAttachShaderFn = void (*)(unsigned int, unsigned int);
using GlLinkProgramFn = void (*)(unsigned int);
using GlGetProgramivFn = void (*)(unsigned int, unsigned int, int*);
using GlGetProgramInfoLogFn = void (*)(unsigned int, int, int*, char*);
using GlDeleteProgramFn = void (*)(unsigned int);
using GlUseProgramFn = void (*)(unsigned int);
using GlGetUniformLocationFn = int (*)(unsigned int, const char*);
using GlUniformMatrix4fvFn = void (*)(int, int, unsigned char, const float*);
using GlUniform1iFn = void (*)(int, int);
using GlUniform1fFn = void (*)(int, float);
using GlUniform3fFn = void (*)(int, float, float, float);
using GlUniform4fFn = void (*)(int, float, float, float, float);
using GlDrawElementsFn = void (*)(unsigned int, int, unsigned int, const void*);
using GlDrawArraysFn = void (*)(unsigned int, int, int);
using GlGenTexturesFn = void (*)(int, unsigned int*);
using GlBindTextureFn = void (*)(unsigned int, unsigned int);
using GlTexImage2DFn = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
using GlTexParameteriFn = void (*)(unsigned int, unsigned int, int);
using GlTexParameterfvFn = void (*)(unsigned int, unsigned int, const float*);
using GlDeleteTexturesFn = void (*)(int, const unsigned int*);
using GlActiveTextureFn = void (*)(unsigned int);
using GlGenFramebuffersFn = void (*)(int, unsigned int*);
using GlBindFramebufferFn = void (*)(unsigned int, unsigned int);
using GlFramebufferTexture2DFn = void (*)(unsigned int, unsigned int, unsigned int, unsigned int, int);
using GlCheckFramebufferStatusFn = unsigned int (*)(unsigned int);
using GlDeleteFramebuffersFn = void (*)(int, const unsigned int*);
using GlDrawBufferFn = void (*)(unsigned int);
using GlReadBufferFn = void (*)(unsigned int);

struct OpenGLShaderResource {
    unsigned int program{0};
    int model_location{-1};
    int view_projection_location{-1};
    int base_color_location{-1};
    int texture_location{-1};
    int use_texture_location{-1};
    int metallic_roughness_texture_location{-1};
    int use_metallic_roughness_texture_location{-1};
    int normal_texture_location{-1};
    int use_normal_texture_location{-1};
    int metallic_location{-1};
    int roughness_location{-1};
    int normal_scale_location{-1};
    int camera_position_location{-1};
    int directional_direction_location{-1};
    int directional_color_location{-1};
    int directional_intensity_location{-1};
    int ambient_color_location{-1};
    int ambient_intensity_location{-1};
    int light_view_projection_location{-1};
    int shadow_map_location{-1};
    int receive_shadows_location{-1};
    int shadows_enabled_location{-1};
};

struct OpenGLMeshResource {
    unsigned int vertex_array{0};
    unsigned int vertex_buffer{0};
    unsigned int index_buffer{0};
    int index_count{0};
};

struct OpenGLTextureResource {
    unsigned int texture{0};
};

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

        if (!load_functions()) {
            std::cerr << "[SeedRenderer] Failed to load required OpenGL functions.\n";
            shutdown();
            return false;
        }

        glfwSwapInterval(config.vsync ? 1 : 0);
        m_enable(GlDepthTest);

        if (!create_sky_resources() || !create_shadow_resources()) {
            std::cerr << "[SeedRenderer] Failed to create Seed sky/shadow resources.\n";
            shutdown();
            return false;
        }

        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(m_window, &framebuffer_width, &framebuffer_height);
        resize(
            static_cast<std::uint32_t>(framebuffer_width > 0 ? framebuffer_width : 1),
            static_cast<std::uint32_t>(framebuffer_height > 0 ? framebuffer_height : 1)
        );

        std::cout << "[SeedRenderer] OpenGL backend initialized with PBR maps, sky + directional shadows.\n";
        return true;
    }

    void resize(std::uint32_t width, std::uint32_t height) override {
        m_config.width = width > 0 ? width : 1;
        m_config.height = height > 0 ? height : 1;
        if (m_viewport && !m_shadow_pass_active) {
            m_viewport(0, 0, static_cast<int>(m_config.width), static_cast<int>(m_config.height));
        }
    }

    std::uint32_t width() const noexcept override { return m_config.width; }
    std::uint32_t height() const noexcept override { return m_config.height; }

    ShaderHandle create_shader(const ShaderDesc& desc) override {
        if (!m_window || desc.vertex_source.empty() || desc.fragment_source.empty()) {
            return {};
        }
        glfwMakeContextCurrent(m_window);
        const unsigned int program = link_program(desc.vertex_source, desc.fragment_source, "Seed material");
        if (program == 0) {
            return {};
        }

        OpenGLShaderResource resource{};
        resource.program = program;
        resource.model_location = m_get_uniform_location(program, "uModel");
        resource.view_projection_location = m_get_uniform_location(program, "uViewProjection");
        resource.base_color_location = m_get_uniform_location(program, "uBaseColor");
        resource.texture_location = m_get_uniform_location(program, "uBaseTexture");
        resource.use_texture_location = m_get_uniform_location(program, "uUseTexture");
        resource.metallic_roughness_texture_location = m_get_uniform_location(program, "uMetallicRoughnessTexture");
        resource.use_metallic_roughness_texture_location = m_get_uniform_location(program, "uUseMetallicRoughnessTexture");
        resource.normal_texture_location = m_get_uniform_location(program, "uNormalTexture");
        resource.use_normal_texture_location = m_get_uniform_location(program, "uUseNormalTexture");
        resource.metallic_location = m_get_uniform_location(program, "uMetallic");
        resource.roughness_location = m_get_uniform_location(program, "uRoughness");
        resource.normal_scale_location = m_get_uniform_location(program, "uNormalScale");
        resource.camera_position_location = m_get_uniform_location(program, "uCameraPosition");
        resource.directional_direction_location = m_get_uniform_location(program, "uDirectionalDirection");
        resource.directional_color_location = m_get_uniform_location(program, "uDirectionalColor");
        resource.directional_intensity_location = m_get_uniform_location(program, "uDirectionalIntensity");
        resource.ambient_color_location = m_get_uniform_location(program, "uAmbientColor");
        resource.ambient_intensity_location = m_get_uniform_location(program, "uAmbientIntensity");
        resource.light_view_projection_location = m_get_uniform_location(program, "uLightViewProjection");
        resource.shadow_map_location = m_get_uniform_location(program, "uShadowMap");
        resource.receive_shadows_location = m_get_uniform_location(program, "uReceiveShadows");
        resource.shadows_enabled_location = m_get_uniform_location(program, "uShadowsEnabled");

        const ShaderHandle handle{m_next_shader_id++};
        m_shaders.emplace(handle.value, resource);
        return handle;
    }

    void destroy_shader(ShaderHandle shader) override {
        const auto found = m_shaders.find(shader.value);
        if (found == m_shaders.end()) return;
        if (m_window) glfwMakeContextCurrent(m_window);
        if (found->second.program != 0 && m_delete_program) m_delete_program(found->second.program);
        m_shaders.erase(found);
    }

    MeshHandle create_mesh(const MeshDesc& desc) override {
        if (!m_window || desc.vertices.empty() || desc.indices.empty()) return {};
        glfwMakeContextCurrent(m_window);

        OpenGLMeshResource resource{};
        m_gen_vertex_arrays(1, &resource.vertex_array);
        m_gen_buffers(1, &resource.vertex_buffer);
        m_gen_buffers(1, &resource.index_buffer);
        resource.index_count = static_cast<int>(desc.indices.size());

        m_bind_vertex_array(resource.vertex_array);
        m_bind_buffer(GlArrayBuffer, resource.vertex_buffer);
        m_buffer_data(GlArrayBuffer, static_cast<std::ptrdiff_t>(desc.vertices.size_bytes()), desc.vertices.data(), GlStaticDraw);
        m_bind_buffer(GlElementArrayBuffer, resource.index_buffer);
        m_buffer_data(GlElementArrayBuffer, static_cast<std::ptrdiff_t>(desc.indices.size_bytes()), desc.indices.data(), GlStaticDraw);

        enable_vertex_attribute(0, 3, offsetof(VertexPositionColor, position));
        enable_vertex_attribute(1, 3, offsetof(VertexPositionColor, color));
        enable_vertex_attribute(2, 2, offsetof(VertexPositionColor, texcoord));
        enable_vertex_attribute(3, 3, offsetof(VertexPositionColor, normal));

        m_bind_vertex_array(0);
        m_bind_buffer(GlArrayBuffer, 0);

        const MeshHandle handle{m_next_mesh_id++};
        m_meshes.emplace(handle.value, resource);
        return handle;
    }

    void destroy_mesh(MeshHandle mesh) override {
        const auto found = m_meshes.find(mesh.value);
        if (found == m_meshes.end()) return;
        if (m_window) glfwMakeContextCurrent(m_window);
        destroy_mesh_resource(found->second);
        m_meshes.erase(found);
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        const std::size_t expected = static_cast<std::size_t>(desc.width) * static_cast<std::size_t>(desc.height) * 4u;
        if (!m_window || desc.width == 0 || desc.height == 0 || desc.rgba8_pixels.size() < expected) return {};
        glfwMakeContextCurrent(m_window);

        OpenGLTextureResource resource{};
        m_gen_textures(1, &resource.texture);
        m_bind_texture(GlTexture2D, resource.texture);
        m_tex_parameter_i(GlTexture2D, GlTextureMinFilter, GlLinear);
        m_tex_parameter_i(GlTexture2D, GlTextureMagFilter, GlLinear);
        m_tex_parameter_i(GlTexture2D, GlTextureWrapS, GlRepeat);
        m_tex_parameter_i(GlTexture2D, GlTextureWrapT, GlRepeat);
        m_tex_image_2d(GlTexture2D, 0, static_cast<int>(GlRgba), static_cast<int>(desc.width), static_cast<int>(desc.height), 0, GlRgba, GlUnsignedByte, desc.rgba8_pixels.data());
        m_bind_texture(GlTexture2D, 0);

        const TextureHandle handle{m_next_texture_id++};
        m_textures.emplace(handle.value, resource);
        return handle;
    }

    void destroy_texture(TextureHandle texture) override {
        const auto found = m_textures.find(texture.value);
        if (found == m_textures.end()) return;
        if (m_window) glfwMakeContextCurrent(m_window);
        if (found->second.texture != 0 && m_delete_textures) m_delete_textures(1, &found->second.texture);
        m_textures.erase(found);
    }

    void begin_frame() override {
        if (!m_window || !m_clear_color || !m_clear) return;
        glfwMakeContextCurrent(m_window);
        m_bind_framebuffer(GlFramebuffer, 0);
        m_viewport(0, 0, static_cast<int>(m_config.width), static_cast<int>(m_config.height));
        m_enable(GlDepthTest);
        m_clear_color(m_config.clear_color.r, m_config.clear_color.g, m_config.clear_color.b, m_config.clear_color.a);
        m_clear(GlColorBufferBit | GlDepthBufferBit);
    }

    void draw_sky(const SkySettings& sky) override {
        if (!sky.enabled || m_sky_program == 0 || m_sky_vertex_array == 0) return;

        m_disable(GlDepthTest);
        m_use_program(m_sky_program);
        if (m_sky_zenith_location >= 0) m_uniform3f(m_sky_zenith_location, sky.zenith_color.x, sky.zenith_color.y, sky.zenith_color.z);
        if (m_sky_horizon_location >= 0) m_uniform3f(m_sky_horizon_location, sky.horizon_color.x, sky.horizon_color.y, sky.horizon_color.z);
        if (m_sky_intensity_location >= 0) m_uniform1f(m_sky_intensity_location, sky.intensity);
        m_bind_vertex_array(m_sky_vertex_array);
        m_draw_arrays(GlTriangles, 0, 3);
        m_bind_vertex_array(0);
        m_use_program(0);
        m_enable(GlDepthTest);
    }

    bool begin_shadow_pass(const Mat4& light_view_projection) override {
        if (m_shadow_framebuffer == 0 || m_shadow_depth_texture == 0 || m_shadow_program == 0) return false;
        m_shadow_pass_active = true;
        m_bind_framebuffer(GlFramebuffer, m_shadow_framebuffer);
        m_viewport(0, 0, static_cast<int>(ShadowMapSize), static_cast<int>(ShadowMapSize));
        m_clear(GlDepthBufferBit);
        m_use_program(m_shadow_program);
        if (m_shadow_light_view_projection_location >= 0) {
            m_uniform_matrix4fv(m_shadow_light_view_projection_location, 1, GlFalse, light_view_projection.data());
        }
        return true;
    }

    void draw_shadow_mesh(MeshHandle mesh, const Mat4& model) override {
        if (!m_shadow_pass_active) return;
        const auto found = m_meshes.find(mesh.value);
        if (found == m_meshes.end()) return;
        if (m_shadow_model_location >= 0) m_uniform_matrix4fv(m_shadow_model_location, 1, GlFalse, model.data());
        m_bind_vertex_array(found->second.vertex_array);
        m_draw_elements(GlTriangles, found->second.index_count, GlUnsignedInt, nullptr);
        m_bind_vertex_array(0);
    }

    void end_shadow_pass() override {
        if (!m_shadow_pass_active) return;
        m_use_program(0);
        m_bind_framebuffer(GlFramebuffer, 0);
        m_viewport(0, 0, static_cast<int>(m_config.width), static_cast<int>(m_config.height));
        m_shadow_pass_active = false;
    }

    void draw_mesh(
        MeshHandle mesh,
        ShaderHandle shader,
        const MaterialTextures& textures,
        const Vec4& base_color,
        const MaterialSurface& surface,
        bool receive_shadows,
        const Mat4& model,
        const Mat4& view_projection,
        const SceneLighting& lighting
    ) override {
        const auto mesh_found = m_meshes.find(mesh.value);
        const auto shader_found = m_shaders.find(shader.value);
        if (mesh_found == m_meshes.end() || shader_found == m_shaders.end()) return;

        const auto& resource = shader_found->second;
        m_use_program(resource.program);
        set_matrix(resource.model_location, model);
        set_matrix(resource.view_projection_location, view_projection);
        set_matrix(resource.light_view_projection_location, lighting.light_view_projection);

        if (resource.base_color_location >= 0) m_uniform4f(resource.base_color_location, base_color.x, base_color.y, base_color.z, base_color.w);
        if (resource.metallic_location >= 0) m_uniform1f(resource.metallic_location, surface.metallic);
        if (resource.roughness_location >= 0) m_uniform1f(resource.roughness_location, surface.roughness);
        if (resource.normal_scale_location >= 0) m_uniform1f(resource.normal_scale_location, surface.normal_scale);
        if (resource.camera_position_location >= 0) m_uniform3f(resource.camera_position_location, lighting.camera_position.x, lighting.camera_position.y, lighting.camera_position.z);
        if (resource.directional_direction_location >= 0) m_uniform3f(resource.directional_direction_location, lighting.directional_direction.x, lighting.directional_direction.y, lighting.directional_direction.z);
        if (resource.directional_color_location >= 0) m_uniform3f(resource.directional_color_location, lighting.directional_color.x, lighting.directional_color.y, lighting.directional_color.z);
        if (resource.directional_intensity_location >= 0) m_uniform1f(resource.directional_intensity_location, lighting.directional_intensity);
        if (resource.ambient_color_location >= 0) m_uniform3f(resource.ambient_color_location, lighting.ambient_color.x, lighting.ambient_color.y, lighting.ambient_color.z);
        if (resource.ambient_intensity_location >= 0) m_uniform1f(resource.ambient_intensity_location, lighting.ambient_intensity);

        const auto* base_texture = find_texture(textures.base_color);
        const auto* metallic_roughness_texture = find_texture(textures.metallic_roughness);
        const auto* normal_texture = find_texture(textures.normal);

        const bool use_base_texture = base_texture != nullptr;
        const bool use_metallic_roughness_texture = metallic_roughness_texture != nullptr;
        const bool use_normal_texture = normal_texture != nullptr;

        if (resource.use_texture_location >= 0) m_uniform1i(resource.use_texture_location, use_base_texture ? 1 : 0);
        if (resource.use_metallic_roughness_texture_location >= 0) {
            m_uniform1i(resource.use_metallic_roughness_texture_location, use_metallic_roughness_texture ? 1 : 0);
        }
        if (resource.use_normal_texture_location >= 0) m_uniform1i(resource.use_normal_texture_location, use_normal_texture ? 1 : 0);

        if (use_base_texture) {
            m_active_texture(GlTexture0);
            m_bind_texture(GlTexture2D, base_texture->texture);
            if (resource.texture_location >= 0) m_uniform1i(resource.texture_location, 0);
        }
        if (use_metallic_roughness_texture) {
            m_active_texture(GlTexture1);
            m_bind_texture(GlTexture2D, metallic_roughness_texture->texture);
            if (resource.metallic_roughness_texture_location >= 0) m_uniform1i(resource.metallic_roughness_texture_location, 1);
        }
        if (use_normal_texture) {
            m_active_texture(GlTexture2);
            m_bind_texture(GlTexture2D, normal_texture->texture);
            if (resource.normal_texture_location >= 0) m_uniform1i(resource.normal_texture_location, 2);
        }

        const bool use_shadows = receive_shadows && lighting.shadows_enabled && m_shadow_depth_texture != 0;
        if (resource.receive_shadows_location >= 0) m_uniform1i(resource.receive_shadows_location, receive_shadows ? 1 : 0);
        if (resource.shadows_enabled_location >= 0) m_uniform1i(resource.shadows_enabled_location, use_shadows ? 1 : 0);
        if (use_shadows) {
            m_active_texture(GlTexture3);
            m_bind_texture(GlTexture2D, m_shadow_depth_texture);
            if (resource.shadow_map_location >= 0) m_uniform1i(resource.shadow_map_location, 3);
        }

        m_bind_vertex_array(mesh_found->second.vertex_array);
        m_draw_elements(GlTriangles, mesh_found->second.index_count, GlUnsignedInt, nullptr);
        m_bind_vertex_array(0);

        if (use_shadows) {
            m_active_texture(GlTexture3);
            m_bind_texture(GlTexture2D, 0);
        }
        if (use_normal_texture) {
            m_active_texture(GlTexture2);
            m_bind_texture(GlTexture2D, 0);
        }
        if (use_metallic_roughness_texture) {
            m_active_texture(GlTexture1);
            m_bind_texture(GlTexture2D, 0);
        }
        if (use_base_texture) {
            m_active_texture(GlTexture0);
            m_bind_texture(GlTexture2D, 0);
        }
        m_active_texture(GlTexture0);
        m_use_program(0);
    }

    void end_frame() override {
        if (m_window) glfwSwapBuffers(m_window);
    }

    std::string_view backend_name() const override { return "Seed OpenGL"; }

    void shutdown() override {
        if (m_window) {
            glfwMakeContextCurrent(m_window);

            for (const auto& [id, resource] : m_textures) {
                (void)id;
                if (resource.texture != 0 && m_delete_textures) m_delete_textures(1, &resource.texture);
            }
            for (const auto& [id, resource] : m_meshes) {
                (void)id;
                destroy_mesh_resource(resource);
            }
            for (const auto& [id, resource] : m_shaders) {
                (void)id;
                if (resource.program != 0 && m_delete_program) m_delete_program(resource.program);
            }

            if (m_shadow_framebuffer != 0 && m_delete_framebuffers) m_delete_framebuffers(1, &m_shadow_framebuffer);
            if (m_shadow_depth_texture != 0 && m_delete_textures) m_delete_textures(1, &m_shadow_depth_texture);
            if (m_shadow_program != 0 && m_delete_program) m_delete_program(m_shadow_program);
            if (m_sky_program != 0 && m_delete_program) m_delete_program(m_sky_program);
            if (m_sky_vertex_array != 0 && m_delete_vertex_arrays) m_delete_vertex_arrays(1, &m_sky_vertex_array);
        }

        m_textures.clear();
        m_meshes.clear();
        m_shaders.clear();
        m_shadow_framebuffer = 0;
        m_shadow_depth_texture = 0;
        m_shadow_program = 0;
        m_sky_program = 0;
        m_sky_vertex_array = 0;
        m_shadow_pass_active = false;

        if (m_window && glfwGetCurrentContext() == m_window) glfwMakeContextCurrent(nullptr);
        m_window = nullptr;
        clear_function_pointers();
    }

    ~OpenGLRenderer() override { shutdown(); }

private:
    template <typename Function>
    static Function load(const char* name) {
        return reinterpret_cast<Function>(glfwGetProcAddress(name));
    }

    void enable_vertex_attribute(unsigned int index, int count, std::size_t offset) {
        m_enable_vertex_attrib_array(index);
        m_vertex_attrib_pointer(index, count, GlFloat, GlFalse, static_cast<int>(sizeof(VertexPositionColor)), reinterpret_cast<const void*>(offset));
    }

    void set_matrix(int location, const Mat4& matrix) {
        if (location >= 0) m_uniform_matrix4fv(location, 1, GlFalse, matrix.data());
    }

    const OpenGLTextureResource* find_texture(TextureHandle handle) const {
        if (!handle) return nullptr;
        const auto found = m_textures.find(handle.value);
        return found == m_textures.end() ? nullptr : &found->second;
    }

    unsigned int compile_shader(unsigned int type, std::string_view source, const char* label) {
        const auto shader = m_create_shader(type);
        const char* pointer = source.data();
        const int length = static_cast<int>(source.size());
        m_shader_source(shader, 1, &pointer, &length);
        m_compile_shader(shader);

        int compiled = 0;
        m_get_shader_iv(shader, GlCompileStatus, &compiled);
        if (compiled != 0) return shader;

        int log_length = 0;
        m_get_shader_iv(shader, GlInfoLogLength, &log_length);
        std::string log(static_cast<std::size_t>(log_length > 0 ? log_length : 1), '\0');
        int actual_length = 0;
        m_get_shader_info_log(shader, static_cast<int>(log.size()), &actual_length, log.data());
        std::cerr << "[SeedRenderer] " << label << " shader compile failed: " << log << '\n';
        m_delete_shader(shader);
        return 0;
    }

    unsigned int link_program(std::string_view vertex_source, std::string_view fragment_source, const char* label) {
        const auto vertex = compile_shader(GlVertexShader, vertex_source, label);
        if (vertex == 0) return 0;
        const auto fragment = compile_shader(GlFragmentShader, fragment_source, label);
        if (fragment == 0) {
            m_delete_shader(vertex);
            return 0;
        }

        const auto program = m_create_program();
        m_attach_shader(program, vertex);
        m_attach_shader(program, fragment);
        m_link_program(program);
        m_delete_shader(vertex);
        m_delete_shader(fragment);

        int linked = 0;
        m_get_program_iv(program, GlLinkStatus, &linked);
        if (linked != 0) return program;

        int log_length = 0;
        m_get_program_iv(program, GlInfoLogLength, &log_length);
        std::string log(static_cast<std::size_t>(log_length > 0 ? log_length : 1), '\0');
        int actual_length = 0;
        m_get_program_info_log(program, static_cast<int>(log.size()), &actual_length, log.data());
        std::cerr << "[SeedRenderer] " << label << " program link failed: " << log << '\n';
        m_delete_program(program);
        return 0;
    }

    bool create_sky_resources() {
        constexpr std::string_view vertex = R"GLSL(
#version 330 core
out vec2 vUv;
void main() {
    vec2 position = gl_VertexID == 0 ? vec2(-1.0, -1.0) :
                    gl_VertexID == 1 ? vec2( 3.0, -1.0) : vec2(-1.0, 3.0);
    vUv = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";
        constexpr std::string_view fragment = R"GLSL(
#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform vec3 uZenithColor;
uniform vec3 uHorizonColor;
uniform float uSkyIntensity;
void main() {
    float t = smoothstep(0.0, 1.0, clamp(vUv.y, 0.0, 1.0));
    vec3 color = mix(uHorizonColor, uZenithColor, t) * max(uSkyIntensity, 0.0);
    FragColor = vec4(color, 1.0);
}
)GLSL";
        m_sky_program = link_program(vertex, fragment, "Seed sky");
        if (m_sky_program == 0) return false;
        m_sky_zenith_location = m_get_uniform_location(m_sky_program, "uZenithColor");
        m_sky_horizon_location = m_get_uniform_location(m_sky_program, "uHorizonColor");
        m_sky_intensity_location = m_get_uniform_location(m_sky_program, "uSkyIntensity");
        m_gen_vertex_arrays(1, &m_sky_vertex_array);
        return m_sky_vertex_array != 0;
    }

    bool create_shadow_resources() {
        constexpr std::string_view vertex = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
uniform mat4 uModel;
uniform mat4 uLightViewProjection;
void main() {
    gl_Position = uLightViewProjection * uModel * vec4(aPosition, 1.0);
}
)GLSL";
        constexpr std::string_view fragment = R"GLSL(
#version 330 core
void main() {}
)GLSL";
        m_shadow_program = link_program(vertex, fragment, "Seed shadow");
        if (m_shadow_program == 0) return false;
        m_shadow_model_location = m_get_uniform_location(m_shadow_program, "uModel");
        m_shadow_light_view_projection_location = m_get_uniform_location(m_shadow_program, "uLightViewProjection");

        m_gen_textures(1, &m_shadow_depth_texture);
        m_bind_texture(GlTexture2D, m_shadow_depth_texture);
        m_tex_image_2d(GlTexture2D, 0, static_cast<int>(GlDepthComponent24), static_cast<int>(ShadowMapSize), static_cast<int>(ShadowMapSize), 0, GlDepthComponent, GlFloat, nullptr);
        m_tex_parameter_i(GlTexture2D, GlTextureMinFilter, GlNearest);
        m_tex_parameter_i(GlTexture2D, GlTextureMagFilter, GlNearest);
        m_tex_parameter_i(GlTexture2D, GlTextureWrapS, GlClampToBorder);
        m_tex_parameter_i(GlTexture2D, GlTextureWrapT, GlClampToBorder);
        const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_tex_parameter_fv(GlTexture2D, GlTextureBorderColor, border);

        m_gen_framebuffers(1, &m_shadow_framebuffer);
        m_bind_framebuffer(GlFramebuffer, m_shadow_framebuffer);
        m_framebuffer_texture_2d(GlFramebuffer, GlDepthAttachment, GlTexture2D, m_shadow_depth_texture, 0);
        m_draw_buffer(GlNone);
        m_read_buffer(GlNone);
        const bool complete = m_check_framebuffer_status(GlFramebuffer) == GlFramebufferComplete;
        m_bind_framebuffer(GlFramebuffer, 0);
        m_bind_texture(GlTexture2D, 0);
        return complete;
    }

    bool load_functions() {
        m_clear_color = load<GlClearColorFn>("glClearColor");
        m_clear = load<GlClearFn>("glClear");
        m_viewport = load<GlViewportFn>("glViewport");
        m_enable = load<GlEnableFn>("glEnable");
        m_disable = load<GlDisableFn>("glDisable");
        m_gen_vertex_arrays = load<GlGenVertexArraysFn>("glGenVertexArrays");
        m_bind_vertex_array = load<GlBindVertexArrayFn>("glBindVertexArray");
        m_delete_vertex_arrays = load<GlDeleteVertexArraysFn>("glDeleteVertexArrays");
        m_gen_buffers = load<GlGenBuffersFn>("glGenBuffers");
        m_bind_buffer = load<GlBindBufferFn>("glBindBuffer");
        m_buffer_data = load<GlBufferDataFn>("glBufferData");
        m_delete_buffers = load<GlDeleteBuffersFn>("glDeleteBuffers");
        m_enable_vertex_attrib_array = load<GlEnableVertexAttribArrayFn>("glEnableVertexAttribArray");
        m_vertex_attrib_pointer = load<GlVertexAttribPointerFn>("glVertexAttribPointer");
        m_create_shader = load<GlCreateShaderFn>("glCreateShader");
        m_shader_source = load<GlShaderSourceFn>("glShaderSource");
        m_compile_shader = load<GlCompileShaderFn>("glCompileShader");
        m_get_shader_iv = load<GlGetShaderivFn>("glGetShaderiv");
        m_get_shader_info_log = load<GlGetShaderInfoLogFn>("glGetShaderInfoLog");
        m_delete_shader = load<GlDeleteShaderFn>("glDeleteShader");
        m_create_program = load<GlCreateProgramFn>("glCreateProgram");
        m_attach_shader = load<GlAttachShaderFn>("glAttachShader");
        m_link_program = load<GlLinkProgramFn>("glLinkProgram");
        m_get_program_iv = load<GlGetProgramivFn>("glGetProgramiv");
        m_get_program_info_log = load<GlGetProgramInfoLogFn>("glGetProgramInfoLog");
        m_delete_program = load<GlDeleteProgramFn>("glDeleteProgram");
        m_use_program = load<GlUseProgramFn>("glUseProgram");
        m_get_uniform_location = load<GlGetUniformLocationFn>("glGetUniformLocation");
        m_uniform_matrix4fv = load<GlUniformMatrix4fvFn>("glUniformMatrix4fv");
        m_uniform1i = load<GlUniform1iFn>("glUniform1i");
        m_uniform1f = load<GlUniform1fFn>("glUniform1f");
        m_uniform3f = load<GlUniform3fFn>("glUniform3f");
        m_uniform4f = load<GlUniform4fFn>("glUniform4f");
        m_draw_elements = load<GlDrawElementsFn>("glDrawElements");
        m_draw_arrays = load<GlDrawArraysFn>("glDrawArrays");
        m_gen_textures = load<GlGenTexturesFn>("glGenTextures");
        m_bind_texture = load<GlBindTextureFn>("glBindTexture");
        m_tex_image_2d = load<GlTexImage2DFn>("glTexImage2D");
        m_tex_parameter_i = load<GlTexParameteriFn>("glTexParameteri");
        m_tex_parameter_fv = load<GlTexParameterfvFn>("glTexParameterfv");
        m_delete_textures = load<GlDeleteTexturesFn>("glDeleteTextures");
        m_active_texture = load<GlActiveTextureFn>("glActiveTexture");
        m_gen_framebuffers = load<GlGenFramebuffersFn>("glGenFramebuffers");
        m_bind_framebuffer = load<GlBindFramebufferFn>("glBindFramebuffer");
        m_framebuffer_texture_2d = load<GlFramebufferTexture2DFn>("glFramebufferTexture2D");
        m_check_framebuffer_status = load<GlCheckFramebufferStatusFn>("glCheckFramebufferStatus");
        m_delete_framebuffers = load<GlDeleteFramebuffersFn>("glDeleteFramebuffers");
        m_draw_buffer = load<GlDrawBufferFn>("glDrawBuffer");
        m_read_buffer = load<GlReadBufferFn>("glReadBuffer");

        return m_clear_color && m_clear && m_viewport && m_enable && m_disable &&
            m_gen_vertex_arrays && m_bind_vertex_array && m_delete_vertex_arrays &&
            m_gen_buffers && m_bind_buffer && m_buffer_data && m_delete_buffers &&
            m_enable_vertex_attrib_array && m_vertex_attrib_pointer &&
            m_create_shader && m_shader_source && m_compile_shader && m_get_shader_iv &&
            m_get_shader_info_log && m_delete_shader && m_create_program && m_attach_shader &&
            m_link_program && m_get_program_iv && m_get_program_info_log && m_delete_program &&
            m_use_program && m_get_uniform_location && m_uniform_matrix4fv && m_uniform1i &&
            m_uniform1f && m_uniform3f && m_uniform4f && m_draw_elements && m_draw_arrays &&
            m_gen_textures && m_bind_texture && m_tex_image_2d && m_tex_parameter_i &&
            m_tex_parameter_fv && m_delete_textures && m_active_texture &&
            m_gen_framebuffers && m_bind_framebuffer && m_framebuffer_texture_2d &&
            m_check_framebuffer_status && m_delete_framebuffers && m_draw_buffer && m_read_buffer;
    }

    void destroy_mesh_resource(const OpenGLMeshResource& resource) {
        if (resource.index_buffer != 0 && m_delete_buffers) m_delete_buffers(1, &resource.index_buffer);
        if (resource.vertex_buffer != 0 && m_delete_buffers) m_delete_buffers(1, &resource.vertex_buffer);
        if (resource.vertex_array != 0 && m_delete_vertex_arrays) m_delete_vertex_arrays(1, &resource.vertex_array);
    }

    void clear_function_pointers() {
        m_clear_color = nullptr; m_clear = nullptr; m_viewport = nullptr; m_enable = nullptr; m_disable = nullptr;
        m_gen_vertex_arrays = nullptr; m_bind_vertex_array = nullptr; m_delete_vertex_arrays = nullptr;
        m_gen_buffers = nullptr; m_bind_buffer = nullptr; m_buffer_data = nullptr; m_delete_buffers = nullptr;
        m_enable_vertex_attrib_array = nullptr; m_vertex_attrib_pointer = nullptr;
        m_create_shader = nullptr; m_shader_source = nullptr; m_compile_shader = nullptr;
        m_get_shader_iv = nullptr; m_get_shader_info_log = nullptr; m_delete_shader = nullptr;
        m_create_program = nullptr; m_attach_shader = nullptr; m_link_program = nullptr;
        m_get_program_iv = nullptr; m_get_program_info_log = nullptr; m_delete_program = nullptr;
        m_use_program = nullptr; m_get_uniform_location = nullptr; m_uniform_matrix4fv = nullptr;
        m_uniform1i = nullptr; m_uniform1f = nullptr; m_uniform3f = nullptr; m_uniform4f = nullptr;
        m_draw_elements = nullptr; m_draw_arrays = nullptr;
        m_gen_textures = nullptr; m_bind_texture = nullptr; m_tex_image_2d = nullptr;
        m_tex_parameter_i = nullptr; m_tex_parameter_fv = nullptr; m_delete_textures = nullptr; m_active_texture = nullptr;
        m_gen_framebuffers = nullptr; m_bind_framebuffer = nullptr; m_framebuffer_texture_2d = nullptr;
        m_check_framebuffer_status = nullptr; m_delete_framebuffers = nullptr; m_draw_buffer = nullptr; m_read_buffer = nullptr;
    }

    GLFWwindow* m_window{nullptr};
    RendererConfig m_config{};
    std::unordered_map<std::uint32_t, OpenGLShaderResource> m_shaders;
    std::unordered_map<std::uint32_t, OpenGLMeshResource> m_meshes;
    std::unordered_map<std::uint32_t, OpenGLTextureResource> m_textures;
    std::uint32_t m_next_shader_id{1};
    std::uint32_t m_next_mesh_id{1};
    std::uint32_t m_next_texture_id{1};

    unsigned int m_sky_program{0};
    unsigned int m_sky_vertex_array{0};
    int m_sky_zenith_location{-1};
    int m_sky_horizon_location{-1};
    int m_sky_intensity_location{-1};

    unsigned int m_shadow_program{0};
    unsigned int m_shadow_framebuffer{0};
    unsigned int m_shadow_depth_texture{0};
    int m_shadow_model_location{-1};
    int m_shadow_light_view_projection_location{-1};
    bool m_shadow_pass_active{false};

    GlClearColorFn m_clear_color{nullptr};
    GlClearFn m_clear{nullptr};
    GlViewportFn m_viewport{nullptr};
    GlEnableFn m_enable{nullptr};
    GlDisableFn m_disable{nullptr};
    GlGenVertexArraysFn m_gen_vertex_arrays{nullptr};
    GlBindVertexArrayFn m_bind_vertex_array{nullptr};
    GlDeleteVertexArraysFn m_delete_vertex_arrays{nullptr};
    GlGenBuffersFn m_gen_buffers{nullptr};
    GlBindBufferFn m_bind_buffer{nullptr};
    GlBufferDataFn m_buffer_data{nullptr};
    GlDeleteBuffersFn m_delete_buffers{nullptr};
    GlEnableVertexAttribArrayFn m_enable_vertex_attrib_array{nullptr};
    GlVertexAttribPointerFn m_vertex_attrib_pointer{nullptr};
    GlCreateShaderFn m_create_shader{nullptr};
    GlShaderSourceFn m_shader_source{nullptr};
    GlCompileShaderFn m_compile_shader{nullptr};
    GlGetShaderivFn m_get_shader_iv{nullptr};
    GlGetShaderInfoLogFn m_get_shader_info_log{nullptr};
    GlDeleteShaderFn m_delete_shader{nullptr};
    GlCreateProgramFn m_create_program{nullptr};
    GlAttachShaderFn m_attach_shader{nullptr};
    GlLinkProgramFn m_link_program{nullptr};
    GlGetProgramivFn m_get_program_iv{nullptr};
    GlGetProgramInfoLogFn m_get_program_info_log{nullptr};
    GlDeleteProgramFn m_delete_program{nullptr};
    GlUseProgramFn m_use_program{nullptr};
    GlGetUniformLocationFn m_get_uniform_location{nullptr};
    GlUniformMatrix4fvFn m_uniform_matrix4fv{nullptr};
    GlUniform1iFn m_uniform1i{nullptr};
    GlUniform1fFn m_uniform1f{nullptr};
    GlUniform3fFn m_uniform3f{nullptr};
    GlUniform4fFn m_uniform4f{nullptr};
    GlDrawElementsFn m_draw_elements{nullptr};
    GlDrawArraysFn m_draw_arrays{nullptr};
    GlGenTexturesFn m_gen_textures{nullptr};
    GlBindTextureFn m_bind_texture{nullptr};
    GlTexImage2DFn m_tex_image_2d{nullptr};
    GlTexParameteriFn m_tex_parameter_i{nullptr};
    GlTexParameterfvFn m_tex_parameter_fv{nullptr};
    GlDeleteTexturesFn m_delete_textures{nullptr};
    GlActiveTextureFn m_active_texture{nullptr};
    GlGenFramebuffersFn m_gen_framebuffers{nullptr};
    GlBindFramebufferFn m_bind_framebuffer{nullptr};
    GlFramebufferTexture2DFn m_framebuffer_texture_2d{nullptr};
    GlCheckFramebufferStatusFn m_check_framebuffer_status{nullptr};
    GlDeleteFramebuffersFn m_delete_framebuffers{nullptr};
    GlDrawBufferFn m_draw_buffer{nullptr};
    GlReadBufferFn m_read_buffer{nullptr};
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
