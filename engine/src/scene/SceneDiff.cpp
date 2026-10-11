#include "seed/scene/SceneDiff.h"

#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace seed {
namespace {

constexpr float FloatEpsilon = 0.0001f;

enum class ComponentPresence {
    Neither,
    AuthoredOnly,
    RuntimeOnly,
    Both,
};

bool nearly_equal(float a, float b) {
    return std::fabs(a - b) <= FloatEpsilon;
}

bool nearly_equal(const Vec3& a, const Vec3& b) {
    return nearly_equal(a.x, b.x) && nearly_equal(a.y, b.y) && nearly_equal(a.z, b.z);
}

bool nearly_equal(const Vec4& a, const Vec4& b) {
    return nearly_equal(a.x, b.x) && nearly_equal(a.y, b.y) && nearly_equal(a.z, b.z) && nearly_equal(a.w, b.w);
}

bool values_equal(const SceneChangeValue& a, const SceneChangeValue& b) {
    if (a.index() != b.index()) return false;
    if (const auto* value = std::get_if<float>(&a)) return nearly_equal(*value, std::get<float>(b));
    if (const auto* value = std::get_if<Vec3>(&a)) return nearly_equal(*value, std::get<Vec3>(b));
    if (const auto* value = std::get_if<Vec4>(&a)) return nearly_equal(*value, std::get<Vec4>(b));
    return a == b;
}

const char* door_motion_name(DoorMotion value) {
    return value == DoorMotion::Slide ? "slide" : "rotate";
}

const char* player_view_mode_name(PlayerViewMode value) {
    return value == PlayerViewMode::ThirdPerson ? "third_person" : "first_person";
}

std::string inventory_string(const InventoryComponent& inventory) {
    std::vector<std::pair<std::string, int>> items(inventory.items.begin(), inventory.items.end());
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::ostringstream stream;
    bool first = true;
    for (const auto& [name, quantity] : items) {
        if (!first) stream << ';';
        first = false;
        stream << name << '=' << quantity;
    }
    return stream.str();
}

void add_change(
    SceneDiff& diff,
    SceneChangeKind kind,
    SceneChangeSafety safety,
    const std::string& persistent_id,
    const std::string& entity_name,
    std::string component = {},
    std::string property = {},
    SceneChangeValue before_value = {},
    SceneChangeValue after_value = {}
) {
    diff.changes.push_back({
        .kind = kind,
        .safety = safety,
        .entity_persistent_id = persistent_id,
        .entity_name = entity_name,
        .component = std::move(component),
        .property = std::move(property),
        .before_value = std::move(before_value),
        .after_value = std::move(after_value),
    });
}

void add_property(
    SceneDiff& diff,
    SceneChangeSafety safety,
    const std::string& persistent_id,
    const std::string& entity_name,
    const char* component,
    const char* property,
    SceneChangeValue before_value,
    SceneChangeValue after_value
) {
    if (values_equal(before_value, after_value)) return;
    add_change(
        diff,
        SceneChangeKind::PropertyChanged,
        safety,
        persistent_id,
        entity_name,
        component,
        property,
        std::move(before_value),
        std::move(after_value)
    );
}

template <typename Component>
ComponentPresence compare_presence(
    const Scene& authored,
    EntityId authored_entity,
    const Scene& runtime,
    EntityId runtime_entity,
    SceneDiff& diff,
    const std::string& persistent_id,
    const std::string& entity_name,
    const char* component_name,
    SceneChangeSafety structural_safety = SceneChangeSafety::Safe
) {
    const bool has_authored = authored.has_component<Component>(authored_entity);
    const bool has_runtime = runtime.has_component<Component>(runtime_entity);
    if (!has_authored && !has_runtime) return ComponentPresence::Neither;
    if (has_authored && has_runtime) return ComponentPresence::Both;
    add_change(
        diff,
        has_runtime ? SceneChangeKind::ComponentAdded : SceneChangeKind::ComponentRemoved,
        structural_safety,
        persistent_id,
        entity_name,
        component_name
    );
    return has_runtime ? ComponentPresence::RuntimeOnly : ComponentPresence::AuthoredOnly;
}

template <typename Component>
void copy_plain_component(Scene& target, EntityId target_entity, const Scene& runtime, EntityId runtime_entity) {
    if (const auto* value = runtime.get_component<Component>(runtime_entity)) {
        target.add_component<Component>(target_entity, *value);
    }
}

void copy_mesh_component(Scene& target, EntityId target_entity, const Scene& runtime, EntityId runtime_entity) {
    if (const auto* source = runtime.get_component<MeshComponent>(runtime_entity)) {
        auto value = *source;
        value.mesh = {};
        target.add_component<MeshComponent>(target_entity, std::move(value));
    }
}

void copy_material_component(Scene& target, EntityId target_entity, const Scene& runtime, EntityId runtime_entity) {
    if (const auto* source = runtime.get_component<MaterialComponent>(runtime_entity)) {
        auto value = *source;
        value.shader = {};
        value.base_color_texture = {};
        value.metallic_roughness_texture = {};
        value.normal_texture = {};
        target.add_component<MaterialComponent>(target_entity, std::move(value));
    }
}

bool copy_component_by_name(
    Scene& target,
    EntityId target_entity,
    const Scene& runtime,
    EntityId runtime_entity,
    const std::string& component
) {
    if (component == "Transform") copy_plain_component<TransformComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Camera") copy_plain_component<CameraComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Mesh") copy_mesh_component(target, target_entity, runtime, runtime_entity);
    else if (component == "Material") copy_material_component(target, target_entity, runtime, runtime_entity);
    else if (component == "DirectionalLight") copy_plain_component<DirectionalLightComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "AmbientLight") copy_plain_component<AmbientLightComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Sky") copy_plain_component<SkyComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Interactable") copy_plain_component<InteractableComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Door") copy_plain_component<DoorComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Health") copy_plain_component<HealthComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Inventory") copy_plain_component<InventoryComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "Pickup") copy_plain_component<PickupComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "PlayerController") copy_plain_component<PlayerControllerComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "BoxCollider") copy_plain_component<BoxColliderComponent>(target, target_entity, runtime, runtime_entity);
    else if (component == "CharacterBody") copy_plain_component<CharacterBodyComponent>(target, target_entity, runtime, runtime_entity);
    else return false;
    return true;
}

