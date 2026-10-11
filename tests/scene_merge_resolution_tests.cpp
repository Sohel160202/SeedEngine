#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"
#include "seed/scene/SceneMergeResolution.h"

#include <cassert>
#include <iostream>

using namespace seed;

namespace {

Scene make_base() {
    Scene scene;
    const auto player = scene.create_entity("Player", "player");
    scene.add_component<TransformComponent>(player);
    return scene;
}

void ours_can_resolve_property_conflict() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;

    ours.get_component<TransformComponent>(ours.find_entity_by_persistent_id("player"))->position.x = 10.0f;
    theirs.get_component<TransformComponent>(theirs.find_entity_by_persistent_id("player"))->position.x = 99.0f;

    SceneMergeResolutionPlan plan;
    plan.choices = {SceneMergeResolutionChoice::Ours};
    const auto result = SceneMergeResolver::resolve(base, ours, theirs, plan);

    assert(result.complete());
    assert(result.resolved_conflicts.size() == 1);
    const auto player = result.merged_scene.find_entity_by_persistent_id("player");
    assert(result.merged_scene.get_component<TransformComponent>(player)->position.x == 10.0f);
}

void theirs_can_resolve_property_conflict() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;

    ours.set_entity_name(ours.find_entity_by_persistent_id("player"), "Knight");
    theirs.set_entity_name(theirs.find_entity_by_persistent_id("player"), "Wizard");

    SceneMergeResolutionPlan plan;
    plan.choices = {SceneMergeResolutionChoice::Theirs};
    const auto result = SceneMergeResolver::resolve(base, ours, theirs, plan);

    assert(result.complete());
    const auto player = result.merged_scene.find_entity_by_persistent_id("player");
    assert(result.merged_scene.entity_name(player) == "Wizard");
}

void unresolved_conflicts_are_preserved() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;

    ours.get_component<TransformComponent>(ours.find_entity_by_persistent_id("player"))->position.x = 10.0f;
    theirs.get_component<TransformComponent>(theirs.find_entity_by_persistent_id("player"))->position.x = 99.0f;

    const auto result = SceneMergeResolver::resolve(base, ours, theirs, {});
    assert(!result.complete());
    assert(result.unresolved_conflicts.size() == 1);
}

void removal_vs_modify_is_resolved_as_a_whole() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;

    assert(ours.destroy_entity(ours.find_entity_by_persistent_id("player")));
    theirs.set_entity_name(theirs.find_entity_by_persistent_id("player"), "ChangedPlayer");

    SceneMergeResolutionPlan plan;
    plan.choices = {SceneMergeResolutionChoice::Ours};
    const auto result = SceneMergeResolver::resolve(base, ours, theirs, plan);
    assert(result.complete());
    assert(result.merged_scene.find_entity_by_persistent_id("player") == InvalidEntity);
}

} // namespace

int main() {
    ours_can_resolve_property_conflict();
    theirs_can_resolve_property_conflict();
    unresolved_conflicts_are_preserved();
    removal_vs_modify_is_resolved_as_a_whole();
    std::cout << "Seed semantic merge resolution tests passed.\n";
    return 0;
}
