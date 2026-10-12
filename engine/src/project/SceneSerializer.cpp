#include "seed/project/SceneSerializer.h"

#include "seed/animation/AnimationComponents.h"
#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/save/PersistenceComponents.h"
#include "seed/scene/Scene.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace seed {
namespace {

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

nlohmann::json vec3_to_json(const Vec3& value) {
    return nlohmann::json::array({value.x, value.y, value.z});
}

Vec3 vec3_from_json(const nlohmann::json& value, Vec3 fallback = {}) {
    if (!value.is_array() || value.size() != 3) return fallback;
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
    };
}

nlohmann::json vec4_to_json(const Vec4& value) {
    return nlohmann::json::array({value.x, value.y, value.z, value.w});
}

Vec4 vec4_from_json(
    const nlohmann::json& value,
    Vec4 fallback = {1.0f, 1.0f, 1.0f, 1.0f}
) {
    if (!value.is_array() || value.size() != 4) return fallback;
    return {
        value.at(0).get<float>(),
        value.at(1).get<float>(),
        value.at(2).get<float>(),
        value.at(3).get<float>(),
    };
}

const char* door_motion_name(DoorMotion motion) {
    return motion == DoorMotion::Slide ? "slide" : "rotate";
}

DoorMotion door_motion_from_name(const std::string& name) {
    return name == "slide" ? DoorMotion::Slide : DoorMotion::Rotate;
}

const char* player_view_mode_name(PlayerViewMode mode) {
    return mode == PlayerViewMode::ThirdPerson ? "third_person" : "first_person";
}

PlayerViewMode player_view_mode_from_name(const std::string& name) {
    return name == "third_person" ? PlayerViewMode::ThirdPerson : PlayerViewMode::FirstPerson;
}

const char* quest_status_name(QuestStatus status) {
    switch (status) {
    case QuestStatus::Active: return "active";
    case QuestStatus::Completed: return "completed";
    case QuestStatus::Inactive:
    default: return "inactive";
    }
}

