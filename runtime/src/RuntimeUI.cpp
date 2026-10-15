#include "RuntimeUI.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>

namespace seed::runtime {

bool RuntimeUI::initialize(void* window_handle) {
    if (m_initialized || window_handle == nullptr) return false;

    auto* window = static_cast<GLFWwindow*>(window_handle);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.WindowBorderSize = 0.0f;
    style.WindowPadding = {16.0f, 14.0f};
    style.ItemSpacing = {8.0f, 7.0f};
    style.Colors[ImGuiCol_WindowBg] = {0.025f, 0.040f, 0.032f, 0.88f};
    style.Colors[ImGuiCol_FrameBg] = {0.08f, 0.14f, 0.10f, 1.0f};
    style.Colors[ImGuiCol_PlotHistogram] = {0.25f, 0.85f, 0.42f, 1.0f};
    style.Colors[ImGuiCol_Button] = {0.08f, 0.30f, 0.16f, 1.0f};
    style.Colors[ImGuiCol_ButtonHovered] = {0.12f, 0.42f, 0.22f, 1.0f};

    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    m_initialized = true;
    return true;
}

void RuntimeUI::shutdown() {
    if (!m_initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void RuntimeUI::begin_frame() {
    if (!m_initialized) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

RuntimeUIAction RuntimeUI::draw(const GameplayHUDState& state, bool save_enabled, bool paused) {
    RuntimeUIAction action;
    if (!m_initialized) return action;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(
        {viewport->WorkPos.x + viewport->WorkSize.x - 250.0f, viewport->WorkPos.y + 18.0f},
        ImGuiCond_Always
    );
    ImGui::SetNextWindowSize({232.0f, 0.0f}, ImGuiCond_Always);
    if (ImGui::Begin("Seed Controls", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(paused ? "GAME PAUSED" : "GAME RUNNING");
        if (ImGui::Button(paused ? "Resume" : "Pause")) action.toggle_pause = true;
        ImGui::SameLine();
        if (save_enabled && ImGui::Button("Save")) action.save_requested = true;
        if (save_enabled && ImGui::Button("Load")) action.load_requested = true;
    }
    ImGui::End();

    ImGui::SetNextWindowPos({18.0f, 18.0f}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({320.0f, 0.0f}, ImGuiCond_Always);
    constexpr ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("Seed HUD", nullptr, flags)) {
        ImGui::TextUnformatted(state.simulation_visible ? "SEED SHOP" : "SEED ADVENTURE");

        if (state.simulation_visible) {
            ImGui::SeparatorText(state.shop_name.empty() ? "Shop" : state.shop_name.c_str());
            ImGui::Text("Wallet        %.2f", state.wallet_balance);
            ImGui::Text("Today         %.2f", state.daily_revenue);
            ImGui::Text("Day %d  %05.2f  %s",
                state.simulation_day,
                state.simulation_hour,
                state.shop_open ? "OPEN" : "CLOSED");
            ImGui::Text("Stock         %d / %d", state.stock_quantity, state.stock_capacity);
            ImGui::Text("Customers     %d", state.active_customers);
            ImGui::Text("Orders        %d waiting  ·  %d completed",
                state.queued_orders,
                state.completed_orders);
            if (!state.goal_title.empty()) {
                ImGui::SeparatorText("Daily Objective");
                ImGui::TextWrapped("%s", state.goal_title.c_str());
                if (state.goal_target_revenue > 0.0f) {
                    ImGui::Text("Revenue       %.0f / %.0f",
                        state.goal_revenue,
                        state.goal_target_revenue);
                }
                if (state.goal_target_customers > 0) {
                    ImGui::Text("Customers     %d / %d",
                        state.goal_customers,
                        state.goal_target_customers);
                }
                ImGui::TextColored(
                    state.goal_completed
                        ? ImVec4{0.35f, 0.95f, 0.55f, 1.0f}
                        : ImVec4{1.0f, 0.82f, 0.35f, 1.0f},
                    "%s",
                    state.goal_completed ? "GOAL COMPLETE" : "Keep trading");
            }
            ImGui::Separator();
        }

        if (state.health_visible) {
            const float maximum = std::max(0.001f, state.health_maximum);
            const float fraction = std::clamp(
                state.health_current / maximum,
                0.0f,
                1.0f
            );
            ImGui::Text("Health  %.0f / %.0f",
                state.health_current,
                state.health_maximum);
            ImGui::ProgressBar(fraction, {-1.0f, 18.0f});
        }

        if (!state.active_objectives.empty()) {
            ImGui::SeparatorText("Objectives");
            for (const auto& objective : state.active_objectives) {
                ImGui::BulletText("%s", objective.c_str());
            }
        }

        if (!state.status_message.empty()) {
            ImGui::SeparatorText(state.dialogue_visible ? "Dialogue" : "Status");
            ImGui::TextWrapped("%s", state.status_message.c_str());
        }
        if (state.interaction_visible) {
            ImGui::Separator();
            ImGui::TextColored(
                ImVec4{0.35f, 0.95f, 0.55f, 1.0f},
                "%s",
                state.interaction_prompt.c_str()
            );
        }

        if (!state.inventory.empty()) {
            ImGui::SeparatorText("Inventory");
            for (const auto& [item, quantity] : state.inventory) {
                ImGui::Text("%s  x%d", item.c_str(), quantity);
            }
        }

        ImGui::Separator();
        ImGui::TextDisabled(state.simulation_visible
            ? "WASD Move   RMB Look   E Interact   F5 Save   F9 Load"
            : "WASD Move   RMB Look   Space Jump");
        if (!state.simulation_visible) ImGui::TextDisabled("F Attack   E Interact   Shift Sprint");
        if (save_enabled) ImGui::TextDisabled("F5 Save   F9 Load");
    }
    ImGui::End();
    return action;
}

void RuntimeUI::render() {
    if (m_initialized) {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}

} // namespace seed::runtime
