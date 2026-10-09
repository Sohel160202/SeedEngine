#include "StudioUI.h"
#include "GameplayAuthoring.h"

#include "seed/gameplay/Components.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Scene.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cstdio>
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

void copy_text(auto& destination, const std::string& value) {
    std::snprintf(destination.data(), destination.size(), "%s", value.c_str());
}

void center_next_dialog() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(
        {io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f},
        ImGuiCond_Appearing,
        {0.5f, 0.5f}
    );
    ImGui::SetNextWindowSize({560.0f, 0.0f}, ImGuiCond_Appearing);
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

    copy_text(m_project_name_buffer, "My Seed Game");
    copy_text(m_project_folder_buffer, "SeedProjects");
    copy_text(m_project_file_buffer, "SeedProjects/My_Seed_Game.seedproject");
    copy_text(m_import_model_file_buffer, "");

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
    m_scene_edited = false;
}

void StudioUI::draw(Scene& scene, const StudioDocumentInfo& document) {
    if (!m_initialized) {
        return;
    }

    if (m_selected_entity != InvalidEntity && !scene.is_alive(m_selected_entity)) {
        m_selected_entity = InvalidEntity;
    }

    draw_main_menu(document);
    if (!document.playing) {
        draw_project_dialogs();
    }
    draw_world_panel(scene);
    draw_inspector(scene, document);
    draw_assets_panel(document);
    draw_viewport_frame(document);

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
            queue_action({.type = document.playing ? StudioActionType::Stop : StudioActionType::Play});
        }

        if (!document.playing) {
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
                copy_text(m_project_name_buffer, document.project_name == "Untitled" ? "My Seed Game" : document.project_name);
                m_show_save_as_dialog = true;
            } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
                if (document.has_project) {
                    queue_action({.type = StudioActionType::Save});
                } else {
                    copy_text(m_project_name_buffer, document.project_name == "Untitled" ? "My Seed Game" : document.project_name);
                    m_show_save_as_dialog = true;
                }
            }

            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
                m_show_open_project_dialog = true;
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false)) {
                queue_action({.type = StudioActionType::NewScene});
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false) && m_selected_entity != InvalidEntity) {
                queue_action({.type = StudioActionType::DuplicateSelected});
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && m_selected_entity != InvalidEntity) {
                queue_action({.type = StudioActionType::DeleteSelected});
            }
        }
    }
}

