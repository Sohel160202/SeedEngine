#include "seed/assets/AssetImporter.h"

#include "seed/animation/AnimationSystem.h"

#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>
#include <string_view>
#include <unordered_map>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

std::string lower_extension(std::filesystem::path path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return extension;
}

std::size_t component_size(int component_type) {
    switch (component_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return 2;
    case TINYGLTF_COMPONENT_TYPE_INT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT: return 4;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE: return 8;
    default: return 0;
    }
}

std::size_t component_count(int type) {
    switch (type) {
    case TINYGLTF_TYPE_SCALAR: return 1;
    case TINYGLTF_TYPE_VEC2: return 2;
    case TINYGLTF_TYPE_VEC3: return 3;
    case TINYGLTF_TYPE_VEC4: return 4;
    case TINYGLTF_TYPE_MAT4: return 16;
    default: return 0;
    }
}

const unsigned char* accessor_base(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    const tinygltf::BufferView** out_view = nullptr
) {
    if (accessor.bufferView < 0 ||
        static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size()) return nullptr;
    const auto& view = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= model.buffers.size()) return nullptr;
    const auto& buffer = model.buffers[static_cast<std::size_t>(view.buffer)];
    const std::size_t offset = view.byteOffset + accessor.byteOffset;
    if (offset >= buffer.data.size()) return nullptr;
    if (out_view != nullptr) *out_view = &view;
    return buffer.data.data() + offset;
}

std::size_t accessor_stride(const tinygltf::Accessor& accessor, const tinygltf::BufferView& view) {
    const int declared = accessor.ByteStride(view);
    if (declared > 0) return static_cast<std::size_t>(declared);
    return component_size(accessor.componentType) * component_count(accessor.type);
}

template <typename T>
T read_unaligned(const unsigned char* data) {
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}