bool remove_component_by_name(Scene& target, EntityId entity, const std::string& component) {
    if (component == "Transform") return target.remove_component<TransformComponent>(entity);
    if (component == "Camera") return target.remove_component<CameraComponent>(entity);
    if (component == "Mesh") return target.remove_component<MeshComponent>(entity);
    if (component == "Material") return target.remove_component<MaterialComponent>(entity);
    if (component == "DirectionalLight") return target.remove_component<DirectionalLightComponent>(entity);
    if (component == "AmbientLight") return target.remove_component<AmbientLightComponent>(entity);
    if (component == "Sky") return target.remove_component<SkyComponent>(entity);
    if (component == "Interactable") return target.remove_component<InteractableComponent>(entity);
    if (component == "Door") return target.remove_component<DoorComponent>(entity);
    if (component == "Health") return target.remove_component<HealthComponent>(entity);
    if (component == "Inventory") return target.remove_component<InventoryComponent>(entity);
    if (component == "Pickup") return target.remove_component<PickupComponent>(entity);
    if (component == "PlayerController") return target.remove_component<PlayerControllerComponent>(entity);
    if (component == "BoxCollider") return target.remove_component<BoxColliderComponent>(entity);
    if (component == "CharacterBody") return target.remove_component<CharacterBodyComponent>(entity);
    return false;
}

