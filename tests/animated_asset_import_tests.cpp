#include "seed/animation/AnimationSystem.h"
#include "seed/assets/AssetImporter.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/render/IRenderer.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

template <typename T>
void append(std::vector<std::uint8_t>& bytes, const T& value) {
    const auto* data = reinterpret_cast<const std::uint8_t*>(&value);
    bytes.insert(bytes.end(), data, data + sizeof(T));
}

void append_float(std::vector<std::uint8_t>& bytes, float value) { append(bytes, value); }
void append_u16(std::vector<std::uint8_t>& bytes, std::uint16_t value) { append(bytes, value); }

void append_vec3(std::vector<std::uint8_t>& bytes, float x, float y, float z) {
    append_float(bytes, x); append_float(bytes, y); append_float(bytes, z);
}

void append_vec4(std::vector<std::uint8_t>& bytes, float x, float y, float z, float w) {
    append_float(bytes, x); append_float(bytes, y); append_float(bytes, z); append_float(bytes, w);
}

void append_identity(std::vector<std::uint8_t>& bytes, float translation_y = 0.0f) {
    for (int i = 0; i < 16; ++i) {
        float value = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        if (i == 13) value = translation_y;
        append_float(bytes, value);
    }
}

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

class FakeRenderer final : public seed::IRenderer {
public:
    bool initialize(void*, const seed::RendererConfig& config) override {
        m_width = config.width;
        m_height = config.height;
        return true;
    }
    void resize(std::uint32_t width, std::uint32_t height) override { m_width = width; m_height = height; }
    std::uint32_t width() const noexcept override { return m_width; }
    std::uint32_t height() const noexcept override { return m_height; }

    seed::ShaderHandle create_shader(const seed::ShaderDesc&) override { return {++m_next}; }
    void destroy_shader(seed::ShaderHandle) override {}
    seed::MeshHandle create_mesh(const seed::MeshDesc&) override { return {++m_next}; }
    void destroy_mesh(seed::MeshHandle) override {}
    seed::TextureHandle create_texture(const seed::TextureDesc&) override { return {++m_next}; }
    void destroy_texture(seed::TextureHandle) override {}