float read_component_as_float(const unsigned char* data, int component_type, bool normalized) {
    switch (component_type) {
    case TINYGLTF_COMPONENT_TYPE_FLOAT: return read_unaligned<float>(data);
    case TINYGLTF_COMPONENT_TYPE_DOUBLE: return static_cast<float>(read_unaligned<double>(data));
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const auto v = read_unaligned<std::uint8_t>(data);
        return normalized ? static_cast<float>(v) / 255.0f : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_BYTE: {
        const auto v = read_unaligned<std::int8_t>(data);
        return normalized ? std::max(-1.0f, static_cast<float>(v) / 127.0f) : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        const auto v = read_unaligned<std::uint16_t>(data);
        return normalized ? static_cast<float>(v) / 65535.0f : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT: {
        const auto v = read_unaligned<std::int16_t>(data);
        return normalized ? std::max(-1.0f, static_cast<float>(v) / 32767.0f) : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        const auto v = read_unaligned<std::uint32_t>(data);
        return normalized
            ? static_cast<float>(static_cast<double>(v) / 4294967295.0)
            : static_cast<float>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_INT: return static_cast<float>(read_unaligned<std::int32_t>(data));
    default: return 0.0f;
    }
}

bool read_attribute(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_components,
    std::vector<std::array<float, 4>>& values,
    std::string* error
) {
    if (accessor_index < 0 || static_cast<std::size_t>(accessor_index) >= model.accessors.size()) {
        set_error(error, "glTF attribute accessor is invalid.");
        return false;
    }
    const auto& accessor = model.accessors[static_cast<std::size_t>(accessor_index)];
    if (expected_components > 4 || component_count(accessor.type) < expected_components) {
        set_error(error, "glTF attribute does not contain the expected components.");
        return false;
    }
    const tinygltf::BufferView* view = nullptr;
    const unsigned char* base = accessor_base(model, accessor, &view);
    if (base == nullptr || view == nullptr) {
        set_error(error, "Sparse or missing glTF attribute buffers are not supported for animated import yet.");
        return false;
    }
    const std::size_t stride = accessor_stride(accessor, *view);
    const std::size_t size = component_size(accessor.componentType);
    if (stride == 0 || size == 0) {
        set_error(error, "Unsupported glTF attribute component format.");
        return false;
    }
    values.resize(accessor.count);
    for (std::size_t i = 0; i < accessor.count; ++i) {
        values[i] = {0.0f, 0.0f, 0.0f, 1.0f};
        const unsigned char* element = base + i * stride;
        for (std::size_t c = 0; c < expected_components; ++c) {
            values[i][c] = read_component_as_float(
                element + c * size, accessor.componentType, accessor.normalized
            );
        }
    }
    return true;
}

bool read_scalar_times(
    const tinygltf::Model& model,
    int accessor_index,
    std::vector<float>& values,
    std::string* error
) {
    std::vector<std::array<float, 4>> raw;
    if (!read_attribute(model, accessor_index, 1, raw, error)) return false;
    values.resize(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) values[i] = raw[i][0];
    return true;
}

bool read_indices(
    const tinygltf::Model& model,
    int accessor_index,
    std::vector<std::uint32_t>& indices,
    std::size_t vertex_count,
    std::string* error
) {
    if (accessor_index < 0) {
        indices.resize(vertex_count);
        for (std::size_t i = 0; i < vertex_count; ++i) indices[i] = static_cast<std::uint32_t>(i);
        return true;
    }
    if (static_cast<std::size_t>(accessor_index) >= model.accessors.size()) {
        set_error(error, "glTF index accessor is invalid.");
        return false;
    }
    const auto& accessor = model.accessors[static_cast<std::size_t>(accessor_index)];
    if (accessor.type != TINYGLTF_TYPE_SCALAR) {
        set_error(error, "glTF index accessor must be scalar.");
        return false;
    }
    const tinygltf::BufferView* view = nullptr;
    const unsigned char* base = accessor_base(model, accessor, &view);
    if (base == nullptr || view == nullptr) {
        set_error(error, "Sparse animated glTF indices are not supported yet.");
        return false;
    }
    const std::size_t stride = accessor_stride(accessor, *view);
    indices.resize(accessor.count);
    for (std::size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* element = base + i * stride;
        switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indices[i] = read_unaligned<std::uint8_t>(element); break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indices[i] = read_unaligned<std::uint16_t>(element); break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: indices[i] = read_unaligned<std::uint32_t>(element); break;
        default:
            set_error(error, "Seed animated import supports unsigned byte/short/int indices.");
            return false;
        }
    }
    return true;
}

bool read_inverse_bind_matrices(
    const tinygltf::Model& model,
    int accessor_index,
    std::vector<Mat4>& matrices,
    std::string* error
) {
    if (accessor_index < 0) return true;
    if (static_cast<std::size_t>(accessor_index) >= model.accessors.size()) {
        set_error(error, "Skin inverse bind accessor is invalid.");
        return false;
    }
    const auto& accessor = model.accessors[static_cast<std::size_t>(accessor_index)];
    if (accessor.type != TINYGLTF_TYPE_MAT4 || accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
        set_error(error, "Seed animated import currently expects FLOAT MAT4 inverse bind matrices.");
        return false;
    }
    const tinygltf::BufferView* view = nullptr;
    const unsigned char* base = accessor_base(model, accessor, &view);
    if (base == nullptr || view == nullptr) {
        set_error(error, "Sparse inverse bind matrices are not supported yet.");
        return false;
    }
    const std::size_t stride = accessor_stride(accessor, *view);
    matrices.resize(accessor.count, Mat4::identity());
    for (std::size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* element = base + i * stride;
        for (std::size_t component = 0; component < 16; ++component) {
            matrices[i].values[component] = read_unaligned<float>(element + component * sizeof(float));
        }
    }
    return true;
}

Quaternion quaternion_multiply(const Quaternion& a, const Quaternion& b) {
    return {
        a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
        a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
        a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
        a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z,
    };
}

Quaternion quaternion_from_euler(const Vec3& euler_degrees) {
    const float hx = radians(euler_degrees.x) * 0.5f;
    const float hy = radians(euler_degrees.y) * 0.5f;
    const float hz = radians(euler_degrees.z) * 0.5f;
    const Quaternion qx{std::sin(hx), 0.0f, 0.0f, std::cos(hx)};
    const Quaternion qy{0.0f, std::sin(hy), 0.0f, std::cos(hy)};
    const Quaternion qz{0.0f, 0.0f, std::sin(hz), std::cos(hz)};
    return AnimationSystem::normalize_quaternion(
        quaternion_multiply(qz, quaternion_multiply(qy, qx))
    );
}

JointTransform node_local_transform(const tinygltf::Node& node) {
    JointTransform result;
    if (node.translation.size() >= 3) {
        result.translation = {
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2]),
        };
    }
    if (node.rotation.size() >= 4) {
        result.rotation = AnimationSystem::normalize_quaternion({
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]),
            static_cast<float>(node.rotation[3]),
        });
    }
    if (node.scale.size() >= 3) {
        result.scale = {
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2]),
        };
    }

    if (node.matrix.size() == 16) {
        Mat4 matrix{};
        for (std::size_t i = 0; i < 16; ++i) matrix.values[i] = static_cast<float>(node.matrix[i]);
        Vec3 position{};
        Vec3 rotation{};
        Vec3 scale{1.0f, 1.0f, 1.0f};
        if (decompose_transform_matrix(matrix, position, rotation, scale)) {
            result.translation = position;
            result.rotation = quaternion_from_euler(rotation);
            result.scale = scale;
        }
    }
    return result;
}

