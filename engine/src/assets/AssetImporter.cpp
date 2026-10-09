#include "seed/assets/AssetImporter.h"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>
#include <string_view>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
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
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return 2;
    case TINYGLTF_COMPONENT_TYPE_INT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return 4;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        return 8;
    default:
        return 0;
    }
}

std::size_t component_count(int type) {
    switch (type) {
    case TINYGLTF_TYPE_SCALAR: return 1;
    case TINYGLTF_TYPE_VEC2: return 2;
    case TINYGLTF_TYPE_VEC3: return 3;
    case TINYGLTF_TYPE_VEC4: return 4;
    default: return 0;
    }
}

const unsigned char* accessor_base(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    const tinygltf::BufferView** out_view = nullptr
) {
    if (accessor.bufferView < 0 ||
        static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size()) {
        return nullptr;
    }

    const auto& view = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
    if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= model.buffers.size()) {
        return nullptr;
    }

    const auto& buffer = model.buffers[static_cast<std::size_t>(view.buffer)];
    const std::size_t offset = view.byteOffset + accessor.byteOffset;
    if (offset >= buffer.data.size()) {
        return nullptr;
    }

    if (out_view != nullptr) {
        *out_view = &view;
    }
    return buffer.data.data() + offset;
}

std::size_t accessor_stride(const tinygltf::Accessor& accessor, const tinygltf::BufferView& view) {
    const int declared = accessor.ByteStride(view);
    if (declared > 0) {
        return static_cast<std::size_t>(declared);
    }
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
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return read_unaligned<float>(data);
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        return static_cast<float>(read_unaligned<double>(data));
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const auto value = read_unaligned<std::uint8_t>(data);
        return normalized ? static_cast<float>(value) / 255.0f : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_BYTE: {
        const auto value = read_unaligned<std::int8_t>(data);
        return normalized
            ? std::max(-1.0f, static_cast<float>(value) / 127.0f)
            : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        const auto value = read_unaligned<std::uint16_t>(data);
        return normalized ? static_cast<float>(value) / 65535.0f : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT: {
        const auto value = read_unaligned<std::int16_t>(data);
        return normalized
            ? std::max(-1.0f, static_cast<float>(value) / 32767.0f)
            : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        const auto value = read_unaligned<std::uint32_t>(data);
        return normalized
            ? static_cast<float>(static_cast<double>(value) / 4294967295.0)
            : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_INT:
        return static_cast<float>(read_unaligned<std::int32_t>(data));
    default:
        return 0.0f;
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
    if (component_count(accessor.type) < expected_components) {
        set_error(error, "glTF attribute does not contain enough components.");
        return false;
    }

    const tinygltf::BufferView* view = nullptr;
    const unsigned char* base = accessor_base(model, accessor, &view);
    if (base == nullptr || view == nullptr) {
        set_error(error, "Sparse or missing glTF attribute buffers are not supported in Seed import v0.");
        return false;
    }

    const std::size_t stride = accessor_stride(accessor, *view);
    const std::size_t element_component_size = component_size(accessor.componentType);
    if (stride == 0 || element_component_size == 0) {
        set_error(error, "Unsupported glTF attribute component format.");
        return false;
    }

    values.resize(accessor.count);
    for (std::size_t i = 0; i < accessor.count; ++i) {
        auto& output = values[i];
        output = {0.0f, 0.0f, 0.0f, 1.0f};
        const unsigned char* element = base + i * stride;
        for (std::size_t component = 0; component < expected_components; ++component) {
            output[component] = read_component_as_float(
                element + component * element_component_size,
                accessor.componentType,
                accessor.normalized
            );
        }
    }
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
        for (std::size_t i = 0; i < vertex_count; ++i) {
            indices[i] = static_cast<std::uint32_t>(i);
        }
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
        set_error(error, "Sparse or missing glTF index buffers are not supported in Seed import v0.");
        return false;
    }

    const std::size_t stride = accessor_stride(accessor, *view);
    indices.resize(accessor.count);
    for (std::size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* element = base + i * stride;
        switch (accessor.componentType) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            indices[i] = read_unaligned<std::uint8_t>(element);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            indices[i] = read_unaligned<std::uint16_t>(element);
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            indices[i] = read_unaligned<std::uint32_t>(element);
            break;
        default:
            set_error(error, "Seed import v0 supports unsigned byte/short/int glTF indices.");
            return false;
        }
    }
    return true;
}

