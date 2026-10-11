#pragma once

#include <string>

namespace seed {

class Scene;

namespace studio {

class StudioUI;

struct SeedDoctorPanelResult {
    bool scene_changed{false};
    std::string message;
};

// Creator-facing presentation for the engine-owned SeedDoctor validator.
// The panel owns no validation rules; it only explains findings, navigates to
// affected entities and invokes explicitly safe engine fixes.
class SeedDoctorPanel {
public:
    SeedDoctorPanelResult draw(Scene& scene, StudioUI& studio, bool enabled = true);

private:
    bool m_open{false};
    std::string m_last_message;
};

} // namespace studio
} // namespace seed