std::optional<ImportedTextureData> convert_image(const tinygltf::Image& image, std::string* error) {
    if (image.width <= 0 || image.height <= 0 || image.image.empty()) return std::nullopt;
    if (image.bits != 8) {
        set_error(error, "Seed animated import currently supports 8-bit material textures.");
        return std::nullopt;
    }
    const int components = std::clamp(image.component, 1, 4);
    const std::size_t pixel_count = static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height);
    if (image.image.size() < pixel_count * static_cast<std::size_t>(components)) {
        set_error(error, "Decoded animated glTF image data is incomplete.");
        return std::nullopt;
    }
    ImportedTextureData result;
    result.width = static_cast<std::uint32_t>(image.width);
    result.height = static_cast<std::uint32_t>(image.height);
    result.rgba8_pixels.resize(pixel_count * 4u);
    for (std::size_t pixel = 0; pixel < pixel_count; ++pixel) {
        const auto* source = image.image.data() + pixel * static_cast<std::size_t>(components);
        auto* destination = result.rgba8_pixels.data() + pixel * 4u;
        if (components == 1) {
            destination[0] = destination[1] = destination[2] = source[0]; destination[3] = 255;
        } else if (components == 2) {
            destination[0] = destination[1] = destination[2] = source[0]; destination[3] = source[1];
        } else if (components == 3) {
            destination[0] = source[0]; destination[1] = source[1]; destination[2] = source[2]; destination[3] = 255;
        } else {
            destination[0] = source[0]; destination[1] = source[1]; destination[2] = source[2]; destination[3] = source[3];
        }
    }
    return result;
}

std::optional<ImportedTextureData> convert_texture(
    const tinygltf::Model& model,
    int texture_index,
    std::string* error
) {
    if (texture_index < 0) return std::nullopt;
    if (static_cast<std::size_t>(texture_index) >= model.textures.size()) {
        set_error(error, "Animated glTF material references an invalid texture.");
        return std::nullopt;
    }
    const auto& texture = model.textures[static_cast<std::size_t>(texture_index)];
    if (texture.source < 0 || static_cast<std::size_t>(texture.source) >= model.images.size()) {
        set_error(error, "Animated glTF texture references an invalid image.");
        return std::nullopt;
    }
    return convert_image(model.images[static_cast<std::size_t>(texture.source)], error);
}

bool is_external_relative_uri(const std::string& uri) {
    if (uri.empty() || std::string_view{uri}.starts_with("data:")) return false;
    const std::filesystem::path path{uri};
    return path.is_relative() && uri.find("://") == std::string::npos;
}

