#include "SemanticMergeReview.h"

#include <imgui.h>

#include <algorithm>
#include <sstream>
#include <string>
#include <type_traits>

namespace seed::studio {
namespace {

const char* conflict_kind_name(SceneMergeConflictKind kind) {
    switch (kind) {
    case SceneMergeConflictKind::SameTargetChangedDifferently: return "Changed differently";
    case SceneMergeConflictKind::EntityRemovedVsModified: return "Removed vs modified";
    case SceneMergeConflictKind::EntityAddedDifferently: return "Added differently";
    }
    return "Conflict";
}

std::string value_text(const SceneChangeValue& value) {
    return std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::monostate>) return "—";
        else if constexpr (std::is_same_v<T, bool>) return item ? "true" : "false";
        else if constexpr (std::is_same_v<T, std::string>) return item;
        else if constexpr (std::is_same_v<T, float>) {
            std::ostringstream stream;
            stream << item;
            return stream.str();
        } else if constexpr (std::is_same_v<T, Vec3>) {
            std::ostringstream stream;
            stream << "(" << item.x << ", " << item.y << ", " << item.z << ")";
            return stream.str();
        } else if constexpr (std::is_same_v<T, Vec4>) {
            std::ostringstream stream;
            stream << "(" << item.x << ", " << item.y << ", " << item.z << ", " << item.w << ")";
            return stream.str();
        } else {
            return "?";
        }
    }, value);
}

const char* choice_name(SceneMergeResolutionChoice choice) {
    switch (choice) {
    case SceneMergeResolutionChoice::Ours: return "Ours";
    case SceneMergeResolutionChoice::Theirs: return "Theirs";
    default: return "Unresolved";
    }
}

} // namespace

void SemanticMergeReview::open(
    const Scene& base_scene,
    const Scene& ours_scene,
    const Scene& theirs_scene
) {
    m_base_scene = base_scene;
    m_ours_scene = ours_scene;
    m_theirs_scene = theirs_scene;
    m_merge_result = SceneMerger::merge(m_base_scene, m_ours_scene, m_theirs_scene);
    m_plan.choices.assign(
        m_merge_result.conflicts.size(),
        SceneMergeResolutionChoice::Unresolved
    );
    m_open = true;
    m_request_open_popup = true;
}

void SemanticMergeReview::reset() {
    m_open = false;
    m_request_open_popup = false;
    m_base_scene.clear();
    m_ours_scene.clear();
    m_theirs_scene.clear();
    m_merge_result = {};
    m_plan = {};
}

SemanticMergeReviewResult SemanticMergeReview::draw() {
    SemanticMergeReviewResult result;
    if (!m_open) return result;

    if (m_request_open_popup) {
        ImGui::OpenPopup("Semantic Merge Review");
        m_request_open_popup = false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize(
        {std::min(1320.0f, io.DisplaySize.x * 0.94f), std::min(760.0f, io.DisplaySize.y * 0.88f)},
        ImGuiCond_Appearing
    );
    ImGui::SetNextWindowPos(
        {io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
        ImGuiCond_Appearing,
        {0.5f, 0.5f}
    );

    bool keep_open = true;
    if (ImGui::BeginPopupModal("Semantic Merge Review", &keep_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("Review semantic scene conflicts before applying the merge.");
        ImGui::Text(
            "%zu automatic changes  •  %zu conflict%s",
            m_merge_result.auto_merged_changes.size(),
            m_merge_result.conflicts.size(),
            m_merge_result.conflicts.size() == 1 ? "" : "s"
        );
        ImGui::Separator();

        if (m_merge_result.conflicts.empty()) {
            ImGui::TextColored({0.30f, 0.92f, 0.48f, 1.0f}, "No conflicts. The merge is ready to apply.");
        } else if (ImGui::BeginTable(
                       "SemanticMergeConflicts",
                       7,
                       ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
                           ImGuiTableFlags_SizingStretchProp,
                       {0.0f, -72.0f}
                   )) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Target", ImGuiTableColumnFlags_WidthStretch, 1.2f);
            ImGui::TableSetupColumn("Base", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Ours", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Theirs", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 130.0f);
            ImGui::TableSetupColumn("Resolution", ImGuiTableColumnFlags_WidthFixed, 220.0f);
            ImGui::TableHeadersRow();

            for (std::size_t index = 0; index < m_merge_result.conflicts.size(); ++index) {
                const auto& conflict = m_merge_result.conflicts[index];
                ImGui::PushID(static_cast<int>(index));
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(conflict.entity_name.empty() ? "Entity" : conflict.entity_name.c_str());
                ImGui::TableSetColumnIndex(1);
                const std::string target = conflict.component.empty()
                    ? conflict.property
                    : conflict.component + " / " + conflict.property;
                ImGui::TextUnformatted(target.empty() ? "Entity" : target.c_str());
                ImGui::TableSetColumnIndex(2);
                const std::string base = value_text(conflict.base_value);
                ImGui::TextWrapped("%s", base.c_str());
                ImGui::TableSetColumnIndex(3);
                const std::string ours = value_text(conflict.ours_value);
                ImGui::TextWrapped("%s", ours.c_str());
                ImGui::TableSetColumnIndex(4);
                const std::string theirs = value_text(conflict.theirs_value);
                ImGui::TextWrapped("%s", theirs.c_str());
                ImGui::TableSetColumnIndex(5);
                ImGui::TextDisabled("%s", conflict_kind_name(conflict.kind));
                ImGui::TableSetColumnIndex(6);

                const auto choice = m_plan.choices[index];
                if (ImGui::Button("Ours", {64.0f, 0.0f})) {
                    m_plan.choices[index] = SceneMergeResolutionChoice::Ours;
                }
                ImGui::SameLine();
                if (ImGui::Button("Theirs", {64.0f, 0.0f})) {
                    m_plan.choices[index] = SceneMergeResolutionChoice::Theirs;
                }
                ImGui::SameLine();
                ImGui::TextDisabled("%s", choice_name(choice));
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        std::size_t resolved_count = 0;
        for (const auto choice : m_plan.choices) {
            if (choice != SceneMergeResolutionChoice::Unresolved) ++resolved_count;
        }

        ImGui::Text(
            "%zu/%zu conflicts resolved",
            resolved_count,
            m_plan.choices.size()
        );
        ImGui::SameLine();
        if (ImGui::Button("Reset Choices")) {
            std::fill(
                m_plan.choices.begin(),
                m_plan.choices.end(),
                SceneMergeResolutionChoice::Unresolved
            );
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f);
        if (ImGui::Button("Cancel", {100.0f, 0.0f})) {
            result.decision = SemanticMergeReviewDecision::Cancel;
            ImGui::CloseCurrentPopup();
            reset();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(resolved_count != m_plan.choices.size());
        if (ImGui::Button("Apply Merge", {120.0f, 0.0f})) {
            const auto resolved = SceneMergeResolver::resolve(
                m_base_scene,
                m_ours_scene,
                m_theirs_scene,
                m_plan
            );
            if (resolved.complete()) {
                result.decision = SemanticMergeReviewDecision::Apply;
                result.merged_scene = resolved.merged_scene;
                ImGui::CloseCurrentPopup();
                reset();
            }
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    if (!keep_open && result.decision == SemanticMergeReviewDecision::None) {
        // Closing with the window X is intentionally treated as Cancel rather
        // than silently applying a partial merge.
        result.decision = SemanticMergeReviewDecision::Cancel;
        reset();
    }

    return result;
}

} // namespace seed::studio
