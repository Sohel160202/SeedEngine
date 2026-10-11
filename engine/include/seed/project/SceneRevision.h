#pragma once

#include "seed/scene/Scene.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace seed {

struct SceneRevision {
    std::string id;
    std::string label;
    std::filesystem::path scene_file;
    std::size_t entity_count{0};
};

struct SceneRevisionWriteResult {
    bool saved{false};
    SceneRevision revision{};
    std::string error;
};

// Native, project-local authored-scene history. Revisions are immutable
// snapshots stored under <project>/.seed/revisions and are independent from
// runtime SaveGame slots.
class SceneRevisionStore {
public:
    explicit SceneRevisionStore(std::filesystem::path project_root = {});

    void set_project_root(std::filesystem::path project_root);
    const std::filesystem::path& project_root() const noexcept { return m_project_root; }
    std::filesystem::path revision_root() const;

    static std::string sanitize_label(std::string_view label);

    std::vector<SceneRevision> list() const;

    SceneRevisionWriteResult create_revision(
        const Scene& scene,
        std::string label
    ) const;

    bool load_revision(
        Scene& scene,
        std::string_view revision_id,
        std::string* error = nullptr
    ) const;

private:
    std::filesystem::path metadata_file(std::string_view revision_id) const;
    std::filesystem::path scene_file(std::string_view revision_id) const;
    static bool valid_revision_id(std::string_view revision_id) noexcept;

    std::filesystem::path m_project_root;
};

} // namespace seed