void copy_authorable_entity(Scene& target, const Scene& runtime, EntityId runtime_entity) {
    const std::string persistent_id = runtime.entity_persistent_id(runtime_entity);
    if (target.find_entity_by_persistent_id(persistent_id) != InvalidEntity) return;
    const EntityId created = target.create_entity(runtime.entity_name(runtime_entity), persistent_id);

    if (const auto* parent = runtime.get_component<ParentComponent>(runtime_entity)) {
        target.add_component<ParentComponent>(created, *parent);
    }
    copy_plain_component<TransformComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<CameraComponent>(target, created, runtime, runtime_entity);
    copy_mesh_component(target, created, runtime, runtime_entity);
    copy_material_component(target, created, runtime, runtime_entity);
    copy_plain_component<DirectionalLightComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<AmbientLightComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<SkyComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<InteractableComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<DoorComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<HealthComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<InventoryComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<PickupComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<PlayerControllerComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<BoxColliderComponent>(target, created, runtime, runtime_entity);
    copy_plain_component<CharacterBodyComponent>(target, created, runtime, runtime_entity);
}

bool apply_property_change(
    Scene& target,
    EntityId target_entity,
    const Scene& runtime,
    EntityId runtime_entity,
    const SceneChange& change
) {
    const std::string& component = change.component;
    const std::string& property = change.property;

    if (component == "Transform") {
        auto* dst = target.get_component<TransformComponent>(target_entity);
        const auto* src = runtime.get_component<TransformComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "position") dst->position = src->position;
        else if (property == "rotation_degrees") dst->rotation_degrees = src->rotation_degrees;
        else if (property == "scale") dst->scale = src->scale;
        else return false;
        return true;
    }
    if (component == "Camera") {
        auto* dst = target.get_component<CameraComponent>(target_entity);
        const auto* src = runtime.get_component<CameraComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "field_of_view_degrees") dst->field_of_view_degrees = src->field_of_view_degrees;
        else if (property == "near_plane") dst->near_plane = src->near_plane;
        else if (property == "far_plane") dst->far_plane = src->far_plane;
        else if (property == "enabled") dst->enabled = src->enabled;
        else if (property == "primary") dst->primary = src->primary;
        else return false;
        return true;
    }
    if (component == "Mesh") {
        auto* dst = target.get_component<MeshComponent>(target_entity);
        const auto* src = runtime.get_component<MeshComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "visible") dst->visible = src->visible;
        else if (property == "cast_shadows") dst->cast_shadows = src->cast_shadows;
        else if (property == "receive_shadows") dst->receive_shadows = src->receive_shadows;
        else if (property == "asset_id") dst->asset_id = src->asset_id;
        else return false;
        return true;
    }
    if (component == "Material") {
        auto* dst = target.get_component<MaterialComponent>(target_entity);
        const auto* src = runtime.get_component<MaterialComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "base_color") dst->base_color = src->base_color;
        else if (property == "metallic") dst->metallic = src->metallic;
        else if (property == "roughness") dst->roughness = src->roughness;
        else if (property == "normal_scale") dst->normal_scale = src->normal_scale;
        else if (property == "use_base_color_texture") dst->use_base_color_texture = src->use_base_color_texture;
        else if (property == "use_metallic_roughness_texture") dst->use_metallic_roughness_texture = src->use_metallic_roughness_texture;
        else if (property == "use_normal_texture") dst->use_normal_texture = src->use_normal_texture;
        else if (property == "asset_id") dst->asset_id = src->asset_id;
        else return false;
        return true;
    }
    if (component == "DirectionalLight") {
        auto* dst = target.get_component<DirectionalLightComponent>(target_entity);
        const auto* src = runtime.get_component<DirectionalLightComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "color") dst->color = src->color;
        else if (property == "intensity") dst->intensity = src->intensity;
        else if (property == "enabled") dst->enabled = src->enabled;
        else if (property == "casts_shadows") dst->casts_shadows = src->casts_shadows;
        else if (property == "shadow_distance") dst->shadow_distance = src->shadow_distance;
        else return false;
        return true;
    }
    if (component == "AmbientLight") {
        auto* dst = target.get_component<AmbientLightComponent>(target_entity);
        const auto* src = runtime.get_component<AmbientLightComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "color") dst->color = src->color;
        else if (property == "intensity") dst->intensity = src->intensity;
        else if (property == "enabled") dst->enabled = src->enabled;
        else return false;
        return true;
    }
    if (component == "Sky") {
        auto* dst = target.get_component<SkyComponent>(target_entity);
        const auto* src = runtime.get_component<SkyComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "zenith_color") dst->zenith_color = src->zenith_color;
        else if (property == "horizon_color") dst->horizon_color = src->horizon_color;
        else if (property == "intensity") dst->intensity = src->intensity;
        else if (property == "enabled") dst->enabled = src->enabled;
        else return false;
        return true;
    }
    if (component == "Interactable") {
        auto* dst = target.get_component<InteractableComponent>(target_entity);
        const auto* src = runtime.get_component<InteractableComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "prompt") dst->prompt = src->prompt;
        else if (property == "enabled") dst->enabled = src->enabled;
        else return false;
        return true;
    }
    if (component == "Door") {
        auto* dst = target.get_component<DoorComponent>(target_entity);
        const auto* src = runtime.get_component<DoorComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "motion") dst->motion = src->motion;
        else if (property == "open_amount") dst->open_amount = src->open_amount;
        else if (property == "duration_seconds") dst->duration_seconds = src->duration_seconds;
        else if (property == "required_item") dst->required_item = src->required_item;
        else if (property == "consume_required_item") dst->consume_required_item = src->consume_required_item;
        else if (property == "starts_open") dst->starts_open = src->starts_open;
        else return false;
        return true;
    }
    if (component == "Health") {
        auto* dst = target.get_component<HealthComponent>(target_entity);
        const auto* src = runtime.get_component<HealthComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "maximum") dst->maximum = src->maximum;
        else if (property == "current") dst->current = src->current;
        else if (property == "invulnerable") dst->invulnerable = src->invulnerable;
        else return false;
        return true;
    }
    if (component == "Inventory") {
        auto* dst = target.get_component<InventoryComponent>(target_entity);
        const auto* src = runtime.get_component<InventoryComponent>(runtime_entity);
        if (!dst || !src || property != "items") return false;
        dst->items = src->items;
        return true;
    }
    if (component == "Pickup") {
        auto* dst = target.get_component<PickupComponent>(target_entity);
        const auto* src = runtime.get_component<PickupComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "item_id") dst->item_id = src->item_id;
        else if (property == "display_name") dst->display_name = src->display_name;
        else if (property == "quantity") dst->quantity = src->quantity;
        else if (property == "destroy_on_pickup") dst->destroy_on_pickup = src->destroy_on_pickup;
        else return false;
        return true;
    }
    if (component == "PlayerController") {
        auto* dst = target.get_component<PlayerControllerComponent>(target_entity);
        const auto* src = runtime.get_component<PlayerControllerComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "view_mode") dst->view_mode = src->view_mode;
        else if (property == "move_speed") dst->move_speed = src->move_speed;
        else if (property == "fast_multiplier") dst->fast_multiplier = src->fast_multiplier;
        else if (property == "look_sensitivity") dst->look_sensitivity = src->look_sensitivity;
        else if (property == "interaction_distance") dst->interaction_distance = src->interaction_distance;
        else if (property == "interaction_radius") dst->interaction_radius = src->interaction_radius;
        else if (property == "camera_distance") dst->camera_distance = src->camera_distance;
        else if (property == "camera_height") dst->camera_height = src->camera_height;
        else if (property == "camera_shoulder_offset") dst->camera_shoulder_offset = src->camera_shoulder_offset;
        else if (property == "camera_min_pitch") dst->camera_min_pitch = src->camera_min_pitch;
        else if (property == "camera_max_pitch") dst->camera_max_pitch = src->camera_max_pitch;
        else if (property == "orient_to_movement") dst->orient_to_movement = src->orient_to_movement;
        else if (property == "enabled") dst->enabled = src->enabled;
        else return false;
        return true;
    }
    if (component == "BoxCollider") {
        auto* dst = target.get_component<BoxColliderComponent>(target_entity);
        const auto* src = runtime.get_component<BoxColliderComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "half_extents") dst->half_extents = src->half_extents;
        else if (property == "offset") dst->offset = src->offset;
        else if (property == "enabled") dst->enabled = src->enabled;
        else if (property == "solid") dst->solid = src->solid;
        else return false;
        return true;
    }
    if (component == "CharacterBody") {
        auto* dst = target.get_component<CharacterBodyComponent>(target_entity);
        const auto* src = runtime.get_component<CharacterBodyComponent>(runtime_entity);
        if (!dst || !src) return false;
        if (property == "radius") dst->radius = src->radius;
        else if (property == "height") dst->height = src->height;
        else if (property == "eye_height") dst->eye_height = src->eye_height;
        else if (property == "gravity") dst->gravity = src->gravity;
        else if (property == "jump_speed") dst->jump_speed = src->jump_speed;
        else if (property == "max_fall_speed") dst->max_fall_speed = src->max_fall_speed;
        else if (property == "enabled") dst->enabled = src->enabled;
        else return false;
        return true;
    }
    return false;
}