    void begin_frame() override {}
    void draw_sky(const seed::SkySettings&) override {}
    bool begin_shadow_pass(const seed::Mat4&) override { return false; }
    void draw_shadow_mesh(seed::MeshHandle, const seed::Mat4&) override {}
    void end_shadow_pass() override {}
    void draw_mesh(
        seed::MeshHandle,
        seed::ShaderHandle,
        const seed::MaterialTextures&,
        const seed::Vec4&,
        const seed::MaterialSurface&,
        bool,
        const seed::Mat4&,
        const seed::Mat4&,
        const seed::SceneLighting&
    ) override {}
    void end_frame() override {}
    std::string_view backend_name() const override { return "Fake"; }
    void shutdown() override {}

private:
    std::uint32_t m_width{1};
    std::uint32_t m_height{1};
    std::uint32_t m_next{0};
};

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_animated_asset_import_test";
    const auto source = root / "Character.gltf";
    const auto binary_file = root / "character.bin";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(264);

    append_vec3(bytes, -0.5f, 0.0f, 0.0f);
    append_vec3(bytes,  0.5f, 0.0f, 0.0f);
    append_vec3(bytes,  0.0f, 1.0f, 0.0f);
    assert(bytes.size() == 36);

    const std::uint8_t joints[12] = {
        0,0,0,0,
        0,0,0,0,
        1,0,0,0,
    };
    bytes.insert(bytes.end(), std::begin(joints), std::end(joints));
    assert(bytes.size() == 48);

    for (int i = 0; i < 3; ++i) append_vec4(bytes, 1.0f, 0.0f, 0.0f, 0.0f);
    assert(bytes.size() == 96);

    append_u16(bytes, 0); append_u16(bytes, 1); append_u16(bytes, 2);
    bytes.push_back(0); bytes.push_back(0);
    assert(bytes.size() == 104);

    append_identity(bytes, 0.0f);
    append_identity(bytes, -1.0f);
    assert(bytes.size() == 232);

    append_float(bytes, 0.0f); append_float(bytes, 1.0f);
    append_vec3(bytes, 0.0f, 0.0f, 0.0f);
    append_vec3(bytes, 1.0f, 0.0f, 0.0f);
    assert(bytes.size() == 264);

    {
        std::ofstream output(binary_file, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    const char* gltf = R"JSON({
  "asset": {"version": "2.0"},
  "scene": 0,
  "scenes": [{"nodes": [0, 2]}],
  "buffers": [{"byteLength": 264, "uri": "character.bin"}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0,   "byteLength": 36,  "target": 34962},
    {"buffer": 0, "byteOffset": 36,  "byteLength": 12,  "target": 34962},
    {"buffer": 0, "byteOffset": 48,  "byteLength": 48,  "target": 34962},
    {"buffer": 0, "byteOffset": 96,  "byteLength": 6,   "target": 34963},
    {"buffer": 0, "byteOffset": 104, "byteLength": 128},
    {"buffer": 0, "byteOffset": 232, "byteLength": 8},
    {"buffer": 0, "byteOffset": 240, "byteLength": 24}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5121, "count": 3, "type": "VEC4"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC4"},
    {"bufferView": 3, "componentType": 5123, "count": 3, "type": "SCALAR"},
    {"bufferView": 4, "componentType": 5126, "count": 2, "type": "MAT4"},
    {"bufferView": 5, "componentType": 5126, "count": 2, "type": "SCALAR"},
    {"bufferView": 6, "componentType": 5126, "count": 2, "type": "VEC3"}
  ],
  "nodes": [
    {"name": "Root", "children": [1]},
    {"name": "Child", "translation": [0, 1, 0]},
    {"name": "Character", "mesh": 0, "skin": 0}
  ],
  "skins": [{
    "name": "Seed Test Skeleton",
    "inverseBindMatrices": 4,
    "skeleton": 0,
    "joints": [0, 1]
  }],
  "meshes": [{
    "name": "Seed Animated Triangle",
    "primitives": [{
      "attributes": {"POSITION": 0, "JOINTS_0": 1, "WEIGHTS_0": 2},
      "indices": 3,
      "mode": 4
    }]
  }],
  "animations": [{
    "name": "Move Root",
    "samplers": [{"input": 5, "output": 6, "interpolation": "LINEAR"}],
    "channels": [{"sampler": 0, "target": {"node": 0, "path": "translation"}}]
  }]
})JSON";

    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << gltf;
    }

    assert(seed::AssetImporter::supports_animated_model(source));
    std::string error;
    const auto imported = seed::AssetImporter::import_animated_model(source, &error);
    if (!imported.has_value()) std::cerr << error << '\n';
    assert(imported.has_value());

    assert(imported->name == "Seed Animated Triangle");
    assert(imported->vertices.size() == 3);
    assert(imported->indices.size() == 3);
    assert(imported->vertices[0].joints[0] == 0);
    assert(imported->vertices[2].joints[0] == 1);
    assert(near(imported->vertices[0].weights[0], 1.0f));
    assert(near(imported->vertices[2].weights[0], 1.0f));
    assert(near(imported->vertices[0].normal[2], 1.0f));

    assert(imported->skeleton.name == "Seed Test Skeleton");
    assert(imported->skeleton.joints.size() == 2);
    assert(imported->skeleton.joints[0].name == "Root");
    assert(imported->skeleton.joints[0].parent == -1);
    assert(imported->skeleton.joints[1].name == "Child");
    assert(imported->skeleton.joints[1].parent == 0);
    assert(near(imported->skeleton.joints[1].bind_local.translation.y, 1.0f));
    assert(near(imported->skeleton.joints[1].inverse_bind_matrix.values[13], -1.0f));

    assert(imported->animations.size() == 1);
    const auto& clip = imported->animations[0];
    assert(clip.name == "Move Root");
    assert(near(clip.duration_seconds, 1.0f));
    assert(clip.tracks.size() == 1);
    assert(clip.tracks[0].joint_index == 0);
    assert(clip.tracks[0].translation_keys.size() == 2);
    assert(near(clip.tracks[0].translation_keys[1].value.x, 1.0f));

    seed::SkeletonPose pose;
    seed::AnimationSystem::evaluate_clip(imported->skeleton, clip, 0.5f, pose);
    assert(pose.global_matrices.size() == 2);
    assert(near(pose.global_matrices[0].values[12], 0.5f));
    assert(near(pose.global_matrices[1].values[12], 0.5f));
    assert(near(pose.global_matrices[1].values[13], 1.0f));

    assert(imported->external_files.size() == 1);
    assert(imported->external_files[0].generic_string() == "character.bin");

    FakeRenderer renderer;
    seed::AssetRuntime runtime(renderer);
    runtime.set_project_root(root);
    const std::string character_id = seed::AssetRuntime::make_character_asset_id("Character.gltf");
    assert(character_id == "character:Character.gltf");
    assert(seed::AssetRuntime::is_character_asset_id(character_id));
    assert(seed::AssetRuntime::character_asset_path(character_id).value() == std::filesystem::path{"Character.gltf"});

    error.clear();
    const seed::AnimatedModelAssetResource* runtime_character = runtime.load_character(character_id, &error);
    if (runtime_character == nullptr) std::cerr << error << '\n';
    assert(runtime_character != nullptr);
    assert(runtime_character->vertices.size() == 3);
    assert(runtime_character->indices.size() == 3);
    assert(runtime_character->skeleton.joints.size() == 2);
    assert(runtime_character->animations.size() == 1);
    assert(near(runtime_character->vertices[2].joints[0], 1.0f));
    assert(near(runtime_character->vertices[2].weights[0], 1.0f));

    // Runtime cache returns the same stable resource for repeat requests.
    assert(runtime.load_character(character_id, &error) == runtime_character);

    std::filesystem::remove_all(root);
    std::cout << "Seed animated glTF import + runtime asset tests passed.\n";
    return 0;
}
