#pragma once

#include "seed/scene/SceneMerge.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace seed {

enum class SceneMergeChoice {
    Unresolved,
    Ours,
    Theirs,
    Manual,
};

struct SceneMergeResolution {
    std::size_t conflict_index{0};
    SceneMergeChoice choice{SceneMergeChoice::Unresolved};
    std::optional<SceneChangeValue> manual_value;
};

struct SceneMergeResolutionResult {
    Scene resolved_scene;
    std::vector<SceneMergeResolution> resolutions;
    std::size_t resolved_count{0};
    std::size_t unresolved_count{0};
    std::vector<std::string> errors;

    bool complete() const noexcept { return unresolved_count == 0 && errors.empty(); }
};

// Resolves the structured conflicts produced by SceneMerger. Studio, CLI and
// future Git integration all use this API so conflict semantics stay in the
// engine instead of being reimplemented by each frontend.
class SceneMergeResolver {
public:
    static SceneMergeResolutionResult resolve(
        const Scene& base_scene,
        const Scene& ours_scene,
        const Scene& theirs_scene,
        const SceneMergeResult& merge_result,
        const std::vector<SceneMergeResolution>& resolutions
    );
};

} // namespace seed
