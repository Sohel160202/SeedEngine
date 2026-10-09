#include "StudioUI.h"

#include "seed/gameplay/Components.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace seed::studio {
namespace {

constexpr float LeftPanelWidth = 250.0f;
constexpr float RightPanelWidth = 320.0f;
constexpr float BottomPanelHeight = 180.0f;

void apply_seed_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 5.0f;
    style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = {8.0f, 7.0f};
    style.FramePadding = {8.0f, 5.0f};

    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = {0.035f, 0.045f, 0.040f, 0.985f};
    colors[ImGuiCol_ChildBg] = {0.030f, 0.038f, 0.034f, 1.0f};
    colors[ImGuiCol_PopupBg] = {0.040f, 0.052f, 0.046f, 0.99f};
    colors[ImGuiCol_Border] = {0.13f, 0.20f, 0.16f, 0.85f};
    colors[ImGuiCol_FrameBg] = {0.075f, 0.095f, 0.083f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = {0.10f, 0.15f, 0.12f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = {0.12f, 0.20f, 0.15f, 1.0f};
    colors[ImGuiCol_TitleBg] = {0.030f, 0.040f, 0.035f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = {0.045f, 0.070f, 0.055f, 1.0f};
    colors[ImGuiCol_MenuBarBg] = {0.025f, 0.033f, 0.029f, 1.0f};
    colors[ImGuiCol_Header] = {0.09f, 0.20f, 0.13f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = {0.12f, 0.30f, 0.18f, 1.0f};
    colors[ImGuiCol_HeaderActive] = {0.14f, 0.36f, 0.21f, 1.0f};
    colors[ImGuiCol_Button] = {0.08f, 0.22f, 0.13f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = {0.10f, 0.32f, 0.18f, 1.0f};
    colors[ImGuiCol_ButtonActive] = {0.12f, 0.39f, 0.21f, 1.0f};
    colors[ImGuiCol_CheckMark] = {0.30f, 0.92f, 0.48f, 1.0f};
    colors[ImGuiCol_SliderGrab] = {0.24f, 0.76f, 0.40f, 1.0f};
    colors[ImGuiCol_SliderGrabActive] = {0.32f, 0.95f, 0.50f, 1.0f};
    colors[ImGuiCol_Separator] = {0.12f, 0.19f, 0.15f, 1.0f};
}

ImGuiWindowFlags fixed_panel_flags() {
    return ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings;
}

} // namespace

bool StudioUI::initialize(void* window_handle) {
    if (m_initialized) {
        return true;
    }

    auto* window = static_cast<GLFWwindow*>(window_handle);
    if (window == nullptr) {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    apply_seed_style();

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

void StudioUI::shutdown() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
}

void StudioUI::begin_frame() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void StudioUI::draw(Scene& scene) {
    if (!m_initialized) {
        return;
    }

    if (m_selected_entity != InvalidEntity && !scene.is_alive(m_selected_entity)) {
        m_selected_entity = InvalidEntity;
    }

    draw_main_menu();
    draw_world_panel(scene);
    draw_inspector(scene);
    draw_assets_panel();
    draw_viewport_frame();
}

void StudioUI::render() {
    if (!m_initialized) {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

bool StudioUI::wants_mouse() const noexcept {
    return m_initialized && ImGui::GetIO().WantCaptureMouse;
}

bool StudioUI::wants_keyboard() const noexcept {
    return m_initialized && ImGui::GetIO().WantCaptureKeyboard;
}

StudioUI::~StudioUI() {
    shutdown();
}

void StudioUI::draw_main_menu() {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    ImGui::TextUnformatted("SEED STUDIO");
    ImGui::Separator();

    if (ImGui::BeginMenu("File")) {
        ImGui::MenuItem("New Scene", "Ctrl+N", false, false);
        ImGui::MenuItem("Open Scene...", "Ctrl+O", false, false);
        ImGui::Separator();
        ImGui::MenuItem("Save", "Ctrl+S", false, false);
        ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, false);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create")) {
        ImGui::MenuItem("Empty Entity", nullptr, false, false);
        ImGui::MenuItem("3D Object", nullptr, false, false);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Build")) {
        ImGui::MenuItem("Play", "F5", false, false);
        ImGui::MenuItem("Build Game...", nullptr, false, false);
        ImGui::EndMenu();
    }

    ImGui::SameLine(ImGui::GetWindowWidth() - 185.0f);
    ImGui::TextDisabled("Seed Engine pre-alpha");
    ImGui::EndMainMenuBar();
}

void StudioUI::draw_world_panel(Scene& scene) {
    const ImGuiIO& io = ImGui::GetIO();
    const float menu_height = ImGui::GetFrameHeight();
    const float panel_height = std::max(120.0f, io.DisplaySize.y - menu_height - BottomPanelHeight);

    ImGui::SetNextWindowPos({0.0f, menu_height}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({LeftPanelWidth, panel_height}, ImGuiCond_Always);

    if (ImGui::Begin("World", nullptr, fixed_panel_flags())) {
        ImGui::TextDisabled("SCENE");
        ImGui::Separator();

        std::vector<std::pair<std::string, EntityId>> entities;
        entities.reserve(scene.entity_count());
        scene.for_each_entity([&](EntityId entity, const std::string& name) {
            entities.emplace_back(name, entity);
        });

        std::sort(entities.begin(), entities.end(), [](const auto& left, const auto& right) {
            return left.first < right.first;
        });

        for (const auto& [name, entity] : entities) {
            const bool selected = entity == m_selected_entity;
            const std::string label = name + "##" + std::to_string(entity);
            if (ImGui::Selectable(label.c_str(), selected)) {
                m_selected_entity = entity;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("%zu entities", scene.entity_count());
    }
    ImGui::End();
}

void StudioUI::draw_inspector(Scene& scene) {
    const ImGuiIO& io = ImGui::GetIO();
    const float menu_height = ImGui::GetFrameHeight();
    const float panel_height = std::max(120.0f, io.DisplaySize.y - menu_height - BottomPanelHeight);

    ImGui::SetNextWindowPos({io.DisplaySize.x - RightPanelWidth, menu_height}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({RightPanelWidth, panel_height}, ImGuiCond_Always);

    if (ImGui::Begin("Inspector", nullptr, fixed_panel_flags())) {
        if (m_selected_entity == InvalidEntity || !scene.is_alive(m_selected_entity)) {
            ImGui::TextDisabled("Select an entity from World.");
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted(scene.entity_name(m_selected_entity).c_str());
        ImGui::TextDisabled("Entity #%llu", static_cast<unsigned long long>(m_selected_entity));
        ImGui::Separator();

        if (auto* transform = scene.get_component<TransformComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Position", &transform->position.x, 0.05f);
                ImGui::DragFloat3("Rotation", &transform->rotation_degrees.x, 0.5f);
                ImGui::DragFloat3("Scale", &transform->scale.x, 0.02f, 0.01f, 100.0f);
            }
        }

        if (auto* camera = scene.get_component<CameraComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Primary", &camera->primary);
                ImGui::Checkbox("Enabled", &camera->enabled);
                ImGui::DragFloat("Field of View", &camera->field_of_view_degrees, 0.25f, 20.0f, 150.0f, "%.1f deg");
                ImGui::DragFloat("Near Plane", &camera->near_plane, 0.01f, 0.01f, 10.0f, "%.2f");
                ImGui::DragFloat("Far Plane", &camera->far_plane, 1.0f, 10.0f, 10000.0f, "%.0f");
            }
        }

        if (auto* mesh = scene.get_component<MeshComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Visible", &mesh->visible);
                ImGui::Text("Resource: Mesh #%u", mesh->mesh.value);
            }
        }

        if (auto* material = scene.get_component<MaterialComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Text("Shader: #%u", material->shader.value);
            }
        }

        ImGui::Spacing();
        if (ImGui::Button("+ Add Gameplay", {-1.0f, 0.0f})) {
            ImGui::OpenPopup("AddGameplayPopup");
        }

        if (ImGui::BeginPopup("AddGameplayPopup")) {
            ImGui::TextDisabled("Gameplay authoring is coming next.");
            ImGui::Separator();
            ImGui::MenuItem("Interactable", nullptr, false, false);
            ImGui::MenuItem("Health", nullptr, false, false);
            ImGui::MenuItem("Door / Lock", nullptr, false, false);
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void StudioUI::draw_assets_panel() {
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0.0f, io.DisplaySize.y - BottomPanelHeight}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({io.DisplaySize.x, BottomPanelHeight}, ImGuiCond_Always);

    if (ImGui::Begin("Assets", nullptr, fixed_panel_flags())) {
        ImGui::TextDisabled("PROJECT ASSETS");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextUnformatted("Seed Cube Mesh");
        ImGui::SameLine(180.0f);
        ImGui::TextDisabled("Built-in preview mesh");
        ImGui::TextUnformatted("Default Seed Material");
        ImGui::SameLine(180.0f);
        ImGui::TextDisabled("Vertex-color material");
        ImGui::Spacing();
        ImGui::TextDisabled("Asset importing arrives with the Seed asset database.");
    }
    ImGui::End();
}

void StudioUI::draw_viewport_frame() {
    const ImGuiIO& io = ImGui::GetIO();
    const float menu_height = ImGui::GetFrameHeight();
    const ImVec2 min{LeftPanelWidth, menu_height};
    const ImVec2 max{
        std::max(LeftPanelWidth + 20.0f, io.DisplaySize.x - RightPanelWidth),
        std::max(menu_height + 20.0f, io.DisplaySize.y - BottomPanelHeight)
    };

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    draw_list->AddRect(min, max, IM_COL32(44, 72, 55, 255), 0.0f, 0, 1.0f);

    const ImVec2 badge_min{min.x + 12.0f, min.y + 12.0f};
    const ImVec2 badge_max{badge_min.x + 112.0f, badge_min.y + 25.0f};
    draw_list->AddRectFilled(badge_min, badge_max, IM_COL32(9, 18, 13, 220), 4.0f);
    draw_list->AddText({badge_min.x + 9.0f, badge_min.y + 5.0f}, IM_COL32(180, 220, 194, 255), "3D VIEWPORT");

    const char* controls = "RMB Look   WASD Move   Q/E Up/Down   Shift Fast   Wheel Speed";
    const ImVec2 text_size = ImGui::CalcTextSize(controls);
    const ImVec2 controls_pos{
        min.x + (max.x - min.x - text_size.x) * 0.5f,
        max.y - text_size.y - 10.0f
    };
    draw_list->AddText(controls_pos, IM_COL32(160, 188, 170, 220), controls);
}

} // namespace seed::studio
