#pragma once

#include "seed/scene/Scene.h"
#include "seed/scene/SceneDiff.h"

#include <cstddef>
#include <string>
#include <vector>

namespace seed {

enum class SceneMergeConflictKind {
    SameTargetChangedDifferently,
    EntityRemovedVsModified,
    EntityAddedDifferently,
};

struct SceneMergeConflict {
    SceneMergeConflictKind kind{SceneMergeConflictKind::SameTargetChangedDifferently};
    std::string entity_persistent_id;
    std::string entity_name;
    std::string component;
    std::string property;
    SceneChangeValue base_value{};
    SceneChangeValue ours_value{};
    SceneChangeValue theirs_value{};
    std::string description;
};

struct SceneMergeResult {
    Scene merged_scene;
    std::vector<SceneChange> auto_merged_changes;
    std::vector<SceneMergeConflict> conflicts;
    std::size_t ours_change_count{0};
    std::size_t theirs_change_count{0};

    bool clean() const noexcept { return conflicts.empty(); }
    std::size_t conflict_count() const noexcept { return conflicts.size(); }
};

// Three-way semantic scene merge. The base scene is the common ancestor;
// ours/theirs are independently edited descendants. Independent semantic
// changes are combined automatically. Competing edits to the same semantic
// target are reported as conflicts instead of becoming text/JSON conflicts.
class SceneMerger {
public:
    static SceneMergeResult merge(
        const Scene& base_scene,
        const Scene& ours_scene,
        const Scene& theirs_scene
    );
};

} // namespace seed
