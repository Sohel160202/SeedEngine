#include "seed/project/SceneRevision.h"

#include "seed/project/SceneSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace seed {
namespace {

constexpr std::string_view RevisionPrefix{"revision-"};
constexpr std::string_view MetadataExtension{".json"};
constexpr std::string_view SceneExtension{".seedscene"};
constexpr std::size_t MaxLabelLength = 96;

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

std::string next_revision_id(const std::filesystem::path& root) {
    std::size_t highest = 0;
    std::error_code filesystem_error;
    if (std::filesystem::is_directory(root, filesystem_error) && !filesystem_error) {
        for (const auto& entry : std::filesystem::directory_iterator(root, filesystem_error)) {
            if (filesystem_error || !entry.is_regular_file(filesystem_error)) continue;
            const std::string name = entry.path().stem().string();
            if (!name.starts_with(RevisionPrefix)) continue;
            const std::string number = name.substr(RevisionPrefix.size());
            if (number.empty() || !std::all_of(number.begin(), number.end(), [](char c) {
                    return std::isdigit(static_cast<unsigned char>(c)) != 0;
                })) {
                continue;
            }
            try {
                highest = std::max(highest, static_cast<std::size_t>(std::stoull(number)));
            } catch (...) {
                continue;
            }
        }
    }

    std::ostringstream id;
    id << RevisionPrefix << std::setw(6) << std::setfill('0') << (highest + 1);
    return id.str();
}

} // namespace

SceneRevisionStore::SceneRevisionStore(std::filesystem::path project_root) {
    set_project_root(std::move(project_root));
}

void SceneRevisionStore::set_project_root(std::filesystem::path project_root) {
    m_project_root = std::move(project_root);
}

std::filesystem::path SceneRevisionStore::revision_root() const {
    return m_project_root / ".seed" / "revisions";
}

std::string SceneRevisionStore::sanitize_label(std::string_view label) {
    std::string sanitized;
    sanitized.reserve(std::min(label.size(), MaxLabelLength));
    bool previous_separator = false;

    for (const char raw : label) {
        if (sanitized.size() >= MaxLabelLength) break;
        const unsigned char value = static_cast<unsigned char>(raw);
        if (std::isalnum(value) != 0 || raw == '-' || raw == '_') {
            sanitized.push_back(raw);
            previous_separator = false;
        } else if (!sanitized.empty() && !previous_separator) {
            sanitized.push_back('_');
            previous_separator = true;
        }
    }

    while (!sanitized.empty() && sanitized.back() == '_') sanitized.pop_back();
    return sanitized.empty() ? "Revision" : sanitized;
}

bool SceneRevisionStore::valid_revision_id(std::string_view revision_id) noexcept {
    if (!revision_id.starts_with(RevisionPrefix) || revision_id.size() <= RevisionPrefix.size()) return false;
    for (const char value : revision_id) {
        if (std::isalnum(static_cast<unsigned char>(value)) == 0 && value != '-') return false;
    }
    return true;
}

std::filesystem::path SceneRevisionStore::metadata_file(std::string_view revision_id) const {
    return revision_root() / (std::string{revision_id} + std::string{MetadataExtension});
}

std::filesystem::path SceneRevisionStore::scene_file(std::string_view revision_id) const {
    return revision_root() / (std::string{revision_id} + std::string{SceneExtension});
}

std::vector<SceneRevision> SceneRevisionStore::list() const {
    std::vector<SceneRevision> revisions;
    std::error_code filesystem_error;
    if (!std::filesystem::is_directory(revision_root(), filesystem_error) || filesystem_error) {
        return revisions;
    }

    for (const auto& entry : std::filesystem::directory_iterator(revision_root(), filesystem_error)) {
        if (filesystem_error || !entry.is_regular_file(filesystem_error) ||
            entry.path().extension() != MetadataExtension) {
            continue;
        }

        try {
            std::ifstream input(entry.path(), std::ios::binary);
            nlohmann::json document;
            input >> document;
            if (document.value("seed_format", std::string{}) != "seedrevision") continue;

            SceneRevision revision;
            revision.id = document.value("id", entry.path().stem().string());
            if (!valid_revision_id(revision.id)) continue;
            revision.label = document.value("label", revision.id);
            revision.scene_file = revision_root() /
                document.value("scene_file", revision.id + std::string{SceneExtension});
            revision.entity_count = document.value("entity_count", std::size_t{0});
            revisions.push_back(std::move(revision));
        } catch (...) {
            // A malformed history entry must not prevent the rest of the
            // project history from being listed.
        }
    }

    std::sort(revisions.begin(), revisions.end(), [](const SceneRevision& left, const SceneRevision& right) {
        return left.id > right.id;
    });
    return revisions;
}

SceneRevisionWriteResult SceneRevisionStore::create_revision(
    const Scene& scene,
    std::string label
) const {
    SceneRevisionWriteResult result;
    if (m_project_root.empty()) {
        result.error = "Cannot create a scene revision without a project root.";
        return result;
    }

    try {
        std::filesystem::create_directories(revision_root());
        const std::string id = next_revision_id(revision_root());
        const std::string safe_label = sanitize_label(label);

        result.revision = {
            .id = id,
            .label = safe_label,
            .scene_file = scene_file(id),
            .entity_count = scene.entity_count(),
        };

        if (!SceneSerializer::save(scene, result.revision.scene_file, &result.error)) {
            return result;
        }

        const nlohmann::json document = {
            {"seed_format", "seedrevision"},
            {"format_version", 1},
            {"id", result.revision.id},
            {"label", result.revision.label},
            {"scene_file", result.revision.scene_file.filename().string()},
            {"entity_count", result.revision.entity_count},
        };
        std::ofstream output(metadata_file(id), std::ios::binary | std::ios::trunc);
        if (!output) {
            result.error = "Could not open revision metadata for writing: " + metadata_file(id).string();
            return result;
        }
        output << document.dump(2) << '\n';
        if (!output) {
            result.error = "Could not write revision metadata: " + metadata_file(id).string();
            return result;
        }

        result.saved = true;
        return result;
    } catch (const std::exception& exception) {
        result.error = exception.what();
        return result;
    }
}

bool SceneRevisionStore::load_revision(
    Scene& scene,
    std::string_view revision_id,
    std::string* error
) const {
    if (!valid_revision_id(revision_id)) {
        set_error(error, "Invalid Seed scene revision id.");
        return false;
    }

    const auto metadata = metadata_file(revision_id);
    if (!std::filesystem::is_regular_file(metadata)) {
        set_error(error, "Seed scene revision does not exist: " + std::string{revision_id});
        return false;
    }

    try {
        std::ifstream input(metadata, std::ios::binary);
        nlohmann::json document;
        input >> document;
        const auto path = revision_root() /
            document.value("scene_file", std::string{revision_id} + std::string{SceneExtension});
        return SceneSerializer::load(scene, path, error);
    } catch (const std::exception& exception) {
        set_error(error, exception.what());
        return false;
    }
}

} // namespace seed