bool apply_change(Scene& authored, const Scene& runtime, const SceneChange& change, std::string& error) {
    const EntityId authored_entity = authored.find_entity_by_persistent_id(change.entity_persistent_id);
    const EntityId runtime_entity = runtime.find_entity_by_persistent_id(change.entity_persistent_id);

    switch (change.kind) {
    case SceneChangeKind::EntityAdded:
        if (runtime_entity == InvalidEntity) {
            error = "Runtime entity no longer exists: " + change.entity_persistent_id;
            return false;
        }
        copy_authorable_entity(authored, runtime, runtime_entity);
        return true;
    case SceneChangeKind::EntityRemoved:
        if (authored_entity == InvalidEntity) return true;
        return authored.destroy_entity(authored_entity);
    case SceneChangeKind::EntityRenamed:
        if (authored_entity == InvalidEntity || runtime_entity == InvalidEntity) {
            error = "Entity missing while applying rename: " + change.entity_persistent_id;
            return false;
        }
        authored.set_entity_name(authored_entity, runtime.entity_name(runtime_entity));
        return true;
    case SceneChangeKind::ParentChanged:
        if (authored_entity == InvalidEntity || runtime_entity == InvalidEntity) {
            error = "Entity missing while applying parent change: " + change.entity_persistent_id;
            return false;
        }
        if (const auto* parent = runtime.get_component<ParentComponent>(runtime_entity)) {
            authored.add_component<ParentComponent>(authored_entity, *parent);
        } else {
            authored.remove_component<ParentComponent>(authored_entity);
        }
        return true;
    case SceneChangeKind::ComponentAdded:
        if (authored_entity == InvalidEntity || runtime_entity == InvalidEntity) {
            error = "Entity missing while applying component addition: " + change.entity_persistent_id;
            return false;
        }
        if (!copy_component_by_name(authored, authored_entity, runtime, runtime_entity, change.component)) {
            error = "Unsupported component addition: " + change.component;
            return false;
        }
        return true;
    case SceneChangeKind::ComponentRemoved:
        if (authored_entity == InvalidEntity) return true;
        if (!remove_component_by_name(authored, authored_entity, change.component)) {
            error = "Unsupported or absent component removal: " + change.component;
            return false;
        }
        return true;
    case SceneChangeKind::PropertyChanged:
        if (authored_entity == InvalidEntity || runtime_entity == InvalidEntity) {
            error = "Entity missing while applying property change: " + change.entity_persistent_id;
            return false;
        }
        if (!apply_property_change(authored, authored_entity, runtime, runtime_entity, change)) {
            error = "Unsupported property change: " + change.component + "." + change.property;
            return false;
        }
        return true;
    }
    error = "Unknown Seed scene change kind.";
    return false;
}

} // namespace

