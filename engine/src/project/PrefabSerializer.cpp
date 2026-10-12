#include "seed/project/PrefabSerializer.h"

#include "seed/project/SceneSerializer.h"
#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <utility>
#include <vector>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

} // namespace

std::string normalized_path(const std::filesystem::path& path) {
    return std::filesystem::absolute(path).lexically_normal().generic_string();
}

std::vector<EntityId> collect_subtree(const Scene& scene, EntityId root) {
    std::vector<EntityId> entities{root};
    std::unordered_set<EntityId> known{root};
    for (std::size_t index = 0; index < entities.size(); ++index) {
        const std::string& parent_id = scene.entity_persistent_id(entities[index]);
        scene.for_each_entity([&](EntityId candidate, const std::string&) {
            if (known.contains(candidate)) return;
            const auto* parent = scene.get_component<ParentComponent>(candidate);
            if (parent != nullptr && parent->parent_persistent_id == parent_id) {
                known.insert(candidate);
                entities.push_back(candidate);
            }
        });
    }
    return entities;
}

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
        prefab_scene.copy_hierarchy_from(scene, entity, scene.entity_name(entity));

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

bool PrefabSerializer::refresh_linked_instances(
    Scene& scene,
    const std::filesystem::path& prefab_file,
    std::size_t* refreshed_instances,
    std::string* error
) {
    try {
        if (refreshed_instances != nullptr) *refreshed_instances = 0;

        Scene prefab_scene;
        if (!SceneSerializer::load(prefab_scene, prefab_file, error)) return false;

        EntityId source_root = InvalidEntity;
        prefab_scene.for_each_entity([&](EntityId candidate, const std::string&) {
            if (source_root == InvalidEntity) source_root = candidate;
        });
        if (source_root == InvalidEntity) {
            set_error(error, "Prefab contains no instantiable entity: " + prefab_file.string());
            return false;
        }

        const std::string source_path = normalized_path(prefab_file);
        std::vector<EntityId> linked_roots;
        scene.for_each_entity([&](EntityId candidate, const std::string&) {
            const auto* link = scene.get_component<PrefabInstanceComponent>(candidate);
            if (link != nullptr && link->linked &&
                normalized_path(link->source_path) == source_path) {
                linked_roots.push_back(candidate);
            }
        });

        for (const EntityId root : linked_roots) {
            if (!scene.is_alive(root)) continue;

            TransformComponent saved_transform;
            const bool has_transform = scene.get_component<TransformComponent>(root) != nullptr;
            if (has_transform) saved_transform = *scene.get_component<TransformComponent>(root);

            std::string saved_parent;
            if (const auto* parent = scene.get_component<ParentComponent>(root)) {
                saved_parent = parent->parent_persistent_id;
            }

            const auto old_entities = collect_subtree(scene, root);
            for (auto it = old_entities.rbegin(); it != old_entities.rend(); ++it) {
                scene.destroy_entity(*it);
            }

            const auto instances = scene.copy_hierarchy_from(prefab_scene, source_root);
            if (instances.empty()) continue;

            const EntityId new_root = instances.front();
            if (has_transform) {
                scene.add_component<TransformComponent>(new_root, saved_transform);
            }
            if (!saved_parent.empty()) {
                scene.add_component<ParentComponent>(
                    new_root,
                    ParentComponent{saved_parent}
                );
            } else {
                scene.remove_component<ParentComponent>(new_root);
            }

            auto& link = scene.add_component<PrefabInstanceComponent>(new_root);
            link.source_path = source_path;
            link.source_entity_id = prefab_scene.entity_persistent_id(source_root);
            link.linked = true;

            if (refreshed_instances != nullptr) ++(*refreshed_instances);
        }
        return true;
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

        const auto instances = scene.copy_hierarchy_from(prefab_scene, source_entity);
        if (instances.empty()) {
            set_error(error, "Prefab hierarchy could not be instantiated: " + prefab_file.string());
            return false;
        }
        auto& link = scene.add_component<PrefabInstanceComponent>(instances.front());
        link.source_path = normalized_path(prefab_file);
        link.source_entity_id = prefab_scene.entity_persistent_id(source_entity);
        link.linked = true;
        if (created_entity != nullptr) *created_entity = instances.front();
        return true;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

} // namespace seed