void generate_missing_normals(ImportedModelData& model) {
    std::vector<Vec3> accumulated(model.vertices.size());
    for (std::size_t i = 0; i + 2 < model.indices.size(); i += 3) {
        const std::uint32_t i0 = model.indices[i];
        const std::uint32_t i1 = model.indices[i + 1];
        const std::uint32_t i2 = model.indices[i + 2];
        if (i0 >= model.vertices.size() || i1 >= model.vertices.size() || i2 >= model.vertices.size()) {
            continue;
        }

        const auto& p0a = model.vertices[i0].position;
        const auto& p1a = model.vertices[i1].position;
        const auto& p2a = model.vertices[i2].position;
        const Vec3 p0{p0a[0], p0a[1], p0a[2]};
        const Vec3 p1{p1a[0], p1a[1], p1a[2]};
        const Vec3 p2{p2a[0], p2a[1], p2a[2]};
        const Vec3 face = cross(p1 - p0, p2 - p0);
        accumulated[i0] += face;
        accumulated[i1] += face;
        accumulated[i2] += face;
    }

    for (std::size_t i = 0; i < model.vertices.size(); ++i) {
        Vec3 normal = normalize(accumulated[i]);
        if (length(normal) <= 0.000001f) {
            normal = {0.0f, 1.0f, 0.0f};
        }
        model.vertices[i].normal = {normal.x, normal.y, normal.z};
    }
}

std::optional<ImportedTextureData> convert_image(const tinygltf::Image& image, std::string* error) {
    if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
        return std::nullopt;
    }
    if (image.bits != 8) {
        set_error(error, "Seed import v0 currently supports 8-bit base-color textures.");
        return std::nullopt;
    }

    const int components = std::clamp(image.component, 1, 4);
    const std::size_t pixel_count = static_cast<std::size_t>(image.width) *
        static_cast<std::size_t>(image.height);
    if (image.image.size() < pixel_count * static_cast<std::size_t>(components)) {
        set_error(error, "Decoded glTF image data is incomplete.");
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
            destination[0] = source[0];
            destination[1] = source[0];
            destination[2] = source[0];
            destination[3] = 255;
        } else if (components == 2) {
            destination[0] = source[0];
            destination[1] = source[0];
            destination[2] = source[0];
            destination[3] = source[1];
        } else if (components == 3) {
            destination[0] = source[0];
            destination[1] = source[1];
            destination[2] = source[2];
            destination[3] = 255;
        } else {
            destination[0] = source[0];
            destination[1] = source[1];
            destination[2] = source[2];
            destination[3] = source[3];
        }
    }
    return result;
}

bool is_external_relative_uri(const std::string& uri) {
    if (uri.empty() || std::string_view{uri}.starts_with("data:")) {
        return false;
    }
    const std::filesystem::path path{uri};
    return path.is_relative() && uri.find("://") == std::string::npos;
}

} // namespace

bool AssetImporter::supports_static_model(const std::filesystem::path& source_file) {
    const std::string extension = lower_extension(source_file);
    return extension == ".gltf" || extension == ".glb";
}

