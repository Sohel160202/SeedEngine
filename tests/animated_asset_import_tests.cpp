#include "seed/animation/AnimationSystem.h"
#include "seed/assets/AssetImporter.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
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

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_animated_asset_import_test";
    const auto source = root / "Character.gltf";
    const auto binary_file = root / "character.bin";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    std::vector<std::uint8_t> bytes;
    bytes.reserve(264);

    // POSITION: three vertices.
    append_vec3(bytes, -0.5f, 0.0f, 0.0f);
    append_vec3(bytes,  0.5f, 0.0f, 0.0f);
    append_vec3(bytes,  0.0f, 1.0f, 0.0f);
    assert(bytes.size() == 36);

    // JOINTS_0: first two vertices use Root, top vertex uses Child.
    const std::uint8_t joints[12] = {
        0,0,0,0,
        0,0,0,0,
        1,0,0,0,
    };
    bytes.insert(bytes.end(), std::begin(joints), std::end(joints));
    assert(bytes.size() == 48);

    // WEIGHTS_0: one full influence per vertex.
    for (int i = 0; i < 3; ++i) append_vec4(bytes, 1.0f, 0.0f, 0.0f, 0.0f);
    assert(bytes.size() == 96);

    // Triangle indices + 2-byte alignment padding.
    append_u16(bytes, 0); append_u16(bytes, 1); append_u16(bytes, 2);
    bytes.push_back(0); bytes.push_back(0);
    assert(bytes.size() == 104);

    // Inverse bind matrices. Child bind pose is translated +1 Y, so inverse is -1 Y.
    append_identity(bytes, 0.0f);
    append_identity(bytes, -1.0f);
    assert(bytes.size() == 232);

    // Animation input times and Root translation outputs.
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

    std::filesystem::remove_all(root);
    std::cout << "Seed animated glTF import tests passed.\n";
    return 0;
}