void collect_dependencies(const tinygltf::Model& model, std::vector<std::filesystem::path>& output) {
    std::set<std::filesystem::path> dependencies;
    for (const auto& buffer : model.buffers) {
        if (is_external_relative_uri(buffer.uri)) dependencies.emplace(buffer.uri);
    }
    for (const auto& image : model.images) {
        if (is_external_relative_uri(image.uri)) dependencies.emplace(image.uri);
    }
    output.assign(dependencies.begin(), dependencies.end());
}

bool load_model(const std::filesystem::path& source_file, tinygltf::Model& model, std::string* error) {
    if (!std::filesystem::exists(source_file)) {
        set_error(error, "Model file does not exist: " + source_file.string());
        return false;
    }
    tinygltf::TinyGLTF loader;
    std::string warning;
    std::string loader_error;
    const bool loaded = lower_extension(source_file) == ".glb"
        ? loader.LoadBinaryFromFile(&model, &loader_error, &warning, source_file.string())
        : loader.LoadASCIIFromFile(&model, &loader_error, &warning, source_file.string());
    if (!loaded) {
        std::string message = "Could not import animated glTF model.";
        if (!loader_error.empty()) message += " " + loader_error;
        set_error(error, std::move(message));
        return false;
    }
    return true;
}

void generate_missing_normals(ImportedAnimatedModelData& model) {
    std::vector<Vec3> accumulated(model.vertices.size());
    for (std::size_t i = 0; i + 2 < model.indices.size(); i += 3) {
        const auto i0 = model.indices[i];
        const auto i1 = model.indices[i + 1];
        const auto i2 = model.indices[i + 2];
        if (i0 >= model.vertices.size() || i1 >= model.vertices.size() || i2 >= model.vertices.size()) continue;
        const auto& a = model.vertices[i0].position;
        const auto& b = model.vertices[i1].position;
        const auto& c = model.vertices[i2].position;
        const Vec3 p0{a[0], a[1], a[2]};
        const Vec3 p1{b[0], b[1], b[2]};
        const Vec3 p2{c[0], c[1], c[2]};
        const Vec3 face = cross(p1 - p0, p2 - p0);
        accumulated[i0] += face; accumulated[i1] += face; accumulated[i2] += face;
    }
    for (std::size_t i = 0; i < model.vertices.size(); ++i) {
        Vec3 n = normalize(accumulated[i]);
        if (length(n) <= 0.000001f) n = {0.0f, 1.0f, 0.0f};
        model.vertices[i].normal = {n.x, n.y, n.z};
    }
}

bool build_skeleton(
    const tinygltf::Model& model,
    const tinygltf::Skin& skin,
    Skeleton& skeleton,
    std::unordered_map<int, std::uint32_t>& node_to_joint,
    std::string* error
) {
    if (skin.joints.empty()) {
        set_error(error, "glTF skin contains no joints.");
        return false;
    }

    skeleton.name = skin.name.empty() ? "Skeleton" : skin.name;
    skeleton.joints.resize(skin.joints.size());
    for (std::size_t i = 0; i < skin.joints.size(); ++i) {
        const int node_index = skin.joints[i];
        if (node_index < 0 || static_cast<std::size_t>(node_index) >= model.nodes.size()) {
            set_error(error, "glTF skin references an invalid joint node.");
            return false;
        }
        node_to_joint[node_index] = static_cast<std::uint32_t>(i);
    }

    std::vector<int> parent_node(model.nodes.size(), -1);
    for (std::size_t parent = 0; parent < model.nodes.size(); ++parent) {
        for (const int child : model.nodes[parent].children) {
            if (child >= 0 && static_cast<std::size_t>(child) < parent_node.size()) {
                parent_node[static_cast<std::size_t>(child)] = static_cast<int>(parent);
            }
        }
    }

    std::vector<Mat4> inverse_bind;
    if (!read_inverse_bind_matrices(model, skin.inverseBindMatrices, inverse_bind, error)) return false;
    if (!inverse_bind.empty() && inverse_bind.size() < skin.joints.size()) {
        set_error(error, "glTF skin has fewer inverse bind matrices than joints.");
        return false;
    }

    for (std::size_t i = 0; i < skin.joints.size(); ++i) {
        const int node_index = skin.joints[i];
        const auto& node = model.nodes[static_cast<std::size_t>(node_index)];
        auto& joint = skeleton.joints[i];
        joint.name = node.name.empty() ? "Joint " + std::to_string(i) : node.name;
        joint.bind_local = node_local_transform(node);
        joint.inverse_bind_matrix = inverse_bind.empty() ? Mat4::identity() : inverse_bind[i];

        const int parent = parent_node[static_cast<std::size_t>(node_index)];
        const auto found = node_to_joint.find(parent);
        joint.parent = found == node_to_joint.end() ? -1 : static_cast<std::int32_t>(found->second);
    }
    return true;
}

