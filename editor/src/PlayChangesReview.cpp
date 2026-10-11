#include "PlayChangesReview.h"
#include "StudioUI.h"

#include <imgui.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace seed::studio {
namespace {

const char* safety_name(SceneChangeSafety safety) {
    switch (safety) {
    case SceneChangeSafety::Safe: return "SAFE";
    case SceneChangeSafety::Review: return "REVIEW";
    case SceneChangeSafety::RuntimeState: return "RUNTIME";
    }
    return "UNKNOWN";
}

const char* kind_name(SceneChangeKind kind) {
    switch (kind) {
    case SceneChangeKind::EntityAdded: return "Entity added";
    case SceneChangeKind::EntityRemoved: return "Entity removed";
    case SceneChangeKind::EntityRenamed: return "Entity renamed";
    case SceneChangeKind::ParentChanged: return "Parent changed";
    case SceneChangeKind::ComponentAdded: return "Component added";
    case SceneChangeKind::ComponentRemoved: return "Component removed";
    case SceneChangeKind::PropertyChanged: return "Property changed";
    }
    return "Changed";
}

std::string value_text(const SceneChangeValue& value) {
    return std::visit([](const auto& item) -> std::string {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            return "—";
        } else if constexpr (std::is_same_v<T, bool>) {
            return item ? "true" : "false";
        } else if constexpr (std::is_same_v<T, int>) {
            return std::to_string(item);
        } else if constexpr (std::is_same_v<T, float>) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(3) << item;
            return stream.str();
        } else if constexpr (std::is_same_v<T, std::string>) {
            return item.empty() ? "(empty)" : item;
        } else if constexpr (std::is_same_v<T, Vec3>) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2)
                   << '(' << item.x << ", " << item.y << ", " << item.z << ')';
            return stream.str();
        } else if constexpr (std::is_same_v<T, Vec4>) {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(2)
                   << '(' << item.x << ", " << item.y << ", " << item.z << ", " << item.w << ')';
            return stream.str();
        }
        return "?";
    }, value);
}

std::string change_path(const SceneChange& change) {
    if (change.kind == SceneChangeKind::EntityAdded || change.kind == SceneChangeKind::EntityRemoved) {
        return kind_name(change.kind);
    }
    if (change.kind == SceneChangeKind::EntityRenamed) return "Entity / name";
    if (change.kind == SceneChangeKind::ParentChanged) return "Parent / parent";
    if (change.property.empty()) return change.component.empty() ? kind_name(change.kind) : change.component;
    return change.component + " / " + change.property;
}

void safety_badge(SceneChangeSafety safety) {
    const ImVec4 safe{0.30f, 0.92f, 0.48f, 1.0f};
    const ImVec4 review{1.00f, 0.74f, 0.28f, 1.0f};
    const ImVec4 runtime{0.62f, 0.68f, 0.72f, 1.0f};
    const ImVec4 color = safety == SceneChangeSafety::Safe ? safe
        : safety == SceneChangeSafety::Review ? review : runtime;
    ImGui::TextColored(color, "%s", safety_name(safety));
}

} // namespace

void PlayChangesReview::open(const SceneDiff& diff) {
    m_open = true;
    m_request_open_popup = true;
    m_selected.assign(diff.changes.size(), false);
    for (std::size_t i = 0; i < diff.changes.size(); ++i) {
        m_selected[i] = diff.changes[i].safety == SceneChangeSafety::Safe;
    }
}

void PlayChangesReview::reset() {
    m_open = false;
    m_request_open_popup = false;
    m_selected.clear();
}

void PlayChangesReview::ensure_selection_matches(const SceneDiff& diff) {
    if (m_selected.size() == diff.changes.size()) return;
    m_selected.assign(diff.changes.size(), false);
    for (std::size_t i = 0; i < diff.changes.size(); ++i) {
        m_selected[i] = diff.changes[i].safety == SceneChangeSafety::Safe;
    }
}