void StudioUI::render() {
    if (!m_initialized) {
        return;
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

StudioAction StudioUI::take_action() {
    StudioAction action = std::move(m_pending_action);
    m_pending_action = {};
    return action;
}

bool StudioUI::consume_scene_edited() noexcept {
    const bool edited = m_scene_edited;
    m_scene_edited = false;
    return edited;
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

void StudioUI::queue_action(StudioAction action) {
    if (m_pending_action.type == StudioActionType::None) {
        m_pending_action = std::move(action);
    }
}

void StudioUI::draw_main_menu(const StudioDocumentInfo& document) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }

    ImGui::TextUnformatted("SEED STUDIO");
    ImGui::Separator();

    if (ImGui::BeginMenu("File", !document.playing)) {
        if (ImGui::MenuItem("New Project...")) {
            copy_text(m_project_name_buffer, "My Seed Game");
            m_show_new_project_dialog = true;
        }
        if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
            m_show_open_project_dialog = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
            queue_action({.type = StudioActionType::NewScene});
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) {
            if (document.has_project) {
                queue_action({.type = StudioActionType::Save});
            } else {
                copy_text(m_project_name_buffer, document.project_name == "Untitled" ? "My Seed Game" : document.project_name);
                m_show_save_as_dialog = true;
            }
        }
        if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
            copy_text(m_project_name_buffer, document.project_name == "Untitled" ? "My Seed Game" : document.project_name);
            m_show_save_as_dialog = true;
        }
        ImGui::Separator();
        ImGui::BeginDisabled(!document.has_project);
        if (ImGui::MenuItem("Import 3D Model...")) {
            m_show_import_model_dialog = true;
        }
        ImGui::EndDisabled();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit", !document.playing)) {
        ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
        ImGui::MenuItem("Redo", "Ctrl+Y", false, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selected_entity != InvalidEntity)) {
            queue_action({.type = StudioActionType::DuplicateSelected});
        }
        if (ImGui::MenuItem("Delete", "Del", false, m_selected_entity != InvalidEntity)) {
            queue_action({.type = StudioActionType::DeleteSelected});
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Create", !document.playing)) {
        if (ImGui::MenuItem("Empty Entity")) {
            queue_action({.type = StudioActionType::CreateEmptyEntity});
        }
        if (ImGui::MenuItem("Cube")) {
            queue_action({.type = StudioActionType::CreateCube});
        }
        ImGui::Separator();
        if (ImGui::BeginMenu("Player")) {
            if (ImGui::MenuItem("First Person")) {
                queue_action({.type = StudioActionType::CreateFirstPersonPlayer});
            }
            ImGui::MenuItem("Third Person", nullptr, false, false);
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Build")) {
        if (document.playing) {
            if (ImGui::MenuItem("Stop", "F5")) {
                queue_action({.type = StudioActionType::Stop});
            }
        } else if (ImGui::MenuItem("Play", "F5")) {
            queue_action({.type = StudioActionType::Play});
        }
        ImGui::Separator();
        ImGui::MenuItem("Build Game...", nullptr, false, false);
        ImGui::EndMenu();
    }

    std::string document_label = document.project_name + " — " + document.scene_name;
    if (document.dirty) {
        document_label += " *";
    }
    if (document.playing) {
        document_label += "   [PLAY]";
    }

    const float label_width = ImGui::CalcTextSize(document_label.c_str()).x;
    ImGui::SameLine(std::max(450.0f, ImGui::GetWindowWidth() - label_width - 18.0f));
    if (document.playing) {
        ImGui::Text("%s", document_label.c_str());
    } else {
        ImGui::TextDisabled("%s", document_label.c_str());
    }
    ImGui::EndMainMenuBar();
}

