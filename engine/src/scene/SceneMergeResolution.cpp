#include "seed/scene/SceneMergeResolution.h"

#include <algorithm>
#include <span>
#include <vector>

namespace seed {
namespace {

bool same_target(const SceneChange& change, const SceneMergeConflict& conflict) {
    return change.entity_persistent_id == conflict.entity_persistent_id &&
           change.component == conflict.component &&
           change.property == conflict.property;
}

bool same_value(const SceneChangeValue& left, const SceneChangeValue& right) {
    return left == right;
}

bool matches_side(
    const SceneChange& change,
    const SceneMergeConflict& conflict,
    SceneMergeResolutionChoice choice
) {
    if (!same_target(change, conflict)) return false;

    const SceneChangeValue& expected = choice == SceneMergeResolutionChoice::Ours
        ? conflict.ours_value
        : conflict.theirs_value;
    return same_value(change.after_value, expected);
}

void append_unique(std::vector<std::size_t>& indices, std::size_t index) {
    if (std::find(indices.begin(), indices.end(), index) == indices.end()) {
        indices.push_back(index);
    }
}

void collect_conflict_changes(
    const SceneDiff& diff,
    const SceneMergeConflict& conflict,
    std::vector<std::size_t>& indices
) {
    if (conflict.kind == SceneMergeConflictKind::EntityRemovedVsModified) {
        // A structural remove-vs-modify decision applies the complete selected
        // branch delta for that entity. This prevents a partial resurrection
        // or a partially deleted entity.
        for (std::size_t index = 0; index < diff.changes.size(); ++index) {
            if (diff.changes[index].entity_persistent_id == conflict.entity_persistent_id) {
                append_unique(indices, index);
            }
        }
        return;
    }

    for (std::size_t index = 0; index < diff.changes.size(); ++index) {
        if (matches_side(diff.changes[index], conflict, choice)) {
            append_unique(indices, index);
        }
    }
}

} // namespace

SceneMergeResolutionResult SceneMergeResolver::resolve(
    const Scene& base_scene,
    const Scene& ours_scene,
    const Scene& theirs_scene,
    const SceneMergeResolutionPlan& plan
) {
    SceneMergeResolutionResult result;
    const SceneMergeResult merge = SceneMerger::merge(base_scene, ours_scene, theirs_scene);
    result.merged_scene = merge.merged_scene;

    const SceneDiff ours_diff = SceneDiffer::compare(base_scene, ours_scene);
    const SceneDiff theirs_diff = SceneDiffer::compare(base_scene, theirs_scene);

    std::vector<std::size_t> ours_indices;
    std::vector<std::size_t> theirs_indices;

    for (std::size_t conflict_index = 0; conflict_index < merge.conflicts.size(); ++conflict_index) {
        const SceneMergeConflict& conflict = merge.conflicts[conflict_index];
        const SceneMergeResolutionChoice choice = plan.choice_for(conflict_index);

        if (choice == SceneMergeResolutionChoice::Unresolved) {
            result.unresolved_conflicts.push_back(conflict);
            continue;
        }

        if (choice == SceneMergeResolutionChoice::Ours) {
            collect_conflict_changes(ours_diff, conflict, ours_indices);
        } else {
            collect_conflict_changes(theirs_diff, conflict, theirs_indices);
        }
        result.resolved_conflicts.push_back(conflict);
    }

    if (!ours_indices.empty()) {
        const SceneApplyResult applied = SceneDiffer::apply(
            result.merged_scene,
            ours_scene,
            ours_diff,
            std::span<const std::size_t>(ours_indices.data(), ours_indices.size())
        );
        if (!applied.success()) {
            result.errors.insert(result.errors.end(), applied.errors.begin(), applied.errors.end());
        }
    }

    if (!theirs_indices.empty()) {
        const SceneApplyResult applied = SceneDiffer::apply(
            result.merged_scene,
            theirs_scene,
            theirs_diff,
            std::span<const std::size_t>(theirs_indices.data(), theirs_indices.size())
        );
        if (!applied.success()) {
            result.errors.insert(result.errors.end(), applied.errors.begin(), applied.errors.end());
        }
    }

    return result;
}

} // namespace seed