std::size_t SceneDiff::safe_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(changes.begin(), changes.end(), [](const SceneChange& change) {
        return change.safety == SceneChangeSafety::Safe;
    }));
}

std::size_t SceneDiff::review_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(changes.begin(), changes.end(), [](const SceneChange& change) {
        return change.safety == SceneChangeSafety::Review;
    }));
}

std::size_t SceneDiff::runtime_state_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(changes.begin(), changes.end(), [](const SceneChange& change) {
        return change.safety == SceneChangeSafety::RuntimeState;
    }));
}

SceneDiff SceneDiffer::compare(const Scene& authored, const Scene& runtime) {
    SceneDiff diff;
    std::unordered_map<std::string, EntityId> authored_entities;
    std::unordered_map<std::string, EntityId> runtime_entities;

    authored.for_each_entity([&](EntityId entity, const std::string&) {
        authored_entities.emplace(authored.entity_persistent_id(entity), entity);
    });
    runtime.for_each_entity([&](EntityId entity, const std::string&) {
        runtime_entities.emplace(runtime.entity_persistent_id(entity), entity);
    });

    for (const auto& [persistent_id, authored_entity] : authored_entities) {
        const auto found_runtime = runtime_entities.find(persistent_id);
        if (found_runtime == runtime_entities.end()) {
            add_change(
                diff,
                SceneChangeKind::EntityRemoved,
                SceneChangeSafety::Review,
                persistent_id,
                authored.entity_name(authored_entity)
            );
            continue;
        }

        const EntityId runtime_entity = found_runtime->second;
        const std::string& entity_name = authored.entity_name(authored_entity);
        if (entity_name != runtime.entity_name(runtime_entity)) {
            add_change(
                diff,
                SceneChangeKind::EntityRenamed,
                SceneChangeSafety::Safe,
                persistent_id,
                entity_name,
                {},
                "name",
                entity_name,
                runtime.entity_name(runtime_entity)
            );
        }

        const auto* authored_parent = authored.get_component<ParentComponent>(authored_entity);
        const auto* runtime_parent = runtime.get_component<ParentComponent>(runtime_entity);
        const std::string authored_parent_id = authored_parent ? authored_parent->parent_persistent_id : std::string{};
        const std::string runtime_parent_id = runtime_parent ? runtime_parent->parent_persistent_id : std::string{};
        if (authored_parent_id != runtime_parent_id) {
            add_change(
                diff,
                SceneChangeKind::ParentChanged,
                SceneChangeSafety::Review,
                persistent_id,
                entity_name,
                "Parent",
                "parent_persistent_id",
                authored_parent_id,
                runtime_parent_id
            );
        }

        if (compare_presence<TransformComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Transform", SceneChangeSafety::Review) == ComponentPresence::Both) {
            const auto& a = *authored.get_component<TransformComponent>(authored_entity);
            const auto& b = *runtime.get_component<TransformComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Review, persistent_id, entity_name, "Transform", "position", a.position, b.position);
            add_property(diff, SceneChangeSafety::Review, persistent_id, entity_name, "Transform", "rotation_degrees", a.rotation_degrees, b.rotation_degrees);
            add_property(diff, SceneChangeSafety::Review, persistent_id, entity_name, "Transform", "scale", a.scale, b.scale);
        }

        if (compare_presence<CameraComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Camera") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<CameraComponent>(authored_entity);
            const auto& b = *runtime.get_component<CameraComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Camera", "field_of_view_degrees", a.field_of_view_degrees, b.field_of_view_degrees);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Camera", "near_plane", a.near_plane, b.near_plane);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Camera", "far_plane", a.far_plane, b.far_plane);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Camera", "enabled", a.enabled, b.enabled);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Camera", "primary", a.primary, b.primary);
        }

        if (compare_presence<MeshComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Mesh") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<MeshComponent>(authored_entity);
            const auto& b = *runtime.get_component<MeshComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Mesh", "visible", a.visible, b.visible);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Mesh", "cast_shadows", a.cast_shadows, b.cast_shadows);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Mesh", "receive_shadows", a.receive_shadows, b.receive_shadows);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Mesh", "asset_id", a.asset_id, b.asset_id);
        }

        if (compare_presence<MaterialComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Material") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<MaterialComponent>(authored_entity);
            const auto& b = *runtime.get_component<MaterialComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "base_color", a.base_color, b.base_color);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "metallic", a.metallic, b.metallic);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "roughness", a.roughness, b.roughness);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "normal_scale", a.normal_scale, b.normal_scale);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "use_base_color_texture", a.use_base_color_texture, b.use_base_color_texture);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "use_metallic_roughness_texture", a.use_metallic_roughness_texture, b.use_metallic_roughness_texture);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "use_normal_texture", a.use_normal_texture, b.use_normal_texture);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Material", "asset_id", a.asset_id, b.asset_id);
        }

        if (compare_presence<DirectionalLightComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "DirectionalLight") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<DirectionalLightComponent>(authored_entity);
            const auto& b = *runtime.get_component<DirectionalLightComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "DirectionalLight", "color", a.color, b.color);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "DirectionalLight", "intensity", a.intensity, b.intensity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "DirectionalLight", "enabled", a.enabled, b.enabled);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "DirectionalLight", "casts_shadows", a.casts_shadows, b.casts_shadows);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "DirectionalLight", "shadow_distance", a.shadow_distance, b.shadow_distance);
        }

        if (compare_presence<AmbientLightComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "AmbientLight") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<AmbientLightComponent>(authored_entity);
            const auto& b = *runtime.get_component<AmbientLightComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "AmbientLight", "color", a.color, b.color);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "AmbientLight", "intensity", a.intensity, b.intensity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "AmbientLight", "enabled", a.enabled, b.enabled);
        }

        if (compare_presence<SkyComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Sky") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<SkyComponent>(authored_entity);
            const auto& b = *runtime.get_component<SkyComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Sky", "zenith_color", a.zenith_color, b.zenith_color);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Sky", "horizon_color", a.horizon_color, b.horizon_color);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Sky", "intensity", a.intensity, b.intensity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Sky", "enabled", a.enabled, b.enabled);
        }

        if (compare_presence<InteractableComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Interactable") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<InteractableComponent>(authored_entity);
            const auto& b = *runtime.get_component<InteractableComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Interactable", "prompt", a.prompt, b.prompt);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Interactable", "enabled", a.enabled, b.enabled);
        }

        if (compare_presence<DoorComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Door") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<DoorComponent>(authored_entity);
            const auto& b = *runtime.get_component<DoorComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Door", "motion", std::string{door_motion_name(a.motion)}, std::string{door_motion_name(b.motion)});
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Door", "open_amount", a.open_amount, b.open_amount);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Door", "duration_seconds", a.duration_seconds, b.duration_seconds);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Door", "required_item", a.required_item, b.required_item);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Door", "consume_required_item", a.consume_required_item, b.consume_required_item);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Door", "starts_open", a.starts_open, b.starts_open);
        }

        if (compare_presence<HealthComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Health") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<HealthComponent>(authored_entity);
            const auto& b = *runtime.get_component<HealthComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Health", "maximum", a.maximum, b.maximum);
            add_property(diff, SceneChangeSafety::RuntimeState, persistent_id, entity_name, "Health", "current", a.current, b.current);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Health", "invulnerable", a.invulnerable, b.invulnerable);
        }

        if (compare_presence<InventoryComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Inventory") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<InventoryComponent>(authored_entity);
            const auto& b = *runtime.get_component<InventoryComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::RuntimeState, persistent_id, entity_name, "Inventory", "items", inventory_string(a), inventory_string(b));
        }

        if (compare_presence<PickupComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "Pickup") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<PickupComponent>(authored_entity);
            const auto& b = *runtime.get_component<PickupComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Pickup", "item_id", a.item_id, b.item_id);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Pickup", "display_name", a.display_name, b.display_name);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Pickup", "quantity", a.quantity, b.quantity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "Pickup", "destroy_on_pickup", a.destroy_on_pickup, b.destroy_on_pickup);
        }

        if (compare_presence<PlayerControllerComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "PlayerController") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<PlayerControllerComponent>(authored_entity);
            const auto& b = *runtime.get_component<PlayerControllerComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "view_mode", std::string{player_view_mode_name(a.view_mode)}, std::string{player_view_mode_name(b.view_mode)});
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "move_speed", a.move_speed, b.move_speed);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "fast_multiplier", a.fast_multiplier, b.fast_multiplier);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "look_sensitivity", a.look_sensitivity, b.look_sensitivity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "interaction_distance", a.interaction_distance, b.interaction_distance);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "interaction_radius", a.interaction_radius, b.interaction_radius);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "camera_distance", a.camera_distance, b.camera_distance);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "camera_height", a.camera_height, b.camera_height);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "camera_shoulder_offset", a.camera_shoulder_offset, b.camera_shoulder_offset);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "camera_min_pitch", a.camera_min_pitch, b.camera_min_pitch);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "camera_max_pitch", a.camera_max_pitch, b.camera_max_pitch);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "orient_to_movement", a.orient_to_movement, b.orient_to_movement);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "PlayerController", "enabled", a.enabled, b.enabled);
        }

        if (compare_presence<BoxColliderComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "BoxCollider") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<BoxColliderComponent>(authored_entity);
            const auto& b = *runtime.get_component<BoxColliderComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "BoxCollider", "half_extents", a.half_extents, b.half_extents);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "BoxCollider", "offset", a.offset, b.offset);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "BoxCollider", "enabled", a.enabled, b.enabled);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "BoxCollider", "solid", a.solid, b.solid);
        }

        if (compare_presence<CharacterBodyComponent>(authored, authored_entity, runtime, runtime_entity, diff, persistent_id, entity_name, "CharacterBody") == ComponentPresence::Both) {
            const auto& a = *authored.get_component<CharacterBodyComponent>(authored_entity);
            const auto& b = *runtime.get_component<CharacterBodyComponent>(runtime_entity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "radius", a.radius, b.radius);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "height", a.height, b.height);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "eye_height", a.eye_height, b.eye_height);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "gravity", a.gravity, b.gravity);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "jump_speed", a.jump_speed, b.jump_speed);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "max_fall_speed", a.max_fall_speed, b.max_fall_speed);
            add_property(diff, SceneChangeSafety::Safe, persistent_id, entity_name, "CharacterBody", "enabled", a.enabled, b.enabled);
        }
    }

    for (const auto& [persistent_id, runtime_entity] : runtime_entities) {
        if (authored_entities.contains(persistent_id)) continue;
        add_change(
            diff,
            SceneChangeKind::EntityAdded,
            SceneChangeSafety::Review,
            persistent_id,
            runtime.entity_name(runtime_entity)
        );
    }

    return diff;
}