void StudioUI::draw_project_dialogs() {
    const ImGuiWindowFlags dialog_flags = ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (m_show_new_project_dialog) {
        center_next_dialog();
        if (ImGui::Begin("New Seed Project", &m_show_new_project_dialog, dialog_flags)) {
            ImGui::TextUnformatted("Create a Seed project");
            ImGui::Separator();
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("Project Name", m_project_name_buffer.data(), m_project_name_buffer.size());
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("Project Folder", m_project_folder_buffer.data(), m_project_folder_buffer.size());
            ImGui::TextDisabled("Creates <name>.seedproject and Scenes/Main.seedscene");
            ImGui::Spacing();

            if (ImGui::Button("Create", {110.0f, 0.0f})) {
                queue_action({
                    .type = StudioActionType::NewProject,
                    .path = m_project_folder_buffer.data(),
                    .project_name = m_project_name_buffer.data(),
                });
                m_show_new_project_dialog = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {110.0f, 0.0f})) {
                m_show_new_project_dialog = false;
            }
        }
        ImGui::End();
    }

    if (m_show_open_project_dialog) {
        center_next_dialog();
        if (ImGui::Begin("Open Seed Project", &m_show_open_project_dialog, dialog_flags)) {
            ImGui::TextUnformatted("Open an existing .seedproject file");
            ImGui::Separator();
            ImGui::SetNextItemWidth(410.0f);
            ImGui::InputText("Project File", m_project_file_buffer.data(), m_project_file_buffer.size());
            ImGui::Spacing();

            if (ImGui::Button("Open", {110.0f, 0.0f})) {
                queue_action({
                    .type = StudioActionType::OpenProject,
                    .path = m_project_file_buffer.data(),
                });
                m_show_open_project_dialog = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {110.0f, 0.0f})) {
                m_show_open_project_dialog = false;
            }
        }
        ImGui::End();
    }

    if (m_show_save_as_dialog) {
        center_next_dialog();
        if (ImGui::Begin("Save Seed Project As", &m_show_save_as_dialog, dialog_flags)) {
            ImGui::TextUnformatted("Save the current scene as a Seed project");
            ImGui::Separator();
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("Project Name", m_project_name_buffer.data(), m_project_name_buffer.size());
            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("Project Folder", m_project_folder_buffer.data(), m_project_folder_buffer.size());
            ImGui::Spacing();

            if (ImGui::Button("Save", {110.0f, 0.0f})) {
                queue_action({
                    .type = StudioActionType::SaveAs,
                    .path = m_project_folder_buffer.data(),
                    .project_name = m_project_name_buffer.data(),
                });
                m_show_save_as_dialog = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {110.0f, 0.0f})) {
                m_show_save_as_dialog = false;
            }
        }
        ImGui::End();
    }

    if (m_show_import_model_dialog) {
        center_next_dialog();
        if (ImGui::Begin("Import 3D Model", &m_show_import_model_dialog, dialog_flags)) {
            ImGui::TextUnformatted("Import a static glTF 2.0 model into this Seed project");
            ImGui::Separator();
            ImGui::SetNextItemWidth(430.0f);
            ImGui::InputText("Model File", m_import_model_file_buffer.data(), m_import_model_file_buffer.size());
            ImGui::TextDisabled("Seed import v0: .glb / .gltf, first triangle primitive, base-color texture.");
            ImGui::TextDisabled("The source and relative .gltf dependencies are copied into Assets/Imported/.");
            ImGui::Spacing();

            if (ImGui::Button("Import", {110.0f, 0.0f}) && m_import_model_file_buffer[0] != '\0') {
                queue_action({
                    .type = StudioActionType::ImportModel,
                    .path = m_import_model_file_buffer.data(),
                });
                m_show_import_model_dialog = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", {110.0f, 0.0f})) {
                m_show_import_model_dialog = false;
            }
        }
        ImGui::End();
    }
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

