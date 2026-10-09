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
constexpr unsigned int GlArrayBuffer = 0x8892;
constexpr unsigned int GlElementArrayBuffer = 0x8893;
constexpr unsigned int GlStaticDraw = 0x88E4;
constexpr unsigned int GlFloat = 0x1406;
constexpr unsigned int GlUnsignedInt = 0x1405;
constexpr unsigned int GlTriangles = 0x0004;
constexpr unsigned int GlVertexShader = 0x8B31;
constexpr unsigned int GlFragmentShader = 0x8B30;
constexpr unsigned int GlCompileStatus = 0x8B81;
constexpr unsigned int GlLinkStatus = 0x8B82;
constexpr unsigned int GlInfoLogLength = 0x8B84;
constexpr unsigned char GlFalse = 0;

using GlClearColorFn = void (*)(float, float, float, float);
using GlClearFn = void (*)(unsigned int);
using GlViewportFn = void (*)(int, int, int, int);
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
using GlDrawElementsFn = void (*)(unsigned int, int, unsigned int, const void*);

struct OpenGLShaderResource {
    unsigned int program{0};
};

struct OpenGLMeshResource {
    unsigned int vertex_array{0};
    unsigned int vertex_buffer{0};
    unsigned int index_buffer{0};
    int index_count{0};
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

        const ShaderHandle handle{m_next_shader_id++};
        m_shaders.emplace(handle.value, OpenGLShaderResource{program});
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

        m_bind_vertex_array(0);
        m_bind_buffer(GlArrayBuffer, 0);
        m_bind_buffer(GlElementArrayBuffer, 0);

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

    void draw_mesh(MeshHandle mesh, ShaderHandle shader) override {
        const auto mesh_found = m_meshes.find(mesh.value);
        const auto shader_found = m_shaders.find(shader.value);
        if (mesh_found == m_meshes.end() || shader_found == m_shaders.end()) {
            return;
        }

        m_use_program(shader_found->second.program);
        m_bind_vertex_array(mesh_found->second.vertex_array);
        m_draw_elements(
            GlTriangles,
            mesh_found->second.index_count,
            GlUnsignedInt,
            nullptr
        );
        m_bind_vertex_array(0);
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
        }

        for (auto& [_, mesh] : m_meshes) {
            destroy_mesh_resource(mesh);
        }
        m_meshes.clear();

        if (m_delete_program) {
            for (auto& [_, shader] : m_shaders) {
                if (shader.program != 0) {
                    m_delete_program(shader.program);
                }
            }
        }
        m_shaders.clear();

        if (m_window && glfwGetCurrentContext() == m_window) {
            glfwMakeContextCurrent(nullptr);
        }

        m_window = nullptr;
        reset_functions();
    }

    ~OpenGLRenderer() override {
        shutdown();
    }

private:
    template <typename T>
    bool load(T& function, const char* name) {
        function = reinterpret_cast<T>(glfwGetProcAddress(name));
        return function != nullptr;
    }

    bool load_functions() {
        bool ok = true;
        ok &= load(m_clear_color, "glClearColor");
        ok &= load(m_clear, "glClear");
        ok &= load(m_viewport, "glViewport");
        ok &= load(m_gen_vertex_arrays, "glGenVertexArrays");
        ok &= load(m_bind_vertex_array, "glBindVertexArray");
        ok &= load(m_delete_vertex_arrays, "glDeleteVertexArrays");
        ok &= load(m_gen_buffers, "glGenBuffers");
        ok &= load(m_bind_buffer, "glBindBuffer");
        ok &= load(m_buffer_data, "glBufferData");
        ok &= load(m_delete_buffers, "glDeleteBuffers");
        ok &= load(m_enable_vertex_attrib_array, "glEnableVertexAttribArray");
        ok &= load(m_vertex_attrib_pointer, "glVertexAttribPointer");
        ok &= load(m_create_shader, "glCreateShader");
        ok &= load(m_shader_source, "glShaderSource");
        ok &= load(m_compile_shader, "glCompileShader");
        ok &= load(m_get_shader_iv, "glGetShaderiv");
        ok &= load(m_get_shader_info_log, "glGetShaderInfoLog");
        ok &= load(m_delete_shader, "glDeleteShader");
        ok &= load(m_create_program, "glCreateProgram");
        ok &= load(m_attach_shader, "glAttachShader");
        ok &= load(m_link_program, "glLinkProgram");
        ok &= load(m_get_program_iv, "glGetProgramiv");
        ok &= load(m_get_program_info_log, "glGetProgramInfoLog");
        ok &= load(m_delete_program, "glDeleteProgram");
        ok &= load(m_use_program, "glUseProgram");
        ok &= load(m_draw_elements, "glDrawElements");
        return ok;
    }

    unsigned int compile_shader(unsigned int type, std::string_view source, const char* label) {
        const auto shader = m_create_shader(type);
        if (shader == 0) {
            std::cerr << "[SeedRenderer] Failed to create " << label << " shader.\n";
            return 0;
        }

        const std::string owned_source(source);
        const char* source_pointer = owned_source.c_str();
        m_shader_source(shader, 1, &source_pointer, nullptr);
        m_compile_shader(shader);

        int compiled = 0;
        m_get_shader_iv(shader, GlCompileStatus, &compiled);
        if (compiled == 0) {
            std::cerr << "[SeedRenderer] " << label << " shader compilation failed.\n";
            print_shader_log(shader);
            m_delete_shader(shader);
            return 0;
        }

        return shader;
    }

    void print_shader_log(unsigned int shader) {
        int length = 0;
        m_get_shader_iv(shader, GlInfoLogLength, &length);
        if (length <= 1) {
            return;
        }

        std::string log(static_cast<std::size_t>(length), '\0');
        int written = 0;
        m_get_shader_info_log(shader, length, &written, log.data());
        std::cerr << "[SeedRenderer] " << log << '\n';
    }

    void print_program_log(unsigned int program) {
        int length = 0;
        m_get_program_iv(program, GlInfoLogLength, &length);
        if (length <= 1) {
            return;
        }

        std::string log(static_cast<std::size_t>(length), '\0');
        int written = 0;
        m_get_program_info_log(program, length, &written, log.data());
        std::cerr << "[SeedRenderer] Program link failed: " << log << '\n';
    }

    void destroy_mesh_resource(OpenGLMeshResource& resource) {
        if (m_delete_buffers) {
            if (resource.index_buffer != 0) {
                m_delete_buffers(1, &resource.index_buffer);
            }
            if (resource.vertex_buffer != 0) {
                m_delete_buffers(1, &resource.vertex_buffer);
            }
        }
        if (m_delete_vertex_arrays && resource.vertex_array != 0) {
            m_delete_vertex_arrays(1, &resource.vertex_array);
        }
        resource = {};
    }

    void reset_functions() {
        m_clear_color = nullptr;
        m_clear = nullptr;
        m_viewport = nullptr;
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
        m_draw_elements = nullptr;
    }

    GLFWwindow* m_window{nullptr};
    RendererConfig m_config{};

    std::uint32_t m_next_shader_id{1};
    std::uint32_t m_next_mesh_id{1};
    std::unordered_map<std::uint32_t, OpenGLShaderResource> m_shaders;
    std::unordered_map<std::uint32_t, OpenGLMeshResource> m_meshes;

    GlClearColorFn m_clear_color{nullptr};
    GlClearFn m_clear{nullptr};
    GlViewportFn m_viewport{nullptr};
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
    GlDrawElementsFn m_draw_elements{nullptr};
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
