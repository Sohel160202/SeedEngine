#include "seed/project/PrefabSerializer.h"

#include "seed/project/SceneSerializer.h"
#include "seed/scene/Scene.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

} // namespace

bool PrefabSerializer::save(
    const Scene& scene,
    EntityId entity,
    const std::filesystem::path& prefab_file,
    std::string* error
) {
    try {
        if (!scene.is_alive(entity)) {
            set_error(error, "Cannot save a prefab: the selected entity is not alive.");
            return false;
        }

        Scene prefab_scene;
        prefab_scene.copy_entity_from(scene, entity, scene.entity_name(entity));

        if (!SceneSerializer::save(prefab_scene, prefab_file, error)) return false;

        std::ifstream input(prefab_file, std::ios::binary);
        if (!input) {
            set_error(error, "Could not reopen prefab file: " + prefab_file.string());
            return false;
        }
        nlohmann::json document;
        input >> document;
        document["seed_format"] = "seedprefab";
        document["format_version"] = 1;
        document["prefab_name"] = scene.entity_name(entity);
        input.close();

        std::ofstream output(prefab_file, std::ios::binary | std::ios::trunc);
        if (!output) {
            set_error(error, "Could not write prefab file: " + prefab_file.string());
            return false;
        }
        output << document.dump(2) << '\n';
        return static_cast<bool>(output);
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

bool PrefabSerializer::instantiate(
    Scene& scene,
    const std::filesystem::path& prefab_file,
    EntityId* created_entity,
    std::string* error
) {
    try {
        Scene prefab_scene;
        if (!SceneSerializer::load(prefab_scene, prefab_file, error)) return false;

        EntityId source_entity = InvalidEntity;
        prefab_scene.for_each_entity([&](EntityId candidate, const std::string&) {
            if (source_entity == InvalidEntity) source_entity = candidate;
        });
        if (source_entity == InvalidEntity) {
            set_error(error, "Prefab contains no instantiable entity: " + prefab_file.string());
            return false;
        }

        const EntityId instance = scene.copy_entity_from(prefab_scene, source_entity);
        if (created_entity != nullptr) *created_entity = instance;
        return true;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

} // namespace seed