void StudioUI::draw_inspector(Scene& scene, const StudioDocumentInfo& document) {
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
        ImGui::TextDisabled("Runtime Entity #%llu", static_cast<unsigned long long>(m_selected_entity));
        const auto& persistent_id = scene.entity_persistent_id(m_selected_entity);
        ImGui::TextDisabled("Persistent %.8s...", persistent_id.c_str());
        if (document.playing) {
            ImGui::TextDisabled("Play Mode — Inspector is read-only. Runtime changes are discarded on Stop.");
        }
        ImGui::Separator();

        const auto* selected_camera = scene.get_component<CameraComponent>(m_selected_entity);
        const bool editor_only = selected_camera != nullptr && selected_camera->editor_only;

        ImGui::BeginDisabled(document.playing);

        if (auto* transform = scene.get_component<TransformComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool changed = false;
                changed |= ImGui::DragFloat3("Position", &transform->position.x, 0.05f);
                changed |= ImGui::DragFloat3("Rotation", &transform->rotation_degrees.x, 0.5f);
                changed |= ImGui::DragFloat3("Scale", &transform->scale.x, 0.02f, 0.01f, 100.0f);
                if (changed && !editor_only) {
                    m_scene_edited = true;
                }
            }
        }

        if (auto* camera = scene.get_component<CameraComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool changed = false;
                if (camera->editor_only) {
                    ImGui::TextDisabled("Seed Studio viewport camera — not saved in .seedscene");
                }
                changed |= ImGui::Checkbox("Primary", &camera->primary);
                changed |= ImGui::Checkbox("Enabled", &camera->enabled);
                changed |= ImGui::DragFloat("Field of View", &camera->field_of_view_degrees, 0.25f, 20.0f, 150.0f, "%.1f deg");
                changed |= ImGui::DragFloat("Near Plane", &camera->near_plane, 0.01f, 0.01f, 10.0f, "%.2f");
                changed |= ImGui::DragFloat("Far Plane", &camera->far_plane, 1.0f, 10.0f, 10000.0f, "%.0f");
                if (changed && !camera->editor_only) {
                    m_scene_edited = true;
                }
            }
        }

        if (auto* mesh = scene.get_component<MeshComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen)) {
                m_scene_edited |= ImGui::Checkbox("Visible", &mesh->visible);
                ImGui::TextWrapped("Asset: %s", mesh->asset_id.c_str());
                ImGui::TextDisabled("Runtime Mesh #%u", mesh->mesh.value);
            }
        }

        if (auto* material = scene.get_component<MaterialComponent>(m_selected_entity)) {
            if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::TextWrapped("Asset: %s", material->asset_id.c_str());
                ImGui::Text("Base Color: %.2f  %.2f  %.2f  %.2f",
                    material->base_color.x,
                    material->base_color.y,
                    material->base_color.z,
                    material->base_color.w);
                ImGui::TextDisabled("Runtime Shader #%u", material->shader.value);
                if (material->base_color_texture) {
                    ImGui::TextDisabled("Runtime Texture #%u", material->base_color_texture.value);
                } else {
                    ImGui::TextDisabled("Base Color Texture: None");
                }
            }
        }

        if (!editor_only) {
            m_scene_edited |= draw_gameplay_components(scene, m_selected_entity);

            ImGui::Spacing();
            if (ImGui::Button("+ Add Gameplay", {-1.0f, 0.0f})) {
                ImGui::OpenPopup("AddGameplayPopup");
            }

            if (ImGui::BeginPopup("AddGameplayPopup")) {
                if (draw_add_gameplay_popup(scene, m_selected_entity)) {
                    m_scene_edited = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        } else {
            ImGui::Spacing();
            ImGui::TextDisabled("Editor-only entities cannot receive gameplay components.");
        }

        ImGui::EndDisabled();
    }
    ImGui::End();
}

