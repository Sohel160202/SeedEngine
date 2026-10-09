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
constexpr unsigned int GlRgba = 0x1908;
constexpr unsigned int GlTextureMinFilter = 0x2801;
constexpr unsigned int GlTextureMagFilter = 0x2800;
constexpr unsigned int GlTextureWrapS = 0x2802;
constexpr unsigned int GlTextureWrapT = 0x2803;
constexpr int GlLinear = 0x2601;
constexpr int GlRepeat = 0x2901;
constexpr unsigned char GlFalse = 0;

using GlClearColorFn = void (*)(float, float, float, float);
using GlClearFn = void (*)(unsigned int);
using GlViewportFn = void (*)(int, int, int, int);
using GlEnableFn = void (*)(unsigned int);
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
using GlGenTexturesFn = void (*)(int, unsigned int*);
using GlBindTextureFn = void (*)(unsigned int, unsigned int);
using GlTexImage2DFn = void (*)(unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
using GlTexParameteriFn = void (*)(unsigned int, unsigned int, int);
using GlDeleteTexturesFn = void (*)(int, const unsigned int*);
using GlActiveTextureFn = void (*)(unsigned int);

struct OpenGLShaderResource {
    unsigned int program{0};
    int model_location{-1};
    int view_projection_location{-1};
    int base_color_location{-1};
    int texture_location{-1};
    int use_texture_location{-1};
    int directional_direction_location{-1};
    int directional_color_location{-1};
    int directional_intensity_location{-1};
    int ambient_color_location{-1};
    int ambient_intensity_location{-1};
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
        m_config.width = width > 0 ? width : 1;
        m_config.height = height > 0 ? height : 1;

        if (m_viewport) {
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

        const auto vertex = compile_shader(GlVertexShader, desc.vertex_source, "vertex");
        if (vertex == 0) {
            return {};
        }

        const auto fragment = compile_shader(GlFragmentShader, desc.fragment_source, "fragment");
        if (fragment == 0) {
            m_delete_shader(vertex);
            return {};
        }

        const auto program = m_create_program();
        m_attach_shader(program, vertex);
        m_attach_shader(program, fragment);
        m_link_program(program);

        int linked = 0;
        m_get_program_iv(program, GlLinkStatus, &linked);

        m_delete_shader(vertex);
        m_delete_shader(fragment);

        if (linked == 0) {
            print_program_log(program);
            m_delete_program(program);
            return {};
        }

        OpenGLShaderResource resource{};
        resource.program = program;
        resource.model_location = m_get_uniform_location(program, "uModel");
        resource.view_projection_location = m_get_uniform_location(program, "uViewProjection");
        resource.base_color_location = m_get_uniform_location(program, "uBaseColor");
        resource.texture_location = m_get_uniform_location(program, "uBaseTexture");
        resource.use_texture_location = m_get_uniform_location(program, "uUseTexture");
        resource.directional_direction_location = m_get_uniform_location(program, "uDirectionalDirection");
        resource.directional_color_location = m_get_uniform_location(program, "uDirectionalColor");
        resource.directional_intensity_location = m_get_uniform_location(program, "uDirectionalIntensity");
        resource.ambient_color_location = m_get_uniform_location(program, "uAmbientColor");
        resource.ambient_intensity_location = m_get_uniform_location(program, "uAmbientIntensity");

        const ShaderHandle handle{m_next_shader_id++};
        m_shaders.emplace(handle.value, resource);
        return handle;
    }

    void destroy_shader(ShaderHandle shader) override {
        const auto found = m_shaders.find(shader.value);
        if (found == m_shaders.end()) {
            return;
        }

        if (m_window) {
            glfwMakeContextCurrent(m_window);
        }
        if (found->second.program != 0 && m_delete_program) {
            m_delete_program(found->second.program);
        }
        m_shaders.erase(found);
    }

    MeshHandle create_mesh(const MeshDesc& desc) override {
        if (!m_window || desc.vertices.empty() || desc.indices.empty()) {
            return {};
        }

        glfwMakeContextCurrent(m_window);

        OpenGLMeshResource resource{};
        m_gen_vertex_arrays(1, &resource.vertex_array);
        m_gen_buffers(1, &resource.vertex_buffer);
        m_gen_buffers(1, &resource.index_buffer);
        resource.index_count = static_cast<int>(desc.indices.size());

        m_bind_vertex_array(resource.vertex_array);

        m_bind_buffer(GlArrayBuffer, resource.vertex_buffer);
        m_buffer_data(
            GlArrayBuffer,
            static_cast<std::ptrdiff_t>(desc.vertices.size_bytes()),
            desc.vertices.data(),
            GlStaticDraw
        );

        m_bind_buffer(GlElementArrayBuffer, resource.index_buffer);
        m_buffer_data(
            GlElementArrayBuffer,
            static_cast<std::ptrdiff_t>(desc.indices.size_bytes()),
            desc.indices.data(),
            GlStaticDraw
        );

        m_enable_vertex_attrib_array(0);
        m_vertex_attrib_pointer(
            0,
            3,
            GlFloat,
            GlFalse,
            static_cast<int>(sizeof(VertexPositionColor)),
            reinterpret_cast<const void*>(offsetof(VertexPositionColor, position))
        );

        m_enable_vertex_attrib_array(1);
        m_vertex_attrib_pointer(
            1,
            3,
            GlFloat,
            GlFalse,
            static_cast<int>(sizeof(VertexPositionColor)),
            reinterpret_cast<const void*>(offsetof(VertexPositionColor, color))
        );

        m_enable_vertex_attrib_array(2);
        m_vertex_attrib_pointer(
            2,
            2,
            GlFloat,
            GlFalse,
            static_cast<int>(sizeof(VertexPositionColor)),
            reinterpret_cast<const void*>(offsetof(VertexPositionColor, texcoord))
        );

        m_enable_vertex_attrib_array(3);
        m_vertex_attrib_pointer(
            3,
            3,
            GlFloat,
            GlFalse,
            static_cast<int>(sizeof(VertexPositionColor)),
            reinterpret_cast<const void*>(offsetof(VertexPositionColor, normal))
        );

        m_bind_vertex_array(0);
        m_bind_buffer(GlArrayBuffer, 0);

        const MeshHandle handle{m_next_mesh_id++};
        m_meshes.emplace(handle.value, resource);
        return handle;
    }

    void destroy_mesh(MeshHandle mesh) override {
        const auto found = m_meshes.find(mesh.value);
        if (found == m_meshes.end()) {
            return;
        }

        if (m_window) {
            glfwMakeContextCurrent(m_window);
        }
        destroy_mesh_resource(found->second);
        m_meshes.erase(found);
    }

    TextureHandle create_texture(const TextureDesc& desc) override {
        const std::size_t expected_size = static_cast<std::size_t>(desc.width) *
            static_cast<std::size_t>(desc.height) * 4u;
        if (!m_window || desc.width == 0 || desc.height == 0 ||
            desc.rgba8_pixels.size() < expected_size) {
            return {};
        }

        glfwMakeContextCurrent(m_window);

        OpenGLTextureResource resource{};
        m_gen_textures(1, &resource.texture);
        m_bind_texture(GlTexture2D, resource.texture);
        m_tex_parameter_i(GlTexture2D, GlTextureMinFilter, GlLinear);
        m_tex_parameter_i(GlTexture2D, GlTextureMagFilter, GlLinear);
        m_tex_parameter_i(GlTexture2D, GlTextureWrapS, GlRepeat);
        m_tex_parameter_i(GlTexture2D, GlTextureWrapT, GlRepeat);
        m_tex_image_2d(
            GlTexture2D,
            0,
            static_cast<int>(GlRgba),
            static_cast<int>(desc.width),
            static_cast<int>(desc.height),
            0,
            GlRgba,
            GlUnsignedByte,
            desc.rgba8_pixels.data()
        );
        m_bind_texture(GlTexture2D, 0);

        const TextureHandle handle{m_next_texture_id++};
        m_textures.emplace(handle.value, resource);
        return handle;
    }

    void destroy_texture(TextureHandle texture) override {
        const auto found = m_textures.find(texture.value);
        if (found == m_textures.end()) {
            return;
        }

        if (m_window) {
            glfwMakeContextCurrent(m_window);
        }
        if (found->second.texture != 0 && m_delete_textures) {
            m_delete_textures(1, &found->second.texture);
        }
        m_textures.erase(found);
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

    void draw_mesh(
        MeshHandle mesh,
        ShaderHandle shader,
        TextureHandle texture,
        const Vec4& base_color,
        const Mat4& model,
        const Mat4& view_projection,
        const SceneLighting& lighting
    ) override {
        const auto mesh_found = m_meshes.find(mesh.value);
        const auto shader_found = m_shaders.find(shader.value);
        if (mesh_found == m_meshes.end() || shader_found == m_shaders.end()) {
            return;
        }

        const auto& shader_resource = shader_found->second;
        m_use_program(shader_resource.program);

        if (shader_resource.model_location >= 0) {
            m_uniform_matrix4fv(shader_resource.model_location, 1, GlFalse, model.data());
        }
        if (shader_resource.view_projection_location >= 0) {
            m_uniform_matrix4fv(
                shader_resource.view_projection_location,
                1,
                GlFalse,
                view_projection.data()
            );
        }
        if (shader_resource.base_color_location >= 0) {
            m_uniform4f(
                shader_resource.base_color_location,
                base_color.x,
                base_color.y,
                base_color.z,
                base_color.w
            );
        }
        if (shader_resource.directional_direction_location >= 0) {
            m_uniform3f(
                shader_resource.directional_direction_location,
                lighting.directional_direction.x,
                lighting.directional_direction.y,
                lighting.directional_direction.z
            );
        }
        if (shader_resource.directional_color_location >= 0) {
            m_uniform3f(
                shader_resource.directional_color_location,
                lighting.directional_color.x,
                lighting.directional_color.y,
                lighting.directional_color.z
            );
        }
        if (shader_resource.directional_intensity_location >= 0) {
            m_uniform1f(shader_resource.directional_intensity_location, lighting.directional_intensity);
        }
        if (shader_resource.ambient_color_location >= 0) {
            m_uniform3f(
                shader_resource.ambient_color_location,
                lighting.ambient_color.x,
                lighting.ambient_color.y,
                lighting.ambient_color.z
            );
        }
        if (shader_resource.ambient_intensity_location >= 0) {
            m_uniform1f(shader_resource.ambient_intensity_location, lighting.ambient_intensity);
        }

        const auto texture_found = m_textures.find(texture.value);
        const bool use_texture = texture && texture_found != m_textures.end();
        if (shader_resource.use_texture_location >= 0) {
            m_uniform1i(shader_resource.use_texture_location, use_texture ? 1 : 0);
        }
        if (use_texture) {
            m_active_texture(GlTexture0);
            m_bind_texture(GlTexture2D, texture_found->second.texture);
            if (shader_resource.texture_location >= 0) {
                m_uniform1i(shader_resource.texture_location, 0);
            }
        }

        m_bind_vertex_array(mesh_found->second.vertex_array);
        m_draw_elements(GlTriangles, mesh_found->second.index_count, GlUnsignedInt, nullptr);
        m_bind_vertex_array(0);

        if (use_texture) {
            m_bind_texture(GlTexture2D, 0);
        }
        m_use_program(0);
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
        if (m_window) {
            glfwMakeContextCurrent(m_window);

            for (const auto& [id, resource] : m_textures) {
                (void)id;
                if (resource.texture != 0 && m_delete_textures) {
                    m_delete_textures(1, &resource.texture);
                }
            }
            for (const auto& [id, resource] : m_meshes) {
                (void)id;
                destroy_mesh_resource(resource);
            }
            for (const auto& [id, resource] : m_shaders) {
                (void)id;
                if (resource.program != 0 && m_delete_program) {
                    m_delete_program(resource.program);
                }
            }
        }

        m_textures.clear();
        m_meshes.clear();
        m_shaders.clear();

        if (m_window && glfwGetCurrentContext() == m_window) {
            glfwMakeContextCurrent(nullptr);
        }

        m_window = nullptr;
        clear_function_pointers();
    }

    ~OpenGLRenderer() override {
        shutdown();
    }

private:
    template <typename Function>
    static Function load(const char* name) {
        return reinterpret_cast<Function>(glfwGetProcAddress(name));
    }

    bool load_functions() {
        m_clear_color = load<GlClearColorFn>("glClearColor");
        m_clear = load<GlClearFn>("glClear");
        m_viewport = load<GlViewportFn>("glViewport");
        m_enable = load<GlEnableFn>("glEnable");
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
        m_gen_textures = load<GlGenTexturesFn>("glGenTextures");
        m_bind_texture = load<GlBindTextureFn>("glBindTexture");
        m_tex_image_2d = load<GlTexImage2DFn>("glTexImage2D");
        m_tex_parameter_i = load<GlTexParameteriFn>("glTexParameteri");
        m_delete_textures = load<GlDeleteTexturesFn>("glDeleteTextures");
        m_active_texture = load<GlActiveTextureFn>("glActiveTexture");

        return m_clear_color && m_clear && m_viewport && m_enable &&
            m_gen_vertex_arrays && m_bind_vertex_array && m_delete_vertex_arrays &&
            m_gen_buffers && m_bind_buffer && m_buffer_data && m_delete_buffers &&
            m_enable_vertex_attrib_array && m_vertex_attrib_pointer &&
            m_create_shader && m_shader_source && m_compile_shader &&
            m_get_shader_iv && m_get_shader_info_log && m_delete_shader &&
            m_create_program && m_attach_shader && m_link_program &&
            m_get_program_iv && m_get_program_info_log && m_delete_program &&
            m_use_program && m_get_uniform_location && m_uniform_matrix4fv &&
            m_uniform1i && m_uniform1f && m_uniform3f && m_uniform4f && m_draw_elements &&
            m_gen_textures && m_bind_texture && m_tex_image_2d &&
            m_tex_parameter_i && m_delete_textures && m_active_texture;
    }

    unsigned int compile_shader(unsigned int type, std::string_view source, const char* label) {
        const auto shader = m_create_shader(type);
        const char* source_pointer = source.data();
        const int source_length = static_cast<int>(source.size());
        m_shader_source(shader, 1, &source_pointer, &source_length);
        m_compile_shader(shader);

        int compiled = 0;
        m_get_shader_iv(shader, GlCompileStatus, &compiled);
        if (compiled != 0) {
            return shader;
        }

        int log_length = 0;
        m_get_shader_iv(shader, GlInfoLogLength, &log_length);
        std::string log(static_cast<std::size_t>(log_length > 0 ? log_length : 1), '\0');
        int actual_length = 0;
        m_get_shader_info_log(shader, static_cast<int>(log.size()), &actual_length, log.data());
        std::cerr << "[SeedRenderer] " << label << " shader compile failed: " << log << '\n';
        m_delete_shader(shader);
        return 0;
    }

    void print_program_log(unsigned int program) {
        int log_length = 0;
        m_get_program_iv(program, GlInfoLogLength, &log_length);
        std::string log(static_cast<std::size_t>(log_length > 0 ? log_length : 1), '\0');
        int actual_length = 0;
        m_get_program_info_log(program, static_cast<int>(log.size()), &actual_length, log.data());
        std::cerr << "[SeedRenderer] shader program link failed: " << log << '\n';
    }

    void destroy_mesh_resource(const OpenGLMeshResource& resource) {
        if (resource.index_buffer != 0 && m_delete_buffers) {
            m_delete_buffers(1, &resource.index_buffer);
        }
        if (resource.vertex_buffer != 0 && m_delete_buffers) {
            m_delete_buffers(1, &resource.vertex_buffer);
        }
        if (resource.vertex_array != 0 && m_delete_vertex_arrays) {
            m_delete_vertex_arrays(1, &resource.vertex_array);
        }
    }

    void clear_function_pointers() {
        m_clear_color = nullptr;
        m_clear = nullptr;
        m_viewport = nullptr;
        m_enable = nullptr;
        m_gen_vertex_arrays = nullptr;
        m_bind_vertex_array = nullptr;
        m_delete_vertex_arrays = nullptr;
        m_gen_buffers = nullptr;
        m_bind_buffer = nullptr;
        m_buffer_data = nullptr;
        m_delete_buffers = nullptr;
        m_enable_vertex_attrib_array = nullptr;
        m_vertex_attrib_pointer = nullptr;
        m_create_shader = nullptr;
        m_shader_source = nullptr;
        m_compile_shader = nullptr;
        m_get_shader_iv = nullptr;
        m_get_shader_info_log = nullptr;
        m_delete_shader = nullptr;
        m_create_program = nullptr;
        m_attach_shader = nullptr;
        m_link_program = nullptr;
        m_get_program_iv = nullptr;
        m_get_program_info_log = nullptr;
        m_delete_program = nullptr;
        m_use_program = nullptr;
        m_get_uniform_location = nullptr;
        m_uniform_matrix4fv = nullptr;
        m_uniform1i = nullptr;
        m_uniform1f = nullptr;
        m_uniform3f = nullptr;
        m_uniform4f = nullptr;
        m_draw_elements = nullptr;
        m_gen_textures = nullptr;
        m_bind_texture = nullptr;
        m_tex_image_2d = nullptr;
        m_tex_parameter_i = nullptr;
        m_delete_textures = nullptr;
        m_active_texture = nullptr;
    }

    GLFWwindow* m_window{nullptr};
    RendererConfig m_config{};
    std::unordered_map<std::uint32_t, OpenGLShaderResource> m_shaders;
    std::unordered_map<std::uint32_t, OpenGLMeshResource> m_meshes;
    std::unordered_map<std::uint32_t, OpenGLTextureResource> m_textures;
    std::uint32_t m_next_shader_id{1};
    std::uint32_t m_next_mesh_id{1};
    std::uint32_t m_next_texture_id{1};

    GlClearColorFn m_clear_color{nullptr};
    GlClearFn m_clear{nullptr};
    GlViewportFn m_viewport{nullptr};
    GlEnableFn m_enable{nullptr};
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
    GlGenTexturesFn m_gen_textures{nullptr};
    GlBindTextureFn m_bind_texture{nullptr};
    GlTexImage2DFn m_tex_image_2d{nullptr};
    GlTexParameteriFn m_tex_parameter_i{nullptr};
    GlDeleteTexturesFn m_delete_textures{nullptr};
    GlActiveTextureFn m_active_texture{nullptr};
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
