#include "seed/assets/AssetImporter.h"
#include "seed/assets/AssetRuntime.h"
#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool nearly_equal(float a, float b, float epsilon = 0.0001f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_asset_import_test";
    const auto source = root / "Triangle.gltf";
    const auto scene_file = root / "Imported.seedscene";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    // 66 embedded bytes: 3 float3 positions, 3 float2 UVs, 3 uint16 indices.
    const char* gltf = R"JSON({
  "asset": {"version": "2.0"},
  "buffers": [{
    "byteLength": 66,
    "uri": "data:application/octet-stream;base64,AAAAvwAAAAAAAAAAAAAAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAD8AAIA/AAABAAIA"
  }],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962},
    {"buffer": 0, "byteOffset": 36, "byteLength": 24, "target": 34962},
    {"buffer": 0, "byteOffset": 60, "byteLength": 6, "target": 34963}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3"},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
    {"bufferView": 2, "componentType": 5123, "count": 3, "type": "SCALAR"}
  ],
  "materials": [{
    "pbrMetallicRoughness": {"baseColorFactor": [0.25, 0.5, 0.75, 1.0]}
  }],
  "meshes": [{
    "name": "Seed Test Triangle",
    "primitives": [{
      "attributes": {"POSITION": 0, "TEXCOORD_0": 1},
      "indices": 2,
      "material": 0,
      "mode": 4
    }]
  }]
})JSON";

    {
        std::ofstream output(source, std::ios::binary | std::ios::trunc);
        output << gltf;
    }

    assert(seed::AssetImporter::supports_static_model(source));
    std::string error;
    const auto imported = seed::AssetImporter::import_static_model(source, &error);
    if (!imported.has_value()) {
        std::cerr << error << '\n';
    }
    assert(imported.has_value());
    assert(imported->name == "Seed Test Triangle");
    assert(imported->vertices.size() == 3);
    assert(imported->indices.size() == 3);
    assert(imported->indices[0] == 0);
    assert(imported->indices[1] == 1);
    assert(imported->indices[2] == 2);
    assert(nearly_equal(imported->vertices[0].position[0], -0.5f));
    assert(nearly_equal(imported->vertices[1].position[0], 0.5f));
    assert(nearly_equal(imported->vertices[2].position[1], 1.0f));
    assert(nearly_equal(imported->vertices[1].texcoord[0], 1.0f));
    assert(nearly_equal(imported->vertices[2].texcoord[0], 0.5f));
    assert(nearly_equal(imported->vertices[2].texcoord[1], 1.0f));
    assert(nearly_equal(imported->base_color.x, 0.25f));
    assert(nearly_equal(imported->base_color.y, 0.5f));
    assert(nearly_equal(imported->base_color.z, 0.75f));
    assert(!imported->base_color_texture.has_value());
    assert(imported->external_files.empty());

    const std::string asset_id = seed::AssetRuntime::make_model_asset_id(
        std::filesystem::path{"Assets/Imported/Triangle/Triangle.gltf"}
    );
    assert(asset_id == "model:Assets/Imported/Triangle/Triangle.gltf");
    assert(seed::AssetRuntime::is_model_asset_id(asset_id));
    const auto relative = seed::AssetRuntime::model_asset_path(asset_id);
    assert(relative.has_value());
    assert(relative->generic_string() == "Assets/Imported/Triangle/Triangle.gltf");

    seed::Scene scene;
    const auto entity = scene.create_entity("Imported Triangle");
    scene.add_component<seed::TransformComponent>(entity);

    seed::MeshComponent mesh;
    mesh.asset_id = asset_id;
    scene.add_component<seed::MeshComponent>(entity, mesh);

    seed::MaterialComponent material;
    material.asset_id = asset_id;
    scene.add_component<seed::MaterialComponent>(entity, material);

    assert(seed::SceneSerializer::save(scene, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    assert(loaded.entity_count() == 1);

    seed::EntityId loaded_entity = seed::InvalidEntity;
    loaded.for_each_entity([&](seed::EntityId candidate, const std::string&) {
        loaded_entity = candidate;
    });
    assert(loaded_entity != seed::InvalidEntity);
    const auto* loaded_mesh = loaded.get_component<seed::MeshComponent>(loaded_entity);
    const auto* loaded_material = loaded.get_component<seed::MaterialComponent>(loaded_entity);
    assert(loaded_mesh != nullptr);
    assert(loaded_material != nullptr);
    assert(loaded_mesh->asset_id == asset_id);
    assert(loaded_material->asset_id == asset_id);

    std::filesystem::remove_all(root);
    std::cout << "SeedAssetImportTests passed.\n";
    return 0;
}