void StudioUI::draw_assets_panel(const StudioDocumentInfo& document) {
    const ImGuiIO& io = ImGui::GetIO();

    ImGui::SetNextWindowPos({0.0f, io.DisplaySize.y - BottomPanelHeight}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({io.DisplaySize.x, BottomPanelHeight}, ImGuiCond_Always);

    if (ImGui::Begin("Assets", nullptr, fixed_panel_flags())) {
        ImGui::TextDisabled(document.playing ? "PLAY MODE" : "PROJECT ASSETS");
        ImGui::Separator();
        ImGui::Spacing();

        if (document.playing) {
            ImGui::TextUnformatted("Runtime scene is a temporary clone. Stop returns to the editor world unchanged.");
            if (!document.gameplay_status.empty()) {
                ImGui::TextWrapped("%s", document.gameplay_status.c_str());
            }
            if (!document.gameplay_prompt.empty()) {
                ImGui::Text("Interaction: %s", document.gameplay_prompt.c_str());
            }

            ImGui::SeparatorText("Player Inventory");
            if (document.gameplay_inventory.empty()) {
                ImGui::TextDisabled("Empty");
            } else {
                for (const auto& [item_id, quantity] : document.gameplay_inventory) {
                    ImGui::Text("%s  x%d", item_id.c_str(), quantity);
                }
            }

            ImGui::TextDisabled("F5 Stop   RMB Look   WASD Walk   Space Jump   Shift Sprint   E Interact");
        } else {
            ImGui::BeginDisabled(!document.has_project);
            if (ImGui::Button("+ Import 3D Model...")) {
                m_show_import_model_dialog = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled(document.has_project
                ? "Static .glb / .gltf"
                : "Save the project before importing assets");

            ImGui::TextUnformatted("Seed Cube Mesh");
            ImGui::SameLine(180.0f);
            ImGui::TextDisabled("builtin:cube");
            ImGui::TextUnformatted("Default Seed Material");
            ImGui::SameLine(180.0f);
            ImGui::TextDisabled("builtin:seed_default");

            for (const auto& asset_id : document.project_assets) {
                ImGui::TextUnformatted("Imported Model");
                ImGui::SameLine(180.0f);
                ImGui::TextDisabled("%s", asset_id.c_str());
            }

            if (!document.status_message.empty()) {
                ImGui::TextWrapped("%s", document.status_message.c_str());
            } else if (!document.has_project) {
                ImGui::TextDisabled("Unsaved project — File > Save As... to create .seedproject and .seedscene files.");
            }
        }
    }
    ImGui::End();
}

void StudioUI::draw_viewport_frame(const StudioDocumentInfo& document) {
    const ImGuiIO& io = ImGui::GetIO();
    const float menu_height = ImGui::GetFrameHeight();
    const ImVec2 min{LeftPanelWidth, menu_height};
    const ImVec2 max{
        std::max(LeftPanelWidth + 20.0f, io.DisplaySize.x - RightPanelWidth),
        std::max(menu_height + 20.0f, io.DisplaySize.y - BottomPanelHeight)
    };

    ImDrawList* draw_list = ImGui::GetForegroundDrawList();
    const ImU32 border_color = document.playing
        ? IM_COL32(70, 190, 105, 255)
        : IM_COL32(44, 72, 55, 255);
    draw_list->AddRect(min, max, border_color, 0.0f, 0, document.playing ? 2.0f : 1.0f);

    const char* badge_text = document.playing ? "PLAY MODE" : "3D VIEWPORT";
    const float badge_width = document.playing ? 100.0f : 112.0f;
    const ImVec2 badge_min{min.x + 12.0f, min.y + 12.0f};
    const ImVec2 badge_max{badge_min.x + badge_width, badge_min.y + 25.0f};
    draw_list->AddRectFilled(badge_min, badge_max, IM_COL32(9, 18, 13, 230), 4.0f);
    draw_list->AddText(
        {badge_min.x + 9.0f, badge_min.y + 5.0f},
        document.playing ? IM_COL32(115, 245, 145, 255) : IM_COL32(180, 220, 194, 255),
        badge_text
    );

    const char* controls = document.playing
        ? "RMB Look   WASD Walk   Space Jump   Shift Sprint   E Interact   F5 Stop"
        : "RMB Look   WASD Move   Q/E Up/Down   Shift Fast   Wheel Speed";
    const ImVec2 text_size = ImGui::CalcTextSize(controls);
    const ImVec2 controls_pos{
        min.x + (max.x - min.x - text_size.x) * 0.5f,
        max.y - text_size.y - 10.0f
    };
    draw_list->AddText(controls_pos, IM_COL32(160, 188, 170, 220), controls);

    if (document.playing && !document.gameplay_prompt.empty()) {
        const ImVec2 prompt_size = ImGui::CalcTextSize(document.gameplay_prompt.c_str());
        const ImVec2 prompt_padding{14.0f, 8.0f};
        const ImVec2 prompt_min{
            min.x + (max.x - min.x - prompt_size.x) * 0.5f - prompt_padding.x,
            max.y - 72.0f - prompt_size.y - prompt_padding.y
        };
        const ImVec2 prompt_max{
            prompt_min.x + prompt_size.x + prompt_padding.x * 2.0f,
            prompt_min.y + prompt_size.y + prompt_padding.y * 2.0f
        };
        draw_list->AddRectFilled(prompt_min, prompt_max, IM_COL32(4, 9, 6, 220), 5.0f);
        draw_list->AddText(
            {prompt_min.x + prompt_padding.x, prompt_min.y + prompt_padding.y},
            IM_COL32(230, 245, 234, 255),
            document.gameplay_prompt.c_str()
        );
    }
}

} // namespace seed::studio
