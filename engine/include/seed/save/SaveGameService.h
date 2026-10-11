#pragma once

#include "seed/save/SaveGame.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace seed {

class Scene;

struct SaveSlotWriteResult {
    bool saved{false};
    std::filesystem::path file;
    std::vector<std::string> warnings;
    std::string error;
};

struct SaveSlotLoadResult {
    bool loaded{false};
    std::filesystem::path file;
    SaveApplyResult apply_result{};
    std::string error;
};

// Runtime-friendly facade over SaveGameSystem. Games operate in named slots
// ("slot_1", "autosave", "checkpoint_forest") rather than constructing file
// paths or dealing with serialization. Slot names are normalized to safe local
// filenames so creator/user input cannot escape the configured save directory.
class SaveGameService {
public:
    explicit SaveGameService(std::filesystem::path save_root = {});

    void set_save_root(std::filesystem::path save_root);
    const std::filesystem::path& save_root() const noexcept { return m_save_root; }

    static std::string sanitize_slot_name(std::string_view slot);

    std::filesystem::path slot_file(std::string_view slot) const;
    bool slot_exists(std::string_view slot) const;
    std::vector<std::string> list_slots() const;

    SaveSlotWriteResult save_slot(
        const Scene& scene,
        std::string slot,
        int game_schema_version = 1
    ) const;

    SaveSlotLoadResult load_slot(Scene& scene, std::string_view slot) const;

    bool delete_slot(std::string_view slot, std::string* error = nullptr) const;

private:
    std::filesystem::path m_save_root;
};

} // namespace seed