std::optional<ImportedModelData> AssetImporter::import_static_model(
    const std::filesystem::path& source_file,
    std::string* error
) {
    if (!supports_static_model(source_file)) {
        set_error(error, "Seed import v0 supports .gltf and .glb static models.");
        return std::nullopt;
    }
    if (!std::filesystem::exists(source_file)) {
        set_error(error, "Model file does not exist: " + source_file.string());
        return std::nullopt;
    }

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warning;
    std::string loader_error;

    const bool loaded = lower_extension(source_file) == ".glb"
        ? loader.LoadBinaryFromFile(&model, &loader_error, &warning, source_file.string())
        : loader.LoadASCIIFromFile(&model, &loader_error, &warning, source_file.string());

    if (!loaded) {
        std::string message = "Could not import glTF model.";
        if (!loader_error.empty()) {
            message += " " + loader_error;
        }
        set_error(error, std::move(message));
        return std::nullopt;
    }

    const tinygltf::Primitive* primitive = nullptr;
    const tinygltf::Mesh* source_mesh = nullptr;
    for (const auto& mesh : model.meshes) {
        for (const auto& candidate : mesh.primitives) {
            if (candidate.mode == TINYGLTF_MODE_TRIANGLES || candidate.mode == -1) {
                primitive = &candidate;
                source_mesh = &mesh;
                break;
            }
        }
        if (primitive != nullptr) {
            break;
        }
    }

    if (primitive == nullptr || source_mesh == nullptr) {
        set_error(error, "No triangle primitive was found in the glTF model.");
        return std::nullopt;
    }

    const auto position_attribute = primitive->attributes.find("POSITION");
    if (position_attribute == primitive->attributes.end()) {
        set_error(error, "The first glTF primitive has no POSITION attribute.");
        return std::nullopt;
    }

    std::vector<std::array<float, 4>> positions;
    if (!read_attribute(model, position_attribute->second, 3, positions, error)) {
        return std::nullopt;
    }

    std::vector<std::array<float, 4>> colors;
    if (const auto found = primitive->attributes.find("COLOR_0"); found != primitive->attributes.end()) {
        const auto& accessor = model.accessors[static_cast<std::size_t>(found->second)];
        const std::size_t count = component_count(accessor.type) >= 4 ? 4u : 3u;
        if (!read_attribute(model, found->second, count, colors, error)) {
            return std::nullopt;
        }
    }

    std::vector<std::array<float, 4>> texcoords;
    if (const auto found = primitive->attributes.find("TEXCOORD_0"); found != primitive->attributes.end()) {
        if (!read_attribute(model, found->second, 2, texcoords, error)) {
            return std::nullopt;
        }
    }

    std::vector<std::array<float, 4>> normals;
    if (const auto found = primitive->attributes.find("NORMAL"); found != primitive->attributes.end()) {
        if (!read_attribute(model, found->second, 3, normals, error)) {
            return std::nullopt;
        }
    }

    ImportedModelData result;
    result.name = source_mesh->name.empty() ? source_file.stem().string() : source_mesh->name;
    result.vertices.resize(positions.size());

    for (std::size_t i = 0; i < positions.size(); ++i) {
        auto& vertex = result.vertices[i];
        vertex.position = {positions[i][0], positions[i][1], positions[i][2]};
        if (i < colors.size()) {
            vertex.color = {colors[i][0], colors[i][1], colors[i][2]};
        } else {
            vertex.color = {1.0f, 1.0f, 1.0f};
        }
        if (i < texcoords.size()) {
            vertex.texcoord = {texcoords[i][0], texcoords[i][1]};
        }
        if (i < normals.size()) {
            const Vec3 normal = normalize({normals[i][0], normals[i][1], normals[i][2]});
            vertex.normal = {normal.x, normal.y, normal.z};
        }
    }

    if (!read_indices(model, primitive->indices, result.indices, result.vertices.size(), error)) {
        return std::nullopt;
    }

    if (normals.empty()) {
        generate_missing_normals(result);
    }

    if (primitive->material >= 0 &&
        static_cast<std::size_t>(primitive->material) < model.materials.size()) {
        const auto& material = model.materials[static_cast<std::size_t>(primitive->material)];
        const auto& pbr = material.pbrMetallicRoughness;
        if (pbr.baseColorFactor.size() >= 4) {
            result.base_color = {
                static_cast<float>(pbr.baseColorFactor[0]),
                static_cast<float>(pbr.baseColorFactor[1]),
                static_cast<float>(pbr.baseColorFactor[2]),
                static_cast<float>(pbr.baseColorFactor[3]),
            };
        }

        const int texture_index = pbr.baseColorTexture.index;
        if (texture_index >= 0 && static_cast<std::size_t>(texture_index) < model.textures.size()) {
            const auto& texture = model.textures[static_cast<std::size_t>(texture_index)];
            if (texture.source >= 0 && static_cast<std::size_t>(texture.source) < model.images.size()) {
                std::string image_error;
                auto converted = convert_image(model.images[static_cast<std::size_t>(texture.source)], &image_error);
                if (!image_error.empty() && !converted.has_value()) {
                    set_error(error, image_error);
                    return std::nullopt;
                }
                result.base_color_texture = std::move(converted);
            }
        }
    }

    std::set<std::filesystem::path> dependencies;
    for (const auto& buffer : model.buffers) {
        if (is_external_relative_uri(buffer.uri)) {
            dependencies.emplace(buffer.uri);
        }
    }
    for (const auto& image : model.images) {
        if (is_external_relative_uri(image.uri)) {
            dependencies.emplace(image.uri);
        }
    }
    result.external_files.assign(dependencies.begin(), dependencies.end());

    if (result.vertices.empty() || result.indices.empty()) {
        set_error(error, "Imported glTF primitive is empty.");
        return std::nullopt;
    }

    return result;
}

} // namespace seed
