#pragma once

#include "seed/math/Math.h"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace seed {

class Scene;

inline constexpr int SeedSaveFormatVersion = 1;

struct SaveTransformState {
    Vec3 position{};
    Vec3 rotation_degrees{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct SaveHealthState {
    float current{100.0f};
};

struct SaveInventoryState {
    std::unordered_map<std::string, int> items;
};

struct SaveEntityState {
    std::string persistent_id;
    std::optional<SaveTransformState> transform;
    std::optional<SaveHealthState> health;
    std::optional<SaveInventoryState> inventory;
};

struct SaveGameSnapshot {
    int format_version{SeedSaveFormatVersion};
    std::string slot{"slot_1"};
    int game_schema_version{1};
    std::vector<SaveEntityState> entities;
};

struct SaveCaptureResult {
    SaveGameSnapshot snapshot;
    std::vector<std::string> warnings;
};

struct SaveApplyResult {
    std::size_t entities_applied{0};
    std::size_t properties_applied{0};
    std::size_t entities_skipped{0};
    std::vector<std::string> warnings;

    bool success() const noexcept { return warnings.empty(); }
};

// Native Seed gameplay persistence. Authored defaults live in .seedscene;
// runtime progress lives in .seedsave. Stable persistent IDs are the semantic
// bridge between the two, and load never guesses when structure changed.
class SaveGameSystem {
public:
    static SaveCaptureResult capture(
        const Scene& scene,
        std::string slot,
        int game_schema_version = 1
    );

    static SaveApplyResult apply(Scene& scene, const SaveGameSnapshot& snapshot);

    static bool write_file(
        const SaveGameSnapshot& snapshot,
        const std::filesystem::path& file,
        std::string* error = nullptr
    );

    static std::optional<SaveGameSnapshot> read_file(
        const std::filesystem::path& file,
        std::string* error = nullptr
    );
};

} // namespace seed
