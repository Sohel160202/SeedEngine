#include "seed/scene/Scene.h"
#include "seed/scene/SceneMerge.h"
#include "seed/scene/SceneMergeResolution.h"
#include "seed/gameplay/Components.h"

#include <cassert>
#include <iostream>

using namespace seed;

namespace {

Scene make_base() {
    Scene scene;
    const auto player = scene.create_entity("Player", "player");
    scene.add_component<TransformComponent>(player);
    const auto sun = scene.create_entity("Sun", "sun");
    scene.add_component<TransformComponent>(sun);
    return scene;
}

void independent_changes_merge_cleanly() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    const auto player = ours.find_entity_by_persistent_id("player");
    const auto sun = theirs.find_entity_by_persistent_id("sun");
    ours.get_component<TransformComponent>(player)->position.x = 10.0f;
    theirs.get_component<TransformComponent>(sun)->position.y = 20.0f;
    const auto result = SceneMerger::merge(base, ours, theirs);
    assert(result.clean());
    const auto merged_player = result.merged_scene.find_entity_by_persistent_id("player");
    const auto merged_sun = result.merged_scene.find_entity_by_persistent_id("sun");
    assert(result.merged_scene.get_component<TransformComponent>(merged_player)->position.x == 10.0f);
    assert(result.merged_scene.get_component<TransformComponent>(merged_sun)->position.y == 20.0f);
}

void same_property_changed_differently_conflicts() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    const auto ours_player = ours.find_entity_by_persistent_id("player");
    const auto theirs_player = theirs.find_entity_by_persistent_id("player");
    ours.get_component<TransformComponent>(ours_player)->position.x = 10.0f;
    theirs.get_component<TransformComponent>(theirs_player)->position.x = 99.0f;
    const auto result = SceneMerger::merge(base, ours, theirs);
    assert(!result.clean());
    assert(result.conflict_count() >= 1);
    assert(result.conflicts.front().kind == SceneMergeConflictKind::SameTargetChangedDifferently);
}

void identical_change_is_not_a_conflict() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    const auto ours_player = ours.find_entity_by_persistent_id("player");
    const auto theirs_player = theirs.find_entity_by_persistent_id("player");
    ours.set_entity_name(ours_player, "Hero");
    theirs.set_entity_name(theirs_player, "Hero");
    const auto result = SceneMerger::merge(base, ours, theirs);
    assert(result.clean());
    const auto merged_player = result.merged_scene.find_entity_by_persistent_id("player");
    assert(result.merged_scene.entity_name(merged_player) == "Hero");
}

void remove_vs_modify_conflicts() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    const auto ours_player = ours.find_entity_by_persistent_id("player");
    ours.destroy_entity(ours_player);
    const auto theirs_player = theirs.find_entity_by_persistent_id("player");
    theirs.set_entity_name(theirs_player, "ChangedPlayer");
    const auto result = SceneMerger::merge(base, ours, theirs);
    assert(!result.clean());
    assert(result.conflicts.front().kind == SceneMergeConflictKind::EntityRemovedVsModified);
}

void ours_resolution_applies_conflicting_value() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    ours.get_component<TransformComponent>(ours.find_entity_by_persistent_id("player"))->position.x = 10.0f;
    theirs.get_component<TransformComponent>(theirs.find_entity_by_persistent_id("player"))->position.x = 99.0f;
    const auto merge = SceneMerger::merge(base, ours, theirs);
    assert(merge.conflict_count() == 1);
    const auto resolved = SceneMergeResolver::resolve(base, ours, theirs, merge, {{0, SceneMergeChoice::Ours, std::nullopt}});
    assert(resolved.complete());
    assert(resolved.resolved_count == 1);
    const auto player = resolved.resolved_scene.find_entity_by_persistent_id("player");
    assert(resolved.resolved_scene.get_component<TransformComponent>(player)->position.x == 10.0f);
}

void theirs_resolution_applies_conflicting_value() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    ours.set_entity_name(ours.find_entity_by_persistent_id("player"), "OurHero");
    theirs.set_entity_name(theirs.find_entity_by_persistent_id("player"), "TheirHero");
    const auto merge = SceneMerger::merge(base, ours, theirs);
    assert(merge.conflict_count() == 1);
    const auto resolved = SceneMergeResolver::resolve(base, ours, theirs, merge, {{0, SceneMergeChoice::Theirs, std::nullopt}});
    assert(resolved.complete());
    const auto player = resolved.resolved_scene.find_entity_by_persistent_id("player");
    assert(resolved.resolved_scene.entity_name(player) == "TheirHero");
}

void unresolved_conflict_stays_incomplete() {
    const Scene base = make_base();
    Scene ours = base;
    Scene theirs = base;
    ours.set_entity_name(ours.find_entity_by_persistent_id("player"), "OurHero");
    theirs.set_entity_name(theirs.find_entity_by_persistent_id("player"), "TheirHero");
    const auto merge = SceneMerger::merge(base, ours, theirs);
    const auto resolved = SceneMergeResolver::resolve(base, ours, theirs, merge, {});
    assert(!resolved.complete());
    assert(resolved.unresolved_count == 1);
}

} // namespace

int main() {
    independent_changes_merge_cleanly();
    same_property_changed_differently_conflicts();
    identical_change_is_not_a_conflict();
    remove_vs_modify_conflicts();
    ours_resolution_applies_conflicting_value();
    theirs_resolution_applies_conflicting_value();
    unresolved_conflict_stays_incomplete();
    std::cout << "Seed semantic scene merge tests passed.\n";
    return 0;
}
