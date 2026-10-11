#include "seed/save/SaveGameService.h"

#include "seed/scene/Scene.h"

#include <algorithm>
#include <cctype>
#include <system_error>

namespace seed {
namespace {

constexpr std::string_view SaveExtension{".seedsave"};
constexpr std::size_t MaxSlotNameLength = 64;

void set_error(std::string* error, std::string message) {
    if (error != nullptr) *error = std::move(message);
}

} // namespace

SaveGameService::SaveGameService(std::filesystem::path save_root) {
    set_save_root(std::move(save_root));
}

void SaveGameService::set_save_root(std::filesystem::path save_root) {
    m_save_root = save_root.empty() ? std::filesystem::path{"Saves"} : std::move(save_root);
}

std::string SaveGameService::sanitize_slot_name(std::string_view slot) {
    std::string sanitized;
    sanitized.reserve(std::min(slot.size(), MaxSlotNameLength));

    bool previous_separator = false;
    for (const char raw : slot) {
        if (sanitized.size() >= MaxSlotNameLength) break;

        const unsigned char value = static_cast<unsigned char>(raw);
        if (std::isalnum(value) != 0 || raw == '-' || raw == '_') {
            sanitized.push_back(raw);
            previous_separator = false;
            continue;
        }

        // Whitespace and path/punctuation characters collapse to one underscore.
        // This deliberately prevents '..', '/', '\\' and drive separators from
        // ever becoming path syntax while keeping human slot labels recognizable.
        if (!sanitized.empty() && !previous_separator) {
            sanitized.push_back('_');
            previous_separator = true;
        }
    }

    while (!sanitized.empty() && sanitized.back() == '_') sanitized.pop_back();
    if (sanitized.empty() || sanitized == "." || sanitized == "..") return "slot_1";
    return sanitized;
}

std::filesystem::path SaveGameService::slot_file(std::string_view slot) const {
    return m_save_root / (sanitize_slot_name(slot) + std::string{SaveExtension});
}

bool SaveGameService::slot_exists(std::string_view slot) const {
    std::error_code error;
    return std::filesystem::is_regular_file(slot_file(slot), error) && !error;
}

std::vector<std::string> SaveGameService::list_slots() const {
    std::vector<std::string> slots;
    std::error_code error;
    if (!std::filesystem::is_directory(m_save_root, error) || error) return slots;

    for (std::filesystem::directory_iterator iterator(m_save_root, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        const auto& entry = *iterator;
        if (!entry.is_regular_file(error) || error) continue;
        const std::filesystem::path path = entry.path();
        if (path.extension() != SaveExtension) continue;
        slots.push_back(path.stem().string());
    }

    std::sort(slots.begin(), slots.end());
    slots.erase(std::unique(slots.begin(), slots.end()), slots.end());
    return slots;
}

SaveSlotWriteResult SaveGameService::save_slot(
    const Scene& scene,
    std::string slot,
    int game_schema_version
) const {
    SaveSlotWriteResult result;
    const std::string safe_slot = sanitize_slot_name(slot);
    result.file = slot_file(safe_slot);

    SaveCaptureResult capture = SaveGameSystem::capture(
        scene,
        safe_slot,
        game_schema_version
    );
    result.warnings = std::move(capture.warnings);

    if (!SaveGameSystem::write_file(capture.snapshot, result.file, &result.error)) {
        return result;
    }

    result.saved = true;
    return result;
}

SaveSlotLoadResult SaveGameService::load_slot(Scene& scene, std::string_view slot) const {
    SaveSlotLoadResult result;
    result.file = slot_file(slot);

    auto snapshot = SaveGameSystem::read_file(result.file, &result.error);
    if (!snapshot.has_value()) return result;

    result.apply_result = SaveGameSystem::apply(scene, *snapshot);
    result.loaded = true;
    return result;
}

bool SaveGameService::delete_slot(std::string_view slot, std::string* error) const {
    const std::filesystem::path file = slot_file(slot);
    std::error_code filesystem_error;
    const bool removed = std::filesystem::remove(file, filesystem_error);
    if (filesystem_error) {
        set_error(error, "Could not delete Seed save slot: " + filesystem_error.message());
        return false;
    }
    if (!removed) {
        set_error(error, "Seed save slot does not exist: " + sanitize_slot_name(slot));
        return false;
    }
    return true;
}

} // namespace seed