bool build_animations(
    const tinygltf::Model& model,
    const std::unordered_map<int, std::uint32_t>& node_to_joint,
    std::vector<AnimationClip>& clips,
    std::string* error
) {
    clips.reserve(model.animations.size());
    for (std::size_t animation_index = 0; animation_index < model.animations.size(); ++animation_index) {
        const auto& source = model.animations[animation_index];
        AnimationClip clip;
        clip.name = source.name.empty() ? "Animation " + std::to_string(animation_index) : source.name;
        clip.looping = true;
        std::unordered_map<std::uint32_t, std::size_t> track_indices;

        for (const auto& channel : source.channels) {
            const auto joint_found = node_to_joint.find(channel.target_node);
            if (joint_found == node_to_joint.end()) continue;
            if (channel.sampler < 0 || static_cast<std::size_t>(channel.sampler) >= source.samplers.size()) {
                set_error(error, "glTF animation channel references an invalid sampler.");
                return false;
            }
            const auto& sampler = source.samplers[static_cast<std::size_t>(channel.sampler)];
            const std::string interpolation = sampler.interpolation.empty() ? "LINEAR" : sampler.interpolation;
            if (interpolation != "LINEAR") {
                set_error(error, "Seed animated import v1 currently supports LINEAR animation samplers only.");
                return false;
            }

            std::vector<float> times;
            if (!read_scalar_times(model, sampler.input, times, error)) return false;
            if (times.empty()) continue;

            const std::uint32_t joint_index = joint_found->second;
            std::size_t track_index = 0;
            const auto existing = track_indices.find(joint_index);
            if (existing == track_indices.end()) {
                track_index = clip.tracks.size();
                track_indices.emplace(joint_index, track_index);
                clip.tracks.push_back({.joint_index = joint_index});
            } else {
                track_index = existing->second;
            }
            auto& track = clip.tracks[track_index];
            clip.duration_seconds = std::max(clip.duration_seconds, times.back());

            if (channel.target_path == "translation" || channel.target_path == "scale") {
                std::vector<std::array<float, 4>> values;
                if (!read_attribute(model, sampler.output, 3, values, error)) return false;
                if (values.size() != times.size()) {
                    set_error(error, "glTF animation time/value counts do not match.");
                    return false;
                }
                auto& keys = channel.target_path == "translation"
                    ? track.translation_keys
                    : track.scale_keys;
                keys.reserve(values.size());
                for (std::size_t i = 0; i < values.size(); ++i) {
                    keys.push_back({times[i], {values[i][0], values[i][1], values[i][2]}});
                }
            } else if (channel.target_path == "rotation") {
                std::vector<std::array<float, 4>> values;
                if (!read_attribute(model, sampler.output, 4, values, error)) return false;
                if (values.size() != times.size()) {
                    set_error(error, "glTF rotation animation time/value counts do not match.");
                    return false;
                }
                track.rotation_keys.reserve(values.size());
                for (std::size_t i = 0; i < values.size(); ++i) {
                    track.rotation_keys.push_back({
                        times[i],
                        AnimationSystem::normalize_quaternion({
                            values[i][0], values[i][1], values[i][2], values[i][3]
                        })
                    });
                }
            }
        }

        if (!clip.tracks.empty()) clips.push_back(std::move(clip));
    }
    return true;
}

} // namespace