PlayChangesReviewResult PlayChangesReview::draw(const SceneDiff& diff) {
    PlayChangesReviewResult result;
    if (!m_open) return result;

    ensure_selection_matches(diff);
    if (m_request_open_popup) {
        ImGui::OpenPopup("Review Play Changes");
        m_request_open_popup = false;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSize({std::min(1120.0f, io.DisplaySize.x * 0.90f), std::min(700.0f, io.DisplaySize.y * 0.84f)}, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Appearing, {0.5f, 0.5f});

    bool keep_open = true;
    if (ImGui::BeginPopupModal("Review Play Changes", &keep_open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::TextUnformatted("Seed found changes made while the game was running.");
        ImGui::TextDisabled("Safe changes are preselected. Review changes are opt-in. Runtime state is never applied automatically.");
        ImGui::Spacing();

        ImGui::Text("%zu Safe", diff.safe_count());
        ImGui::SameLine();
        ImGui::TextDisabled("•");
        ImGui::SameLine();
        ImGui::Text("%zu Review", diff.review_count());
        ImGui::SameLine();
        ImGui::TextDisabled("•");
        ImGui::SameLine();
        ImGui::Text("%zu Runtime State", diff.runtime_state_count());
        ImGui::Separator();

        const ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("PlayChangesTable", 7, flags, {0.0f, -72.0f})) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Apply", ImGuiTableColumnFlags_WidthFixed, 58.0f);
            ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch, 1.25f);
            ImGui::TableSetupColumn("Change", ImGuiTableColumnFlags_WidthStretch, 1.55f);
            ImGui::TableSetupColumn("Before", ImGuiTableColumnFlags_WidthStretch, 1.20f);
            ImGui::TableSetupColumn("After", ImGuiTableColumnFlags_WidthStretch, 1.20f);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("Meaning", ImGuiTableColumnFlags_WidthStretch, 1.05f);
            ImGui::TableHeadersRow();

            for (std::size_t i = 0; i < diff.changes.size(); ++i) {
                const SceneChange& change = diff.changes[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                if (change.safety == SceneChangeSafety::RuntimeState) {
                    bool disabled = false;
                    ImGui::BeginDisabled();
                    ImGui::Checkbox("##apply", &disabled);
                    ImGui::EndDisabled();
                    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                        ImGui::SetTooltip("Runtime State belongs to the running game and is intentionally not applied from this review.");
                    }
                } else {
                    bool selected = m_selected[i];
                    if (ImGui::Checkbox("##apply", &selected)) m_selected[i] = selected;
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(change.entity_name.empty() ? "Entity" : change.entity_name.c_str());
                if (!change.entity_persistent_id.empty() && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Persistent ID: %s", change.entity_persistent_id.c_str());
                }

                ImGui::TableSetColumnIndex(2);
                const std::string path = change_path(change);
                ImGui::TextUnformatted(path.c_str());

                ImGui::TableSetColumnIndex(3);
                const std::string before = value_text(change.before_value);
                ImGui::TextWrapped("%s", before.c_str());

                ImGui::TableSetColumnIndex(4);
                const std::string after = value_text(change.after_value);
                ImGui::TextWrapped("%s", after.c_str());

                ImGui::TableSetColumnIndex(5);
                safety_badge(change.safety);

                ImGui::TableSetColumnIndex(6);
                if (change.safety == SceneChangeSafety::Safe) ImGui::TextDisabled("Tuning / authored setting");
                else if (change.safety == SceneChangeSafety::Review) ImGui::TextDisabled("Could be gameplay or authoring");
                else ImGui::TextDisabled("Running-game state");

                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        std::size_t selected_count = 0;
        for (std::size_t i = 0; i < m_selected.size(); ++i) {
            if (m_selected[i] && diff.changes[i].safety != SceneChangeSafety::RuntimeState) ++selected_count;
        }

        ImGui::TextDisabled("%zu change%s selected", selected_count, selected_count == 1 ? "" : "s");
        ImGui::SameLine();
        if (ImGui::Button("Select Safe")) {
            for (std::size_t i = 0; i < diff.changes.size(); ++i) {
                m_selected[i] = diff.changes[i].safety == SceneChangeSafety::Safe;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Selection")) {
            std::fill(m_selected.begin(), m_selected.end(), false);
        }

        const float right_group = 360.0f;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - right_group));
        if (ImGui::Button("Discard Runtime Changes", {170.0f, 0.0f})) {
            result.decision = PlayChangesDecision::Discard;
            ImGui::CloseCurrentPopup();
            reset();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(selected_count == 0);
        if (ImGui::Button("Apply Selected", {150.0f, 0.0f})) {
            result.decision = PlayChangesDecision::ApplySelected;
            result.selected_change_indices.reserve(selected_count);
            for (std::size_t i = 0; i < m_selected.size(); ++i) {
                if (m_selected[i] && diff.changes[i].safety != SceneChangeSafety::RuntimeState) {
                    result.selected_change_indices.push_back(i);
                }
            }

            // The semantic apply happens in Studio's main loop immediately after
            // this modal returns. Begin one external history transaction here so
            // all selected Play changes become a single native Ctrl+Z operation.
            if (StudioUI* studio = StudioUI::active_instance(); studio != nullptr) {
                studio->begin_external_history_transaction();
            }

            ImGui::CloseCurrentPopup();
            reset();
        }
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    // Closing via the window X is treated as "keep reviewing" rather than an
    // implicit discard. Re-open next frame so important Play changes cannot be
    // lost by accidentally dismissing the modal.
    if (!keep_open && result.decision == PlayChangesDecision::None) {
        m_request_open_popup = true;
        m_open = true;
    }

    return result;
}

} // namespace seed::studio
