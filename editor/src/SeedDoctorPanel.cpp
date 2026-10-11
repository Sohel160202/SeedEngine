#include "SeedDoctorPanel.h"
#include "StudioUI.h"

#include "seed/diagnostics/SeedDoctor.h"
#include "seed/scene/Scene.h"

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>

namespace seed::studio {
namespace {

const char* severity_name(DoctorSeverity severity) {
    switch (severity) {
    case DoctorSeverity::Error: return "ERROR";
    case DoctorSeverity::Warning: return "WARNING";
    case DoctorSeverity::Info: return "INFO";
    }
    return "INFO";
}

ImVec4 severity_color(DoctorSeverity severity) {
    switch (severity) {
    case DoctorSeverity::Error: return {1.00f, 0.34f, 0.30f, 1.0f};
    case DoctorSeverity::Warning: return {1.00f, 0.73f, 0.26f, 1.0f};
    case DoctorSeverity::Info: return {0.35f, 0.72f, 1.00f, 1.0f};
    }
    return {0.75f, 0.75f, 0.75f, 1.0f};
}

std::size_t count_severity(const std::vector<DoctorFinding>& findings, DoctorSeverity severity) {
    return static_cast<std::size_t>(std::count_if(
        findings.begin(), findings.end(),
        [&](const DoctorFinding& finding) { return finding.severity == severity; }
    ));
}

} // namespace

SeedDoctorPanelResult SeedDoctorPanel::draw(Scene& scene, StudioUI& studio, bool enabled) {
    SeedDoctorPanelResult result;
    if (!enabled) return result;

    const std::vector<DoctorFinding> findings = SeedDoctor::inspect(scene);
    const std::size_t errors = count_severity(findings, DoctorSeverity::Error);
    const std::size_t warnings = count_severity(findings, DoctorSeverity::Warning);
    const std::size_t infos = count_severity(findings, DoctorSeverity::Info);

    // Keep a tiny always-available health chip over the viewport. It gives Seed
    // Doctor a visible identity without consuming a permanent editor dock.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({260.0f, ImGui::GetFrameHeight() + 8.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.94f);
    const ImGuiWindowFlags chip_flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("SeedDoctorChip", nullptr, chip_flags)) {
        if (findings.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.08f, 0.28f, 0.15f, 1.0f});
            if (ImGui::Button("Seed Doctor  Healthy")) m_open = !m_open;
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, errors > 0
                ? ImVec4{0.40f, 0.10f, 0.09f, 1.0f}
                : ImVec4{0.32f, 0.23f, 0.07f, 1.0f});
            const std::string label = "Seed Doctor  " + std::to_string(findings.size()) +
                (findings.size() == 1 ? " issue" : " issues");
            if (ImGui::Button(label.c_str())) m_open = !m_open;
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();

    if (!m_open) return result;

    ImGui::SetNextWindowSize({720.0f, 560.0f}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Appearing, {0.5f, 0.5f});
    if (!ImGui::Begin("Seed Doctor", &m_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return result;
    }

    ImGui::TextUnformatted("Why isn't this working?");
    ImGui::TextDisabled("Seed Doctor checks gameplay structure and only offers fixes it considers safe.");
    ImGui::Spacing();

    if (findings.empty()) {
        ImGui::TextColored({0.30f, 0.92f, 0.48f, 1.0f}, "No known issues found in this scene.");
        ImGui::TextDisabled("Doctor rules will grow as Seed gains more gameplay systems.");
        if (!m_last_message.empty()) {
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_last_message.c_str());
        }
        ImGui::End();
        return result;
    }

    ImGui::Text("%zu Error%s", errors, errors == 1 ? "" : "s");
    ImGui::SameLine(); ImGui::TextDisabled("•"); ImGui::SameLine();
    ImGui::Text("%zu Warning%s", warnings, warnings == 1 ? "" : "s");
    ImGui::SameLine(); ImGui::TextDisabled("•"); ImGui::SameLine();
    ImGui::Text("%zu Info", infos);
    ImGui::Separator();

    ImGui::BeginChild("SeedDoctorFindings", {0.0f, -48.0f}, false);
    for (std::size_t index = 0; index < findings.size(); ++index) {
        const DoctorFinding& finding = findings[index];
        ImGui::PushID(static_cast<int>(index));

        const ImVec4 color = severity_color(finding.severity);
        ImGui::TextColored(color, "%s", severity_name(finding.severity));
        ImGui::SameLine();
        ImGui::TextUnformatted(finding.entity_name.empty() ? "Entity" : finding.entity_name.c_str());
        ImGui::TextWrapped("%s", finding.message.c_str());
        ImGui::TextDisabled("%s", finding.code.c_str());

        if (ImGui::Button("Select Entity")) {
            const EntityId entity = scene.find_entity_by_persistent_id(finding.entity_persistent_id);
            if (entity != InvalidEntity && scene.is_alive(entity)) studio.select_entity(entity);
        }

        if (finding.safe_fix != DoctorFixKind::None) {
            ImGui::SameLine();
            const char* fix_label = finding.fix_label.empty() ? "Fix" : finding.fix_label.c_str();
            if (ImGui::Button(fix_label)) {
                // Doctor safe fixes are authored scene edits. Route them through
                // the same external history bridge as Apply Play Changes so each
                // explicit fix is one normal Studio Undo operation.
                studio.begin_external_history_transaction();
                const DoctorFixResult fix = SeedDoctor::apply_safe_fix(scene, finding);
                m_last_message = fix.message;
                if (fix.applied) {
                    result.scene_changed = true;
                    result.message = "Seed Doctor: " + fix.message;
                }
            }
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Technical details")) ImGui::OpenPopup("DoctorTechnicalDetail");
        if (ImGui::BeginPopup("DoctorTechnicalDetail")) {
            ImGui::TextWrapped("%s", finding.technical_detail.c_str());
            if (!finding.entity_persistent_id.empty()) {
                ImGui::Separator();
                ImGui::TextDisabled("Entity ID: %s", finding.entity_persistent_id.c_str());
            }
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PopID();
    }
    ImGui::EndChild();

    if (!m_last_message.empty()) ImGui::TextWrapped("%s", m_last_message.c_str());
    ImGui::End();
    return result;
}

} // namespace seed::studio
