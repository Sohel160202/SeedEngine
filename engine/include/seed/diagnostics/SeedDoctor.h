#pragma once

#include <string>
#include <vector>

namespace seed {

class Scene;

enum class DoctorSeverity {
    Info,
    Warning,
    Error,
};

enum class DoctorFixKind {
    None,
    AddInteractable,
    AddInventory,
    AddCharacterBody,
};

struct DoctorFinding {
    std::string code;
    DoctorSeverity severity{DoctorSeverity::Warning};
    std::string entity_persistent_id;
    std::string entity_name;
    std::string message;
    std::string technical_detail;
    DoctorFixKind safe_fix{DoctorFixKind::None};
    std::string fix_label;
};

struct DoctorFixResult {
    bool applied{false};
    std::string message;
};

// Seed Doctor translates structural gameplay mistakes into creator-facing game
// language. Rules live in the engine rather than Studio so validation can also
// run in CI, packaging, automated tests and future collaborative workflows.
class SeedDoctor {
public:
    static std::vector<DoctorFinding> inspect(const Scene& scene);

    // Only explicitly safe, local dependency fixes are applied here. Seed Doctor
    // never guesses project intent or silently changes behavior.
    static DoctorFixResult apply_safe_fix(Scene& scene, const DoctorFinding& finding);
};

} // namespace seed
