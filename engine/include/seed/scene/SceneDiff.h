#pragma once

#include "seed/math/Math.h"

#include <cstddef>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace seed {

class Scene;

enum class SceneChangeKind {
    EntityAdded,
    EntityRemoved,
    EntityRenamed,
    ParentChanged,
    ComponentAdded,
    ComponentRemoved,
    PropertyChanged,
};

// Safe changes are good candidates for one-click "Apply Play Changes".
// Review changes may be useful authoring edits, but gameplay can also mutate
// them legitimately (for example, a player Transform). RuntimeState is state
// that normally belongs to the running game rather than the authored scene.
enum class SceneChangeSafety {
    Safe,
    Review,
    RuntimeState,
};

using SceneChangeValue = std::variant<std::monostate, bool, int, float, std::string, Vec3, Vec4>;

struct SceneChange {
    SceneChangeKind kind{SceneChangeKind::PropertyChanged};
    SceneChangeSafety safety{SceneChangeSafety::Review};
    std::string entity_persistent_id;
    std::string entity_name;
    std::string component;
    std::string property;
    SceneChangeValue before_value{};
    SceneChangeValue after_value{};
};

struct SceneDiff {
    std::vector<SceneChange> changes;

    bool empty() const noexcept { return changes.empty(); }
    std::size_t size() const noexcept { return changes.size(); }
    std::size_t safe_count() const noexcept;
    std::size_t review_count() const noexcept;
    std::size_t runtime_state_count() const noexcept;
};

struct SceneApplyResult {
    std::size_t applied{0};
    std::size_t skipped{0};
    std::size_t failed{0};
    std::vector<std::string> errors;

    bool success() const noexcept { return failed == 0; }
};

// Semantic Scene comparison uses persistent entity IDs and Seed component
// meaning rather than serialized JSON/text lines. It is intentionally reusable:
// Studio Play Mode, scene history, source-control merge tooling and future
// prefab/reusable-object workflows can all build on the same diff model.
class SceneDiffer {
public:
    static SceneDiff compare(const Scene& authored_scene, const Scene& runtime_scene);

    // Apply selected changes from runtime_scene back into authored_scene.
    // PropertyChanged entries apply only the selected property, not the entire
    // component, so unrelated runtime state cannot leak back accidentally.
    static SceneApplyResult apply(
        Scene& authored_scene,
        const Scene& runtime_scene,
        const SceneDiff& diff,
        std::span<const std::size_t> selected_change_indices
    );

    // Convenience path for the beginner-facing Stop dialog. Only changes
    // classified Safe are applied; Review/RuntimeState remain untouched.
    static SceneApplyResult apply_all_safe(
        Scene& authored_scene,
        const Scene& runtime_scene,
        const SceneDiff& diff
    );
};

} // namespace seed
