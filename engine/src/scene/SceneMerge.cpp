#include "seed/scene/SceneMerge.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace seed {
namespace {

struct ChangeKey {
    std::string entity;
    std::string component;
    std::string property;
    SceneChangeKind kind{SceneChangeKind::PropertyChanged};

    auto operator<=>(const ChangeKey&) const = default;
};

ChangeKey key_for(const SceneChange& change) {
    return {change.entity_persistent_id, change.component, change.property, change.kind};
}

bool same_value(const SceneChangeValue& a, const SceneChangeValue& b) {
    return a == b;
}

bool equivalent_change(const SceneChange& a, const SceneChange& b) {
    return a.kind == b.kind &&
           a.entity_persistent_id == b.entity_persistent_id &&
           a.component == b.component &&
           a.property == b.property &&
           same_value(a.after_value, b.after_value);
}

bool is_entity_removal(const SceneChange& change) {
    return change.kind == SceneChangeKind::EntityRemoved;
}

bool is_entity_addition(const SceneChange& change) {
    return change.kind == SceneChangeKind::EntityAdded;
}

std::string describe_conflict(const SceneChange& ours, const SceneChange& theirs) {
    std::ostringstream out;
    out << "Both branches changed entity '" << ours.entity_name << "' ("
        << ours.entity_persistent_id << ")";
    if (!ours.component.empty()) out << ", component " << ours.component;
    if (!ours.property.empty()) out << ", property " << ours.property;
    out << " differently.";
    return out.str();
}

SceneMergeConflict make_conflict(
    SceneMergeConflictKind kind,
    const SceneChange& ours,
    const SceneChange& theirs
) {
    SceneMergeConflict conflict;
    conflict.kind = kind;
    conflict.entity_persistent_id = ours.entity_persistent_id;
    conflict.entity_name = !ours.entity_name.empty() ? ours.entity_name : theirs.entity_name;
    conflict.component = !ours.component.empty() ? ours.component : theirs.component;
    conflict.property = !ours.property.empty() ? ours.property : theirs.property;
    conflict.base_value = ours.before_value;
    conflict.ours_value = ours.after_value;
    conflict.theirs_value = theirs.after_value;
    conflict.description = describe_conflict(ours, theirs);
    return conflict;
}

bool branch_modifies_entity(const SceneDiff& diff, const std::string& persistent_id) {
    return std::any_of(diff.changes.begin(), diff.changes.end(), [&](const SceneChange& change) {
        return change.entity_persistent_id == persistent_id && !is_entity_removal(change);
    });
}

} // namespace

SceneMergeResult SceneMerger::merge(
    const Scene& base_scene,
    const Scene& ours_scene,
    const Scene& theirs_scene
) {
    SceneMergeResult result;
    result.merged_scene = base_scene;

    const SceneDiff ours_diff = SceneDiffer::compare(base_scene, ours_scene);
    const SceneDiff theirs_diff = SceneDiffer::compare(base_scene, theirs_scene);
    result.ours_change_count = ours_diff.size();
    result.theirs_change_count = theirs_diff.size();

    std::set<std::size_t> conflicted_ours;
    std::set<std::size_t> conflicted_theirs;

    // Removal versus any modification on the other branch is a structural
    // conflict. Detect it before property-level matching so we never partially
    // apply edits to an entity another branch deleted.
    for (std::size_t i = 0; i < ours_diff.changes.size(); ++i) {
        const auto& ours = ours_diff.changes[i];
        if (!is_entity_removal(ours)) continue;
        if (!branch_modifies_entity(theirs_diff, ours.entity_persistent_id)) continue;

        for (std::size_t j = 0; j < theirs_diff.changes.size(); ++j) {
            const auto& theirs = theirs_diff.changes[j];
            if (theirs.entity_persistent_id != ours.entity_persistent_id || is_entity_removal(theirs)) continue;
            result.conflicts.push_back(make_conflict(SceneMergeConflictKind::EntityRemovedVsModified, ours, theirs));
            conflicted_ours.insert(i);
            conflicted_theirs.insert(j);
            break;
        }
    }
    for (std::size_t j = 0; j < theirs_diff.changes.size(); ++j) {
        const auto& theirs = theirs_diff.changes[j];
        if (!is_entity_removal(theirs)) continue;
        if (!branch_modifies_entity(ours_diff, theirs.entity_persistent_id)) continue;

        for (std::size_t i = 0; i < ours_diff.changes.size(); ++i) {
            const auto& ours = ours_diff.changes[i];
            if (ours.entity_persistent_id != theirs.entity_persistent_id || is_entity_removal(ours)) continue;
            result.conflicts.push_back(make_conflict(SceneMergeConflictKind::EntityRemovedVsModified, ours, theirs));
            conflicted_ours.insert(i);
            conflicted_theirs.insert(j);
            break;
        }
    }

    std::map<ChangeKey, std::size_t> ours_by_key;
    for (std::size_t i = 0; i < ours_diff.changes.size(); ++i) {
        if (!conflicted_ours.contains(i)) ours_by_key.emplace(key_for(ours_diff.changes[i]), i);
    }

    for (std::size_t j = 0; j < theirs_diff.changes.size(); ++j) {
        if (conflicted_theirs.contains(j)) continue;
        const auto& theirs = theirs_diff.changes[j];
        const auto it = ours_by_key.find(key_for(theirs));
        if (it == ours_by_key.end()) continue;

        const std::size_t i = it->second;
        const auto& ours = ours_diff.changes[i];
        if (equivalent_change(ours, theirs)) continue;

        const auto kind = (is_entity_addition(ours) && is_entity_addition(theirs))
            ? SceneMergeConflictKind::EntityAddedDifferently
            : SceneMergeConflictKind::SameTargetChangedDifferently;
        result.conflicts.push_back(make_conflict(kind, ours, theirs));
        conflicted_ours.insert(i);
        conflicted_theirs.insert(j);
    }

    // Both diffs are relative to the common base. Apply only those original
    // branch deltas to the merged base copy. Re-diffing the partially merged
    // scene against the other branch would incorrectly interpret already-
    // merged changes as reversions requested by that branch.
    std::vector<std::size_t> ours_to_apply;
    for (std::size_t i = 0; i < ours_diff.changes.size(); ++i) {
        if (!conflicted_ours.contains(i)) ours_to_apply.push_back(i);
    }
    if (!ours_to_apply.empty()) {
        const auto applied = SceneDiffer::apply(result.merged_scene, ours_scene, ours_diff, ours_to_apply);
        (void)applied;
        for (const auto index : ours_to_apply) result.auto_merged_changes.push_back(ours_diff.changes[index]);
    }

    std::vector<std::size_t> theirs_to_apply;
    for (std::size_t j = 0; j < theirs_diff.changes.size(); ++j) {
        if (!conflicted_theirs.contains(j)) theirs_to_apply.push_back(j);
    }
    if (!theirs_to_apply.empty()) {
        const auto applied = SceneDiffer::apply(result.merged_scene, theirs_scene, theirs_diff, theirs_to_apply);
        (void)applied;
        for (const auto index : theirs_to_apply) result.auto_merged_changes.push_back(theirs_diff.changes[index]);
    }

    return result;
}

} // namespace seed
