#include "seed/scene/SceneMergeResolution.h"

#include <map>
#include <sstream>

namespace seed {
namespace {

const SceneChange* find_matching_change(
    const SceneDiff& diff,
    const SceneMergeConflict& conflict,
    const SceneChangeValue& expected_value
) {
    for (const auto& change : diff.changes) {
        if (change.entity_persistent_id != conflict.entity_persistent_id) continue;
        if (!conflict.component.empty() && change.component != conflict.component) continue;
        if (!conflict.property.empty() && change.property != conflict.property) continue;
        if (change.after_value == expected_value) return &change;
    }
    return nullptr;
}

bool apply_change(Scene& scene, const Scene& source, const SceneChange& change) {
    SceneDiff one;
    one.changes.push_back(change);
    const std::size_t index = 0;
    return SceneDiffer::apply(scene, source, one, std::span<const std::size_t>(&index, 1)).success();
}

} // namespace

SceneMergeResolutionResult SceneMergeResolver::resolve(
    const Scene& base_scene,
    const Scene& ours_scene,
    const Scene& theirs_scene,
    const SceneMergeResult& merge_result,
    const std::vector<SceneMergeResolution>& resolutions
) {
    SceneMergeResolutionResult result;
    result.resolved_scene = merge_result.merged_scene;
    result.resolutions = resolutions;

    const SceneDiff ours_diff = SceneDiffer::compare(base_scene, ours_scene);
    const SceneDiff theirs_diff = SceneDiffer::compare(base_scene, theirs_scene);

    std::map<std::size_t, SceneMergeResolution> by_conflict;
    for (const auto& resolution : resolutions) {
        if (resolution.conflict_index >= merge_result.conflicts.size()) {
            result.errors.push_back("Merge resolution references an invalid conflict index.");
            continue;
        }
        by_conflict[resolution.conflict_index] = resolution;
    }

    for (std::size_t i = 0; i < merge_result.conflicts.size(); ++i) {
        const auto resolution_it = by_conflict.find(i);
        if (resolution_it == by_conflict.end() || resolution_it->second.choice == SceneMergeChoice::Unresolved) {
            ++result.unresolved_count;
            continue;
        }

        const auto& conflict = merge_result.conflicts[i];
        const auto& resolution = resolution_it->second;
        const SceneChange* selected = nullptr;
        const Scene* source = nullptr;

        if (resolution.choice == SceneMergeChoice::Ours) {
            selected = find_matching_change(ours_diff, conflict, conflict.ours_value);
            source = &ours_scene;
        } else if (resolution.choice == SceneMergeChoice::Theirs) {
            selected = find_matching_change(theirs_diff, conflict, conflict.theirs_value);
            source = &theirs_scene;
        } else if (resolution.choice == SceneMergeChoice::Manual) {
            // Manual scalar/structured property editing will be layered on this
            // API next. Keeping it explicit prevents Studio from silently
            // pretending a manual value was applied when it was not.
            if (!resolution.manual_value.has_value()) {
                result.errors.push_back("Manual merge resolution requires a value.");
            } else {
                result.errors.push_back("Manual merge values are not supported for this conflict yet.");
            }
            ++result.unresolved_count;
            continue;
        }

        if (selected == nullptr || source == nullptr) {
            std::ostringstream error;
            error << "Could not map resolution for conflict " << i << " back to its semantic scene change.";
            result.errors.push_back(error.str());
            ++result.unresolved_count;
            continue;
        }

        if (!apply_change(result.resolved_scene, *source, *selected)) {
            std::ostringstream error;
            error << "Failed to apply resolution for conflict " << i << ".";
            result.errors.push_back(error.str());
            ++result.unresolved_count;
            continue;
        }

        ++result.resolved_count;
    }

    return result;
}

} // namespace seed
