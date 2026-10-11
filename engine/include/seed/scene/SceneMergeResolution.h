#pragma once

#include "seed/scene/SceneMerge.h"

#include <cstddef>
#include <string>
#include <vector>

namespace seed {

enum class SceneMergeResolutionChoice {
    Unresolved,
    Ours,
    Theirs,
};

struct SceneMergeResolutionPlan {
    std::vector<SceneMergeResolutionChoice> choices;

    SceneMergeResolutionChoice choice_for(std::size_t conflict_index) const noexcept {
        if (conflict_index >= choices.size()) return SceneMergeResolutionChoice::Unresolved;
        return choices[conflict_index];
    }
};

struct SceneMergeResolutionResult {
    Scene merged_scene;
    std::vector<SceneMergeConflict> resolved_conflicts;
    std::vector<SceneMergeConflict> unresolved_conflicts;
    std::vector<std::string> errors;

    bool complete() const noexcept {
        return unresolved_conflicts.empty() && errors.empty();
    }
};

// Applies a user's choices to the semantic three-way merge result. The merge
// engine remains deterministic: independent changes are auto-merged first,
// then only the selected side of each conflict is applied.
class SceneMergeResolver {
public:
    static SceneMergeResolutionResult resolve(
        const Scene& base_scene,
        const Scene& ours_scene,
        const Scene& theirs_scene,
        const SceneMergeResolutionPlan& plan
    );
};

} // namespace seed