bool AssetImporter::supports_animated_model(const std::filesystem::path& source_file) {
    const std::string extension = lower_extension(source_file);
    return extension == ".gltf" || extension == ".glb";
}

std::optional<ImportedAnimatedModelData> AssetImporter::import_animated_model(
    const std::filesystem::path& source_file,
    std::string* error
) {
    if (!supports_animated_model(source_file)) {
        set_error(error, "Seed animated import supports .gltf and .glb characters.");
        return std::nullopt;
    }

    tinygltf::Model model;
    if (!load_model(source_file, model, error)) return std::nullopt;

    int character_node_index = -1;
    const tinygltf::Primitive* primitive = nullptr;
    const tinygltf::Mesh* source_mesh = nullptr;
    const tinygltf::Skin* source_skin = nullptr;

    for (std::size_t node_index = 0; node_index < model.nodes.size() && primitive == nullptr; ++node_index) {
        const auto& node = model.nodes[node_index];
        if (node.mesh < 0 || node.skin < 0 ||
            static_cast<std::size_t>(node.mesh) >= model.meshes.size() ||
            static_cast<std::size_t>(node.skin) >= model.skins.size()) continue;
        const auto& mesh = model.meshes[static_cast<std::size_t>(node.mesh)];
        for (const auto& candidate : mesh.primitives) {
            const bool triangles = candidate.mode == TINYGLTF_MODE_TRIANGLES || candidate.mode == -1;
            if (!triangles || !candidate.attributes.contains("POSITION") ||
                !candidate.attributes.contains("JOINTS_0") ||
                !candidate.attributes.contains("WEIGHTS_0")) continue;
            character_node_index = static_cast<int>(node_index);
            primitive = &candidate;
            source_mesh = &mesh;
            source_skin = &model.skins[static_cast<std::size_t>(node.skin)];
            break;
        }
    }

    if (primitive == nullptr || source_mesh == nullptr || source_skin == nullptr) {
        set_error(error, "No skinned triangle primitive with JOINTS_0/WEIGHTS_0 was found.");
        return std::nullopt;
    }
    (void)character_node_index;

    std::vector<std::array<float, 4>> positions;
    std::vector<std::array<float, 4>> joints;
    std::vector<std::array<float, 4>> weights;
    if (!read_attribute(model, primitive->attributes.at("POSITION"), 3, positions, error) ||
        !read_attribute(model, primitive->attributes.at("JOINTS_0"), 4, joints, error) ||
        !read_attribute(model, primitive->attributes.at("WEIGHTS_0"), 4, weights, error)) {
        return std::nullopt;
    }
    if (positions.size() != joints.size() || positions.size() != weights.size()) {
        set_error(error, "Animated glTF POSITION/JOINTS_0/WEIGHTS_0 vertex counts do not match.");
        return std::nullopt;
    }

    std::vector<std::array<float, 4>> colors;
    if (const auto found = primitive->attributes.find("COLOR_0"); found != primitive->attributes.end()) {
        const auto& accessor = model.accessors[static_cast<std::size_t>(found->second)];
        const std::size_t count = component_count(accessor.type) >= 4 ? 4u : 3u;
        if (!read_attribute(model, found->second, count, colors, error)) return std::nullopt;
    }
    std::vector<std::array<float, 4>> texcoords;
    if (const auto found = primitive->attributes.find("TEXCOORD_0"); found != primitive->attributes.end()) {
        if (!read_attribute(model, found->second, 2, texcoords, error)) return std::nullopt;
    }
    std::vector<std::array<float, 4>> normals;
    if (const auto found = primitive->attributes.find("NORMAL"); found != primitive->attributes.end()) {
        if (!read_attribute(model, found->second, 3, normals, error)) return std::nullopt;
    }

    ImportedAnimatedModelData result;
    result.name = source_mesh->name.empty() ? source_file.stem().string() : source_mesh->name;
    result.vertices.resize(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i) {
        auto& vertex = result.vertices[i];
        vertex.position = {positions[i][0], positions[i][1], positions[i][2]};
        if (i < colors.size()) vertex.color = {colors[i][0], colors[i][1], colors[i][2]};
        if (i < texcoords.size()) vertex.texcoord = {texcoords[i][0], texcoords[i][1]};
        if (i < normals.size()) {
            const Vec3 n = normalize({normals[i][0], normals[i][1], normals[i][2]});
            vertex.normal = {n.x, n.y, n.z};
        }

        float weight_sum = 0.0f;
        for (std::size_t influence = 0; influence < 4; ++influence) {
            const float joint_value = std::max(0.0f, joints[i][influence]);
            vertex.joints[influence] = static_cast<std::uint16_t>(
                std::min(joint_value, static_cast<float>(std::numeric_limits<std::uint16_t>::max()))
            );
            vertex.weights[influence] = std::max(0.0f, weights[i][influence]);
            weight_sum += vertex.weights[influence];
        }
        if (weight_sum > 0.000001f) {
            for (float& weight : vertex.weights) weight /= weight_sum;
        } else {
            vertex.weights = {1.0f, 0.0f, 0.0f, 0.0f};
            vertex.joints = {0, 0, 0, 0};
        }
    }

    if (!read_indices(model, primitive->indices, result.indices, result.vertices.size(), error)) {
        return std::nullopt;
    }
    if (normals.empty()) generate_missing_normals(result);

    std::unordered_map<int, std::uint32_t> node_to_joint;
    if (!build_skeleton(model, *source_skin, result.skeleton, node_to_joint, error)) return std::nullopt;
    for (const auto& vertex : result.vertices) {
        for (std::size_t influence = 0; influence < 4; ++influence) {
            if (vertex.weights[influence] > 0.000001f && vertex.joints[influence] >= result.skeleton.joints.size()) {
                set_error(error, "Animated glTF vertex references a joint outside the active skin.");
                return std::nullopt;
            }
        }
    }

    if (!build_animations(model, node_to_joint, result.animations, error)) return std::nullopt;

    if (primitive->material >= 0 && static_cast<std::size_t>(primitive->material) < model.materials.size()) {
        const auto& material = model.materials[static_cast<std::size_t>(primitive->material)];
        const auto& pbr = material.pbrMetallicRoughness;
        if (pbr.baseColorFactor.size() >= 4) {
            result.base_color = {
                static_cast<float>(pbr.baseColorFactor[0]), static_cast<float>(pbr.baseColorFactor[1]),
                static_cast<float>(pbr.baseColorFactor[2]), static_cast<float>(pbr.baseColorFactor[3])
            };
        }
        result.metallic = std::clamp(static_cast<float>(pbr.metallicFactor), 0.0f, 1.0f);
        result.roughness = std::clamp(static_cast<float>(pbr.roughnessFactor), 0.04f, 1.0f);
        result.normal_scale = std::clamp(static_cast<float>(material.normalTexture.scale), 0.0f, 4.0f);

        std::string texture_error;
        result.base_color_texture = convert_texture(model, pbr.baseColorTexture.index, &texture_error);
        if (!texture_error.empty()) { set_error(error, texture_error); return std::nullopt; }
        texture_error.clear();
        result.metallic_roughness_texture = convert_texture(model, pbr.metallicRoughnessTexture.index, &texture_error);
        if (!texture_error.empty()) { set_error(error, texture_error); return std::nullopt; }
        texture_error.clear();
        result.normal_texture = convert_texture(model, material.normalTexture.index, &texture_error);
        if (!texture_error.empty()) { set_error(error, texture_error); return std::nullopt; }
    }

    collect_dependencies(model, result.external_files);
    if (result.vertices.empty() || result.indices.empty() || result.skeleton.joints.empty()) {
        set_error(error, "Imported animated glTF character is incomplete.");
        return std::nullopt;
    }
    return result;
}

} // namespace seed
