#include "seed/project/SceneSerializer.h"

#include "seed/gameplay/Components.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) {
        *error = std::move(message);
    }
}

nlohmann::json vec3_to_json(const Vec3& value) {
    return nlohmann::json::array({value.x, value.y, value.z});
}

Vec3 vec3_from_json(const nlohmann::json& value, Vec3 fallback = {}) {
    if (!value.is_array() || value.size() != 3) {
        return fallback;
    }
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
    };
}

const char* door_motion_name(DoorMotion motion) {
    return motion == DoorMotion::Slide ? "slide" : "rotate";
}

DoorMotion door_motion_from_name(const std::string& name) {
    return name == "slide" ? DoorMotion::Slide : DoorMotion::Rotate;
}

} // namespace

bool SceneSerializer::save(
    const Scene& scene,
    const std::filesystem::path& scene_file,
    std::string* error
) {
    try {
        if (!scene_file.parent_path().empty()) {
            std::filesystem::create_directories(scene_file.parent_path());
        }

        nlohmann::json entities = nlohmann::json::array();

        scene.for_each_entity([&](EntityId entity, const std::string& name) {
            if (const auto* camera = scene.get_component<CameraComponent>(entity);
                camera != nullptr && camera->editor_only) {
                return;
            }

            nlohmann::json components = nlohmann::json::object();

            if (const auto* transform = scene.get_component<TransformComponent>(entity)) {
                components["Transform"] = {
                    {"position", vec3_to_json(transform->position)},
                    {"rotation", vec3_to_json(transform->rotation_degrees)},
                    {"scale", vec3_to_json(transform->scale)},
                };
            }

            if (const auto* camera = scene.get_component<CameraComponent>(entity)) {
                components["Camera"] = {
                    {"field_of_view", camera->field_of_view_degrees},
                    {"near_plane", camera->near_plane},
                    {"far_plane", camera->far_plane},
                    {"enabled", camera->enabled},
                    {"primary", camera->primary},
                };
            }

            if (const auto* mesh = scene.get_component<MeshComponent>(entity)) {
                components["Mesh"] = {
                    {"asset", mesh->asset_id},
                    {"visible", mesh->visible},
                };
            }

            if (const auto* material = scene.get_component<MaterialComponent>(entity)) {
                components["Material"] = {
                    {"asset", material->asset_id},
                };
            }

            if (const auto* interactable = scene.get_component<InteractableComponent>(entity)) {
                components["Interactable"] = {
                    {"prompt", interactable->prompt},
                    {"enabled", interactable->enabled},
                };
            }

            if (const auto* door = scene.get_component<DoorComponent>(entity)) {
                components["Door"] = {
                    {"motion", door_motion_name(door->motion)},
                    {"open_amount", door->open_amount},
                    {"duration_seconds", door->duration_seconds},
                    {"required_item", door->required_item},
                    {"consume_required_item", door->consume_required_item},
                    {"starts_open", door->starts_open},
                };
            }

            if (const auto* health = scene.get_component<HealthComponent>(entity)) {
                components["Health"] = {
                    {"maximum", health->maximum},
                    {"current", health->current},
                    {"invulnerable", health->invulnerable},
                };
            }

            if (const auto* inventory = scene.get_component<InventoryComponent>(entity)) {
                components["Inventory"] = {
                    {"items", inventory->items},
                };
            }

            entities.push_back({
                {"id", scene.entity_persistent_id(entity)},
                {"name", name},
                {"components", std::move(components)},
            });
        });

        nlohmann::json document = {
            {"seed_format", "seedscene"},
            {"format_version", 1},
            {"entities", std::move(entities)},
        };

        std::ofstream output(scene_file, std::ios::binary | std::ios::trunc);
        if (!output) {
            set_error(error, "Could not open scene file for writing: " + scene_file.string());
            return false;
        }

        output << document.dump(2) << '\n';
        return static_cast<bool>(output);
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

bool SceneSerializer::load(
    Scene& scene,
    const std::filesystem::path& scene_file,
    std::string* error
) {
    try {
        std::ifstream input(scene_file, std::ios::binary);
        if (!input) {
            set_error(error, "Could not open scene file: " + scene_file.string());
            return false;
        }

        nlohmann::json document;
        input >> document;

        if (document.value("seed_format", std::string{}) != "seedscene") {
            set_error(error, "File is not a Seed scene: " + scene_file.string());
            return false;
        }

        Scene loaded_scene;

        for (const auto& entity_json : document.value("entities", nlohmann::json::array())) {
            const std::string name = entity_json.value("name", std::string{"Entity"});
            const std::string persistent_id = entity_json.value("id", std::string{});
            const EntityId entity = loaded_scene.create_entity(name, persistent_id);
            const auto& components = entity_json.value("components", nlohmann::json::object());

            if (components.contains("Transform")) {
                const auto& value = components.at("Transform");
                TransformComponent transform;
                transform.position = vec3_from_json(value.value("position", nlohmann::json::array()), {});
                transform.rotation_degrees = vec3_from_json(value.value("rotation", nlohmann::json::array()), {});
                transform.scale = vec3_from_json(value.value("scale", nlohmann::json::array()), {1.0f, 1.0f, 1.0f});
                loaded_scene.add_component<TransformComponent>(entity, transform);
            }

            if (components.contains("Camera")) {
                const auto& value = components.at("Camera");
                CameraComponent camera;
                camera.field_of_view_degrees = value.value("field_of_view", 60.0f);
                camera.near_plane = value.value("near_plane", 0.1f);
                camera.far_plane = value.value("far_plane", 1000.0f);
                camera.enabled = value.value("enabled", true);
                camera.primary = value.value("primary", true);
                camera.editor_only = false;
                loaded_scene.add_component<CameraComponent>(entity, camera);
            }

            if (components.contains("Mesh")) {
                const auto& value = components.at("Mesh");
                MeshComponent mesh;
                mesh.asset_id = value.value("asset", std::string{"builtin:cube"});
                mesh.visible = value.value("visible", true);
                loaded_scene.add_component<MeshComponent>(entity, mesh);
            }

            if (components.contains("Material")) {
                const auto& value = components.at("Material");
                MaterialComponent material;
                material.asset_id = value.value("asset", std::string{"builtin:seed_default"});
                loaded_scene.add_component<MaterialComponent>(entity, material);
            }

            if (components.contains("Interactable")) {
                const auto& value = components.at("Interactable");
                InteractableComponent interactable;
                interactable.prompt = value.value("prompt", std::string{"Interact"});
                interactable.enabled = value.value("enabled", true);
                loaded_scene.add_component<InteractableComponent>(entity, interactable);
            }

            if (components.contains("Door")) {
                const auto& value = components.at("Door");
                DoorComponent door;
                door.motion = door_motion_from_name(value.value("motion", std::string{"rotate"}));
                door.open_amount = value.value("open_amount", 90.0f);
                door.duration_seconds = value.value("duration_seconds", 0.75f);
                door.required_item = value.value("required_item", std::string{});
                door.consume_required_item = value.value("consume_required_item", false);
                door.starts_open = value.value("starts_open", false);
                loaded_scene.add_component<DoorComponent>(entity, door);
            }

            if (components.contains("Health")) {
                const auto& value = components.at("Health");
                HealthComponent health;
                health.maximum = value.value("maximum", 100.0f);
                health.current = value.value("current", health.maximum);
                health.invulnerable = value.value("invulnerable", false);
                loaded_scene.add_component<HealthComponent>(entity, health);
            }

            if (components.contains("Inventory")) {
                const auto& value = components.at("Inventory");
                InventoryComponent inventory;
                if (value.contains("items")) {
                    inventory.items = value.at("items").get<std::unordered_map<std::string, int>>();
                }
                loaded_scene.add_component<InventoryComponent>(entity, std::move(inventory));
            }
        }

        scene = std::move(loaded_scene);
        return true;
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

} // namespace seed