QuestStatus quest_status_from_name(const std::string& name) {
    if (name == "active") return QuestStatus::Active;
    if (name == "completed") return QuestStatus::Completed;
    return QuestStatus::Inactive;
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
            if (const auto* parent = scene.get_component<ParentComponent>(entity);
                parent != nullptr && !parent->parent_persistent_id.empty()) {
                components["Parent"] = {{"entity", parent->parent_persistent_id}};
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
            if (const auto* light = scene.get_component<DirectionalLightComponent>(entity)) {
                components["DirectionalLight"] = {
                    {"color", vec3_to_json(light->color)},
                    {"intensity", light->intensity},
                    {"enabled", light->enabled},
                    {"casts_shadows", light->casts_shadows},
                    {"shadow_distance", light->shadow_distance},
                };
            }
            if (const auto* light = scene.get_component<AmbientLightComponent>(entity)) {
                components["AmbientLight"] = {
                    {"color", vec3_to_json(light->color)},
                    {"intensity", light->intensity},
                    {"enabled", light->enabled},
                };
            }
            if (const auto* sky = scene.get_component<SkyComponent>(entity)) {
                components["Sky"] = {
                    {"zenith_color", vec3_to_json(sky->zenith_color)},
                    {"horizon_color", vec3_to_json(sky->horizon_color)},
                    {"intensity", sky->intensity},
                    {"enabled", sky->enabled},
                };
            }
            if (const auto* mesh = scene.get_component<MeshComponent>(entity)) {
                components["Mesh"] = {
                    {"asset", mesh->asset_id},
                    {"visible", mesh->visible},
                    {"cast_shadows", mesh->cast_shadows},
                    {"receive_shadows", mesh->receive_shadows},
                };
            }
            if (const auto* mesh = scene.get_component<SkinnedMeshComponent>(entity)) {
                components["SkinnedMesh"] = {
                    {"asset", mesh->asset_id},
                    {"visible", mesh->visible},
                    {"cast_shadows", mesh->cast_shadows},
                    {"receive_shadows", mesh->receive_shadows},
                };
            }
            if (const auto* animation = scene.get_component<AnimationPlayerComponent>(entity)) {
                nlohmann::json events = nlohmann::json::array();
                for (const auto& event : animation->events) {
                    events.push_back({
                        {"clip", event.clip_name},
                        {"name", event.name},
                        {"time", event.time_seconds},
                    });
                }
                components["AnimationPlayer"] = {
                    {"clip", animation->clip_name},
                    {"playback_speed", animation->playback_speed},
                    {"playing", animation->playing},
                    {"blend_duration", animation->blend_duration_seconds},
                    {"locomotion_enabled", animation->locomotion_enabled},
                    {"events", std::move(events)},
                };
            }
            if (const auto* material = scene.get_component<MaterialComponent>(entity)) {
                components["Material"] = {
                    {"asset", material->asset_id},
                    {"base_color", vec4_to_json(material->base_color)},
                    {"metallic", material->metallic},
                    {"roughness", material->roughness},
                    {"normal_scale", material->normal_scale},
                    {"use_base_color_texture", material->use_base_color_texture},
                    {"use_metallic_roughness_texture", material->use_metallic_roughness_texture},
                    {"use_normal_texture", material->use_normal_texture},
                };
            }
            if (const auto* collider = scene.get_component<BoxColliderComponent>(entity)) {
                components["BoxCollider"] = {
                    {"half_extents", vec3_to_json(collider->half_extents)},
                    {"offset", vec3_to_json(collider->offset)},
                    {"enabled", collider->enabled},
                    {"solid", collider->solid},
                };
            }
            if (const auto* character = scene.get_component<CharacterBodyComponent>(entity)) {
                components["CharacterBody"] = {
                    {"radius", character->radius},
                    {"height", character->height},
                    {"eye_height", character->eye_height},
                    {"gravity", character->gravity},
                    {"jump_speed", character->jump_speed},
                    {"max_fall_speed", character->max_fall_speed},
                    {"enabled", character->enabled},
                };
            }
            if (const auto* persistent = scene.get_component<PersistentComponent>(entity)) {
                components["Persistent"] = {{"enabled", persistent->enabled}};
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
            if (const auto* attack = scene.get_component<AttackComponent>(entity)) {
                components["Attack"] = {
                    {"open_event", attack->open_event},
                    {"close_event", attack->close_event},
                    {"damage", attack->damage},
                };
            }
            if (const auto* feedback_cues = scene.get_component<FeedbackCueComponent>(entity)) {
                nlohmann::json bindings = nlohmann::json::array();
                for (const auto& binding : feedback_cues->bindings) {
                    bindings.push_back({
                        {"event_name", binding.event_name},
                        {"audio_asset", binding.audio_asset},
                        {"visual_asset", binding.visual_asset},
                        {"volume", binding.volume},
                        {"camera_shake", binding.camera_shake},
                    });
                }
                components["FeedbackCues"] = {{"bindings", std::move(bindings)}};
            }
            if (const auto* inventory = scene.get_component<InventoryComponent>(entity)) {
                components["Inventory"] = {{"items", inventory->items}};
            }
            if (const auto* pickup = scene.get_component<PickupComponent>(entity)) {
                components["Pickup"] = {
                    {"item_id", pickup->item_id},
                    {"display_name", pickup->display_name},
                    {"quantity", pickup->quantity},
                    {"destroy_on_pickup", pickup->destroy_on_pickup},
                };
            }
            if (const auto* dialogue = scene.get_component<DialogueComponent>(entity)) {
                nlohmann::json lines = nlohmann::json::array();
                for (const auto& line : dialogue->lines) {
                    nlohmann::json choices = nlohmann::json::array();
                    for (const auto& choice : line.choices) {
                        choices.push_back({
                            {"label", choice.label},
                            {"next_line", choice.next_line},
                        });
                    }
                    lines.push_back({
                        {"speaker", line.speaker},
                        {"text", line.text},
                        {"choices", std::move(choices)},
                    });
                }
                components["Dialogue"] = {
                    {"lines", std::move(lines)},
                    {"repeatable", dialogue->repeatable},
                    {"interaction_prompt", dialogue->interaction_prompt},
                    {"quest_id_to_start", dialogue->quest_id_to_start},
                };
            }
            if (const auto* quest = scene.get_component<QuestComponent>(entity)) {
                components["Quest"] = {
                    {"quest_id", quest->quest_id},
                    {"title", quest->title},
                    {"description", quest->description},
                    {"required_item", quest->required_item},
                    {"required_quantity", quest->required_quantity},
                    {"reward_item", quest->reward_item},
                    {"reward_quantity", quest->reward_quantity},
                    {"status", quest_status_name(quest->status)},
                };
            }
            if (const auto* player = scene.get_component<PlayerControllerComponent>(entity)) {
                components["PlayerController"] = {
                    {"view_mode", player_view_mode_name(player->view_mode)},
                    {"move_speed", player->move_speed},
                    {"fast_multiplier", player->fast_multiplier},
                    {"look_sensitivity", player->look_sensitivity},
                    {"interaction_distance", player->interaction_distance},
                    {"interaction_radius", player->interaction_radius},
                    {"camera_distance", player->camera_distance},
                    {"camera_height", player->camera_height},
                    {"camera_shoulder_offset", player->camera_shoulder_offset},
                    {"camera_min_pitch", player->camera_min_pitch},
                    {"camera_max_pitch", player->camera_max_pitch},
                    {"orient_to_movement", player->orient_to_movement},
                    {"enabled", player->enabled},
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
            const auto& components =
                entity_json.value("components", nlohmann::json::object());

            if (components.contains("Transform")) {
                const auto& value = components.at("Transform");
                TransformComponent transform;
                transform.position = vec3_from_json(
                    value.value("position", nlohmann::json::array()),
                    {}
                );
                transform.rotation_degrees = vec3_from_json(
                    value.value("rotation", nlohmann::json::array()),
                    {}
                );
                transform.scale = vec3_from_json(
                    value.value("scale", nlohmann::json::array()),
                    {1.0f, 1.0f, 1.0f}
                );
                loaded_scene.add_component<TransformComponent>(entity, transform);
            }
            if (components.contains("Parent")) {
                const auto& value = components.at("Parent");
                ParentComponent parent;
                parent.parent_persistent_id = value.value("entity", std::string{});
                if (!parent.parent_persistent_id.empty()) {
                    loaded_scene.add_component<ParentComponent>(entity, std::move(parent));
                }
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
            if (components.contains("DirectionalLight")) {
                const auto& value = components.at("DirectionalLight");
                DirectionalLightComponent light;
                light.color = vec3_from_json(
                    value.value("color", nlohmann::json::array()),
                    {1.0f, 0.96f, 0.88f}
                );
                light.intensity = value.value("intensity", 1.0f);
                light.enabled = value.value("enabled", true);
                light.casts_shadows = value.value("casts_shadows", true);
                light.shadow_distance = value.value("shadow_distance", 35.0f);
                loaded_scene.add_component<DirectionalLightComponent>(entity, light);
            }
            if (components.contains("AmbientLight")) {
                const auto& value = components.at("AmbientLight");
                AmbientLightComponent light;
                light.color = vec3_from_json(
                    value.value("color", nlohmann::json::array()),
                    {0.72f, 0.82f, 1.0f}
                );
                light.intensity = value.value("intensity", 0.22f);
                light.enabled = value.value("enabled", true);
                loaded_scene.add_component<AmbientLightComponent>(entity, light);
            }
            if (components.contains("Sky")) {
                const auto& value = components.at("Sky");
                SkyComponent sky;
                sky.zenith_color = vec3_from_json(
                    value.value("zenith_color", nlohmann::json::array()),
                    {0.08f, 0.20f, 0.42f}
                );
                sky.horizon_color = vec3_from_json(
                    value.value("horizon_color", nlohmann::json::array()),
                    {0.62f, 0.76f, 0.92f}
                );
                sky.intensity = value.value("intensity", 1.0f);
                sky.enabled = value.value("enabled", true);
                loaded_scene.add_component<SkyComponent>(entity, sky);
            }
            if (components.contains("Mesh")) {
                const auto& value = components.at("Mesh");
                MeshComponent mesh;
                mesh.asset_id = value.value("asset", std::string{"builtin:cube"});
                mesh.visible = value.value("visible", true);
                mesh.cast_shadows = value.value("cast_shadows", true);
                mesh.receive_shadows = value.value("receive_shadows", true);
                loaded_scene.add_component<MeshComponent>(entity, mesh);
            }
            if (components.contains("SkinnedMesh")) {
                const auto& value = components.at("SkinnedMesh");
                SkinnedMeshComponent mesh;
                mesh.asset_id = value.value("asset", std::string{});
                mesh.visible = value.value("visible", true);
                mesh.cast_shadows = value.value("cast_shadows", true);
                mesh.receive_shadows = value.value("receive_shadows", true);
                mesh.mesh = {};
                mesh.runtime_asset = nullptr;
                loaded_scene.add_component<SkinnedMeshComponent>(entity, std::move(mesh));
            }
            if (components.contains("AnimationPlayer")) {
                const auto& value = components.at("AnimationPlayer");
                AnimationPlayerComponent animation;
                animation.clip_name = value.value("clip", std::string{});
                animation.playback_speed = value.value("playback_speed", 1.0f);
                animation.playing = value.value("playing", true);
                animation.blend_duration_seconds = std::max(
                    0.0f,
                    value.value("blend_duration", 0.15f)
                );
                animation.locomotion_enabled = value.value("locomotion_enabled", false);
                if (value.contains("events") && value.at("events").is_array()) {
                    for (const auto& event_value : value.at("events")) {
                        AnimationEvent event;
                        event.clip_name = event_value.value("clip", std::string{});
                        event.name = event_value.value("name", std::string{"Event"});
                        event.time_seconds = std::max(
                            0.0f,
                            event_value.value("time", 0.0f)
                        );
                        animation.events.push_back(std::move(event));
                    }
                }
                animation.time_seconds = 0.0f;
                animation.pose = {};
                loaded_scene.add_component<AnimationPlayerComponent>(entity, std::move(animation));
            }
            if (components.contains("Material")) {
                const auto& value = components.at("Material");
                MaterialComponent material;
                material.asset_id = value.value(
                    "asset",
                    std::string{"builtin:seed_default"}
                );
                material.use_asset_defaults =
                    !value.contains("base_color") &&
                    !value.contains("metallic") &&
                    !value.contains("roughness");
                material.base_color = vec4_from_json(
                    value.value("base_color", nlohmann::json::array()),
                    {1.0f, 1.0f, 1.0f, 1.0f}
                );
                material.metallic = value.value("metallic", 0.0f);
                material.roughness = value.value("roughness", 0.65f);
                material.normal_scale = value.value("normal_scale", 1.0f);
                material.use_base_color_texture =
                    value.value("use_base_color_texture", true);
                material.use_metallic_roughness_texture =
                    value.value("use_metallic_roughness_texture", true);
                material.use_normal_texture = value.value("use_normal_texture", true);
                loaded_scene.add_component<MaterialComponent>(entity, material);
            }
            if (components.contains("BoxCollider")) {
                const auto& value = components.at("BoxCollider");
                BoxColliderComponent collider;
                collider.half_extents = vec3_from_json(
                    value.value("half_extents", nlohmann::json::array()),
                    {0.75f, 0.75f, 0.75f}
                );
                collider.offset = vec3_from_json(
                    value.value("offset", nlohmann::json::array()),
                    {}
                );
                collider.enabled = value.value("enabled", true);
                collider.solid = value.value("solid", true);
                loaded_scene.add_component<BoxColliderComponent>(entity, collider);
            }
            if (components.contains("CharacterBody")) {
                const auto& value = components.at("CharacterBody");
                CharacterBodyComponent character;
                character.radius = value.value("radius", 0.35f);
                character.height = value.value("height", 1.8f);
                character.eye_height = value.value("eye_height", 1.65f);
                character.gravity = value.value("gravity", 18.0f);
                character.jump_speed = value.value("jump_speed", 6.5f);
                character.max_fall_speed = value.value("max_fall_speed", 30.0f);
                character.enabled = value.value("enabled", true);
                loaded_scene.add_component<CharacterBodyComponent>(entity, character);
            }
            if (components.contains("Persistent")) {
                const auto& value = components.at("Persistent");
                PersistentComponent persistent;
                persistent.enabled = value.value("enabled", true);
                loaded_scene.add_component<PersistentComponent>(entity, persistent);
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
                door.motion = door_motion_from_name(
                    value.value("motion", std::string{"rotate"})
                );
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
            if (components.contains("Attack")) {
                const auto& value = components.at("Attack");
                AttackComponent attack;
                attack.open_event = value.value("open_event", std::string{"AttackStart"});
                attack.close_event = value.value("close_event", std::string{"AttackEnd"});
                attack.damage = std::max(0.0f, value.value("damage", 25.0f));
                attack.active = false;
                loaded_scene.add_component<AttackComponent>(entity, std::move(attack));
            }
            if (components.contains("FeedbackCues")) {
                const auto& value = components.at("FeedbackCues");
                FeedbackCueComponent feedback_cues;
                if (value.contains("bindings")) {
                    for (const auto& item : value.at("bindings")) {
                        FeedbackCueBinding binding;
                        binding.event_name = item.value("event_name", std::string{"Footstep"});
                        binding.audio_asset = item.value("audio_asset", std::string{});
                        binding.visual_asset = item.value("visual_asset", std::string{});
                        binding.volume = std::max(0.0f, item.value("volume", 1.0f));
                        binding.camera_shake = std::max(0.0f, item.value("camera_shake", 0.0f));
                        feedback_cues.bindings.push_back(std::move(binding));
                    }
                }
                loaded_scene.add_component<FeedbackCueComponent>(entity, std::move(feedback_cues));
            }
            if (components.contains("Inventory")) {
                const auto& value = components.at("Inventory");
                InventoryComponent inventory;
                if (value.contains("items")) {
                    inventory.items = value.at("items")
                        .get<std::unordered_map<std::string, int>>();
                }
                loaded_scene.add_component<InventoryComponent>(entity, std::move(inventory));
            }
            if (components.contains("Pickup")) {
                const auto& value = components.at("Pickup");
                PickupComponent pickup;
                pickup.item_id = value.value("item_id", std::string{"Item"});
                pickup.display_name = value.value("display_name", pickup.item_id);
                pickup.quantity = std::max(1, value.value("quantity", 1));
                pickup.destroy_on_pickup = value.value("destroy_on_pickup", true);
                loaded_scene.add_component<PickupComponent>(entity, std::move(pickup));
            }
            if (components.contains("Dialogue")) {
                const auto& value = components.at("Dialogue");
                DialogueComponent dialogue;
                dialogue.repeatable = value.value("repeatable", true);
                dialogue.interaction_prompt = value.value("interaction_prompt", std::string{"Talk"});
                dialogue.quest_id_to_start = value.value("quest_id_to_start", std::string{});
                if (value.contains("lines") && value.at("lines").is_array()) {
                    for (const auto& line_value : value.at("lines")) {
                        DialogueLine line;
                        line.speaker = line_value.value("speaker", std::string{"Speaker"});
                        line.text = line_value.value("text", std::string{"..."});
                        if (line_value.contains("choices") && line_value.at("choices").is_array()) {
                            for (const auto& choice_value : line_value.at("choices")) {
                                DialogueChoice choice;
                                choice.label = choice_value.value("label", std::string{"Continue"});
                                choice.next_line = choice_value.value("next_line", -1);
                                line.choices.push_back(std::move(choice));
                            }
                        }
                        dialogue.lines.push_back(std::move(line));
                    }
                }
                loaded_scene.add_component<DialogueComponent>(entity, std::move(dialogue));
            }
            if (components.contains("Quest")) {
                const auto& value = components.at("Quest");
                QuestComponent quest;
                quest.quest_id = value.value("quest_id", std::string{"quest"});
                quest.title = value.value("title", std::string{"New Quest"});
                quest.description = value.value("description", std::string{});
                quest.required_item = value.value("required_item", std::string{});
                quest.required_quantity = std::max(1, value.value("required_quantity", 1));
                quest.reward_item = value.value("reward_item", std::string{});
                quest.reward_quantity = std::max(1, value.value("reward_quantity", 1));
                quest.status = quest_status_from_name(value.value("status", std::string{"inactive"}));
                loaded_scene.add_component<QuestComponent>(entity, std::move(quest));
            }
            if (components.contains("PlayerController")) {
                const auto& value = components.at("PlayerController");
                PlayerControllerComponent player;
                player.view_mode = player_view_mode_from_name(
                    value.value("view_mode", std::string{"first_person"})
                );
                player.move_speed = value.value("move_speed", 4.0f);
                player.fast_multiplier = value.value("fast_multiplier", 2.5f);
                player.look_sensitivity = value.value("look_sensitivity", 0.12f);
                player.interaction_distance = value.value("interaction_distance", 4.0f);
                player.interaction_radius = value.value("interaction_radius", 1.25f);
                player.camera_distance = value.value("camera_distance", 4.5f);
                player.camera_height = value.value("camera_height", 1.55f);
                player.camera_shoulder_offset = value.value("camera_shoulder_offset", 0.0f);
                player.camera_min_pitch = value.value("camera_min_pitch", -60.0f);
                player.camera_max_pitch = value.value("camera_max_pitch", 70.0f);
                player.orient_to_movement = value.value("orient_to_movement", true);
                player.enabled = value.value("enabled", true);
                loaded_scene.add_component<PlayerControllerComponent>(entity, player);
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
