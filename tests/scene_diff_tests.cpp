#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/scene/Scene.h"
#include "seed/scene/SceneDiff.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool near(float a, float b, float epsilon = 0.001f) {
    return std::fabs(a - b) <= epsilon;
}

std::size_t find_change(
    const seed::SceneDiff& diff,
    const std::string& persistent_id,
    seed::SceneChangeKind kind,
    const std::string& component = {},
    const std::string& property = {}
) {
    for (std::size_t index = 0; index < diff.changes.size(); ++index) {
        const auto& change = diff.changes[index];
        if (change.entity_persistent_id != persistent_id || change.kind != kind) continue;
        if (!component.empty() && change.component != component) continue;
        if (!property.empty() && change.property != property) continue;
        return index;
    }
    return static_cast<std::size_t>(-1);
}

} // namespace

int main() {
    seed::Scene authored;

    const seed::EntityId player = authored.create_entity("Player", "player-1");
    auto& player_transform = authored.add_component<seed::TransformComponent>(player);
    player_transform.position = {0.0f, 1.65f, 0.0f};
    auto& player_controller = authored.add_component<seed::PlayerControllerComponent>(player);
    player_controller.view_mode = seed::PlayerViewMode::ThirdPerson;
    player_controller.move_speed = 4.0f;
    auto& health = authored.add_component<seed::HealthComponent>(player);
    health.maximum = 100.0f;
    health.current = 100.0f;

    const seed::EntityId crate = authored.create_entity("Crate", "crate-1");
    authored.add_component<seed::TransformComponent>(crate);

    seed::Scene runtime = authored;
    const seed::EntityId runtime_player = runtime.find_entity_by_persistent_id("player-1");
    assert(runtime_player != seed::InvalidEntity);

    runtime.set_entity_name(runtime_player, "Hero");
    auto* runtime_transform = runtime.get_component<seed::TransformComponent>(runtime_player);
    auto* runtime_controller = runtime.get_component<seed::PlayerControllerComponent>(runtime_player);
    auto* runtime_health = runtime.get_component<seed::HealthComponent>(runtime_player);
    assert(runtime_transform && runtime_controller && runtime_health);

    // Typical runtime movement: visible in the diff, but requires review.
    runtime_transform->position = {7.0f, 1.65f, -2.0f};

    // Typical tuning change: safe to offer for one-click apply.
    runtime_controller->move_speed = 6.25f;

    // Typical gameplay state: visible, but never silently applied.
    runtime_health->current = 25.0f;

    seed::BoxColliderComponent collider;
    collider.half_extents = {0.4f, 0.9f, 0.4f};
    runtime.add_component<seed::BoxColliderComponent>(runtime_player, collider);

    const seed::EntityId runtime_crate = runtime.find_entity_by_persistent_id("crate-1");
    assert(runtime_crate != seed::InvalidEntity);
    assert(runtime.destroy_entity(runtime_crate));

    const seed::EntityId spawned = runtime.create_entity("Runtime Spawn", "spawned-1");
    auto& spawned_transform = runtime.add_component<seed::TransformComponent>(spawned);
    spawned_transform.position = {3.0f, 0.5f, 4.0f};
    seed::PickupComponent spawned_pickup;
    spawned_pickup.item_id = "RuntimeGem";
    spawned_pickup.display_name = "Runtime Gem";
    runtime.add_component<seed::PickupComponent>(spawned, spawned_pickup);

    const seed::SceneDiff diff = seed::SceneDiffer::compare(authored, runtime);
    assert(!diff.empty());

    const std::size_t rename_index = find_change(diff, "player-1", seed::SceneChangeKind::EntityRenamed);
    const std::size_t position_index = find_change(diff, "player-1", seed::SceneChangeKind::PropertyChanged, "Transform", "position");
    const std::size_t speed_index = find_change(diff, "player-1", seed::SceneChangeKind::PropertyChanged, "PlayerController", "move_speed");
    const std::size_t health_index = find_change(diff, "player-1", seed::SceneChangeKind::PropertyChanged, "Health", "current");
    const std::size_t collider_index = find_change(diff, "player-1", seed::SceneChangeKind::ComponentAdded, "BoxCollider");
    const std::size_t removed_index = find_change(diff, "crate-1", seed::SceneChangeKind::EntityRemoved);
    const std::size_t spawned_index = find_change(diff, "spawned-1", seed::SceneChangeKind::EntityAdded);

    const std::size_t missing = static_cast<std::size_t>(-1);
    assert(rename_index != missing);
    assert(position_index != missing);
    assert(speed_index != missing);
    assert(health_index != missing);
    assert(collider_index != missing);
    assert(removed_index != missing);
    assert(spawned_index != missing);

    assert(diff.changes[rename_index].safety == seed::SceneChangeSafety::Safe);
    assert(diff.changes[position_index].safety == seed::SceneChangeSafety::Review);
    assert(diff.changes[speed_index].safety == seed::SceneChangeSafety::Safe);
    assert(diff.changes[health_index].safety == seed::SceneChangeSafety::RuntimeState);
    assert(diff.changes[spawned_index].safety == seed::SceneChangeSafety::Review);

    // Selective apply proves Seed copies the exact property rather than replacing
    // the entire runtime component and leaking unrelated gameplay state.
    const std::vector<std::size_t> selected{rename_index, speed_index, spawned_index};
    const seed::SceneApplyResult selected_result = seed::SceneDiffer::apply(authored, runtime, diff, selected);
    assert(selected_result.success());
    assert(selected_result.applied == selected.size());

    const seed::EntityId applied_player = authored.find_entity_by_persistent_id("player-1");
    assert(applied_player != seed::InvalidEntity);
    assert(authored.entity_name(applied_player) == "Hero");
    const auto* applied_transform = authored.get_component<seed::TransformComponent>(applied_player);
    const auto* applied_controller = authored.get_component<seed::PlayerControllerComponent>(applied_player);
    const auto* applied_health = authored.get_component<seed::HealthComponent>(applied_player);
    assert(applied_transform && applied_controller && applied_health);
    assert(near(applied_transform->position.x, 0.0f));
    assert(near(applied_controller->move_speed, 6.25f));
    assert(near(applied_health->current, 100.0f));

    const seed::EntityId promoted = authored.find_entity_by_persistent_id("spawned-1");
    assert(promoted != seed::InvalidEntity);
    const auto* promoted_transform = authored.get_component<seed::TransformComponent>(promoted);
    const auto* promoted_pickup = authored.get_component<seed::PickupComponent>(promoted);
    assert(promoted_transform && promoted_pickup);
    assert(near(promoted_transform->position.x, 3.0f));
    assert(promoted_pickup->item_id == "RuntimeGem");

    // Rebuild from the original state and apply only automatically safe changes.
    seed::Scene authored_safe;
    const seed::EntityId safe_player = authored_safe.create_entity("Player", "player-1");
    auto& safe_transform = authored_safe.add_component<seed::TransformComponent>(safe_player);
    safe_transform.position = {0.0f, 1.65f, 0.0f};
    auto& safe_controller = authored_safe.add_component<seed::PlayerControllerComponent>(safe_player);
    safe_controller.view_mode = seed::PlayerViewMode::ThirdPerson;
    safe_controller.move_speed = 4.0f;
    auto& safe_health = authored_safe.add_component<seed::HealthComponent>(safe_player);
    safe_health.current = 100.0f;
    const seed::EntityId safe_crate = authored_safe.create_entity("Crate", "crate-1");
    authored_safe.add_component<seed::TransformComponent>(safe_crate);

    const seed::SceneDiff safe_diff = seed::SceneDiffer::compare(authored_safe, runtime);
    const seed::SceneApplyResult safe_result = seed::SceneDiffer::apply_all_safe(authored_safe, runtime, safe_diff);
    assert(safe_result.success());
    assert(safe_result.applied == safe_diff.safe_count());
    assert(safe_result.skipped == safe_diff.size() - safe_diff.safe_count());

    const seed::EntityId safe_applied_player = authored_safe.find_entity_by_persistent_id("player-1");
    const auto* safe_applied_transform = authored_safe.get_component<seed::TransformComponent>(safe_applied_player);
    const auto* safe_applied_controller = authored_safe.get_component<seed::PlayerControllerComponent>(safe_applied_player);
    const auto* safe_applied_health = authored_safe.get_component<seed::HealthComponent>(safe_applied_player);
    assert(authored_safe.entity_name(safe_applied_player) == "Hero");
    assert(safe_applied_transform && safe_applied_controller && safe_applied_health);
    assert(near(safe_applied_transform->position.x, 0.0f));
    assert(near(safe_applied_controller->move_speed, 6.25f));
    assert(near(safe_applied_health->current, 100.0f));
    assert(authored_safe.has_component<seed::BoxColliderComponent>(safe_applied_player));
    assert(authored_safe.find_entity_by_persistent_id("crate-1") != seed::InvalidEntity);
    assert(authored_safe.find_entity_by_persistent_id("spawned-1") == seed::InvalidEntity);

    std::cout << "Seed semantic SceneDiff tests passed.\n";
    return 0;
}