SceneApplyResult SceneDiffer::apply(
    Scene& authored,
    const Scene& runtime,
    const SceneDiff& diff,
    std::span<const std::size_t> selected_change_indices
) {
    SceneApplyResult result;
    for (const std::size_t index : selected_change_indices) {
        if (index >= diff.changes.size()) {
            ++result.failed;
            result.errors.push_back("Scene change index is out of range: " + std::to_string(index));
            continue;
        }
        std::string error;
        if (apply_change(authored, runtime, diff.changes[index], error)) {
            ++result.applied;
        } else {
            ++result.failed;
            result.errors.push_back(error.empty() ? "Could not apply Seed scene change." : std::move(error));
        }
    }
    return result;
}

SceneApplyResult SceneDiffer::apply_all_safe(Scene& authored, const Scene& runtime, const SceneDiff& diff) {
    std::vector<std::size_t> safe_indices;
    safe_indices.reserve(diff.safe_count());
    for (std::size_t index = 0; index < diff.changes.size(); ++index) {
        if (diff.changes[index].safety == SceneChangeSafety::Safe) safe_indices.push_back(index);
    }
    SceneApplyResult result = apply(authored, runtime, diff, safe_indices);
    result.skipped = diff.changes.size() - safe_indices.size();
    return result;
}

} // namespace seed
