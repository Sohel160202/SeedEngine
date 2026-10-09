#include "StudioUI.h"
#include "GameplayAuthoring.h"

#include "seed/gameplay/Components.h"
#include "seed/physics/PhysicsComponents.h"
#include "seed/render/RenderComponents.h"
#include "seed/scene/Hierarchy.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <ImGuizmo.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace seed::studio {
namespace {

constexpr float LeftPanelWidth = 250.0f;
constexpr float RightPanelWidth = 320.0f;
constexpr float BottomPanelHeight = 180.0f;
constexpr std::size_t HistoryLimit = 64;

void apply_seed_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f; style.ChildRounding = 4.0f; style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f; style.ScrollbarRounding = 5.0f; style.GrabRounding = 4.0f;
    style.WindowBorderSize = 1.0f; style.ItemSpacing = {8.0f, 7.0f}; style.FramePadding = {8.0f, 5.0f};
    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = {0.035f,0.045f,0.040f,0.985f}; colors[ImGuiCol_ChildBg] = {0.030f,0.038f,0.034f,1.0f};
    colors[ImGuiCol_PopupBg] = {0.040f,0.052f,0.046f,0.99f}; colors[ImGuiCol_Border] = {0.13f,0.20f,0.16f,0.85f};
    colors[ImGuiCol_FrameBg] = {0.075f,0.095f,0.083f,1.0f}; colors[ImGuiCol_FrameBgHovered] = {0.10f,0.15f,0.12f,1.0f};
    colors[ImGuiCol_FrameBgActive] = {0.12f,0.20f,0.15f,1.0f}; colors[ImGuiCol_MenuBarBg] = {0.025f,0.033f,0.029f,1.0f};
    colors[ImGuiCol_Header] = {0.09f,0.20f,0.13f,1.0f}; colors[ImGuiCol_HeaderHovered] = {0.12f,0.30f,0.18f,1.0f};
    colors[ImGuiCol_HeaderActive] = {0.14f,0.36f,0.21f,1.0f}; colors[ImGuiCol_Button] = {0.08f,0.22f,0.13f,1.0f};
    colors[ImGuiCol_ButtonHovered] = {0.10f,0.32f,0.18f,1.0f}; colors[ImGuiCol_ButtonActive] = {0.12f,0.39f,0.21f,1.0f};
    colors[ImGuiCol_CheckMark] = {0.30f,0.92f,0.48f,1.0f}; colors[ImGuiCol_SliderGrab] = {0.24f,0.76f,0.40f,1.0f};
    colors[ImGuiCol_SliderGrabActive] = {0.32f,0.95f,0.50f,1.0f}; colors[ImGuiCol_Separator] = {0.12f,0.19f,0.15f,1.0f};
}

ImGuiWindowFlags fixed_panel_flags() {
    return ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
}

void copy_text(auto& destination, const std::string& value) { std::snprintf(destination.data(), destination.size(), "%s", value.c_str()); }

void center_next_dialog() {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f}, ImGuiCond_Appearing, {0.5f,0.5f});
    ImGui::SetNextWindowSize({560.0f,0.0f}, ImGuiCond_Appearing);
}

bool editor_only_entity(Scene& scene, EntityId entity) {
    const auto* camera = scene.get_component<CameraComponent>(entity);
    return camera != nullptr && camera->editor_only;
}

bool build_camera_matrices(Scene& scene, EntityId camera_entity, Mat4& view, Mat4& projection, Mat4& view_projection) {
    if (camera_entity == InvalidEntity || !scene.is_alive(camera_entity)) return false;
    const auto* camera = scene.get_component<CameraComponent>(camera_entity);
    if (camera == nullptr || !camera->enabled || !scene.has_component<TransformComponent>(camera_entity)) return false;
    const TransformComponent camera_world = world_transform(scene, camera_entity);
    const Vec3 forward = forward_from_euler(camera_world.rotation_degrees);
    view = look_at_matrix(camera_world.position, camera_world.position + forward, {0,1,0});
    const ImGuiIO& io = ImGui::GetIO();
    projection = perspective_matrix(camera->field_of_view_degrees, io.DisplaySize.y > 0 ? io.DisplaySize.x / io.DisplaySize.y : 1.0f, camera->near_plane, camera->far_plane);
    view_projection = projection * view;
    return true;
}

bool project_to_screen(const Mat4& view_projection, const Vec3& world, ImVec2& screen, float& depth) {
    const Vec4 clip = view_projection * Vec4{world.x,world.y,world.z,1.0f};
    if (clip.w <= 0.001f) return false;
    const float x = clip.x / clip.w, y = clip.y / clip.w, z = clip.z / clip.w;
    if (x < -1.25f || x > 1.25f || y < -1.25f || y > 1.25f || z < -1.2f || z > 1.2f) return false;
    const ImGuiIO& io = ImGui::GetIO();
    screen = {(x*0.5f+0.5f)*io.DisplaySize.x, (1.0f-(y*0.5f+0.5f))*io.DisplaySize.y}; depth = z; return true;
}

Vec3 matrix_position(const Mat4& matrix) { return {matrix.values[12],matrix.values[13],matrix.values[14]}; }

} // namespace

bool StudioUI::initialize(void* window_handle) {
    if (m_initialized) return true;
    auto* window = static_cast<GLFWwindow*>(window_handle); if (!window) return false;
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; io.IniFilename = nullptr; apply_seed_style();
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true)) { ImGui::DestroyContext(); return false; }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) { ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); return false; }
    copy_text(m_project_name_buffer,"My Seed Game"); copy_text(m_project_folder_buffer,"SeedProjects");
    copy_text(m_project_file_buffer,"SeedProjects/My_Seed_Game.seedproject"); copy_text(m_import_model_file_buffer,"");
    m_initialized = true; return true;
}

void StudioUI::shutdown() {
    if (!m_initialized) return; ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext(); m_initialized = false;
}

void StudioUI::begin_frame() {
    if (!m_initialized) return; ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame(); ImGuizmo::BeginFrame(); m_scene_edited = false;
}

void StudioUI::draw(Scene& scene, const StudioDocumentInfo& document, EntityId viewport_camera) {
    if (!m_initialized) return;
    m_last_document_playing = document.playing;
    if (m_selected_entity != InvalidEntity && !scene.is_alive(m_selected_entity)) m_selected_entity = InvalidEntity;
    if (!document.playing) begin_history_frame(scene, document);

    draw_main_menu(document);
    if (!document.playing) draw_project_dialogs();
    draw_world_panel(scene); draw_inspector(scene, document); draw_assets_panel(document); draw_viewport_frame(scene, document, viewport_camera);

    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_F5,false)) queue_action({.type=document.playing?StudioActionType::Stop:StudioActionType::Play});
        if (!document.playing) {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z,false)) queue_action({.type=io.KeyShift?StudioActionType::Redo:StudioActionType::Undo});
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y,false)) queue_action({.type=StudioActionType::Redo});
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S,false)) { copy_text(m_project_name_buffer, document.project_name=="Untitled"?"My Seed Game":document.project_name); m_show_save_as_dialog=true; }
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S,false)) { if (document.has_project) queue_action({.type=StudioActionType::Save}); else m_show_save_as_dialog=true; }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O,false)) m_show_open_project_dialog=true;
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N,false)) queue_action({.type=StudioActionType::NewScene});
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D,false) && m_selected_entity!=InvalidEntity) queue_action({.type=StudioActionType::DuplicateSelected});
            if (ImGui::IsKeyPressed(ImGuiKey_Delete,false) && m_selected_entity!=InvalidEntity) queue_action({.type=StudioActionType::DeleteSelected});
            if (!io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                if (ImGui::IsKeyPressed(ImGuiKey_Q,false)) m_gizmo_tool=GizmoTool::Select;
                if (ImGui::IsKeyPressed(ImGuiKey_W,false)) m_gizmo_tool=GizmoTool::Move;
                if (ImGui::IsKeyPressed(ImGuiKey_E,false)) m_gizmo_tool=GizmoTool::Rotate;
                if (ImGui::IsKeyPressed(ImGuiKey_R,false)) m_gizmo_tool=GizmoTool::Scale;
                if (ImGui::IsKeyPressed(ImGuiKey_F,false) && m_selected_entity!=InvalidEntity) queue_action({.type=StudioActionType::FocusSelected});
            }
        }
    }

    if (!document.playing) {
        if (m_pending_action.type == StudioActionType::Undo) { m_pending_action={}; undo(scene); }
        else if (m_pending_action.type == StudioActionType::Redo) { m_pending_action={}; redo(scene); }
        else if (m_pending_action.type == StudioActionType::FocusSelected) { m_pending_action={}; focus_selected(scene, viewport_camera); }
        finish_history_frame(scene);
    }
}

void StudioUI::render() { if (m_initialized) { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); } }
StudioAction StudioUI::take_action() { StudioAction action=std::move(m_pending_action); m_pending_action={}; return action; }
bool StudioUI::consume_scene_edited() noexcept { const bool e=m_scene_edited; m_scene_edited=false; return e; }
bool StudioUI::scene_edit_active() const noexcept { return m_initialized && (ImGui::IsAnyItemActive() || ImGuizmo::IsUsing()); }
bool StudioUI::wants_mouse() const noexcept { return m_initialized && ImGui::GetIO().WantCaptureMouse; }
bool StudioUI::wants_keyboard() const noexcept {
    if (!m_initialized) return false;
    if (m_last_document_playing) return ImGui::GetIO().WantCaptureKeyboard;
    return !ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::GetIO().WantCaptureKeyboard;
}
StudioUI::~StudioUI() { shutdown(); }
void StudioUI::queue_action(StudioAction action) { if (m_pending_action.type==StudioActionType::None) m_pending_action=std::move(action); }

void StudioUI::draw_main_menu(const StudioDocumentInfo& document) {
    if (!ImGui::BeginMainMenuBar()) return;
    ImGui::TextUnformatted("SEED STUDIO"); ImGui::Separator();
    if (ImGui::BeginMenu("File", !document.playing)) {
        if (ImGui::MenuItem("New Project...")) { copy_text(m_project_name_buffer,"My Seed Game"); m_show_new_project_dialog=true; }
        if (ImGui::MenuItem("Open Project...","Ctrl+O")) m_show_open_project_dialog=true; ImGui::Separator();
        if (ImGui::MenuItem("New Scene","Ctrl+N")) queue_action({.type=StudioActionType::NewScene});
        if (ImGui::MenuItem("Save","Ctrl+S")) { if (document.has_project) queue_action({.type=StudioActionType::Save}); else m_show_save_as_dialog=true; }
        if (ImGui::MenuItem("Save As...","Ctrl+Shift+S")) m_show_save_as_dialog=true; ImGui::Separator();
        ImGui::BeginDisabled(!document.has_project); if (ImGui::MenuItem("Import 3D Model...")) m_show_import_model_dialog=true; ImGui::EndDisabled(); ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit", !document.playing)) {
        if (ImGui::MenuItem("Undo","Ctrl+Z",false,!m_undo_stack.empty())) queue_action({.type=StudioActionType::Undo});
        if (ImGui::MenuItem("Redo","Ctrl+Y",false,!m_redo_stack.empty())) queue_action({.type=StudioActionType::Redo}); ImGui::Separator();
        if (ImGui::MenuItem("Duplicate","Ctrl+D",false,m_selected_entity!=InvalidEntity)) queue_action({.type=StudioActionType::DuplicateSelected});
        if (ImGui::MenuItem("Delete","Del",false,m_selected_entity!=InvalidEntity)) queue_action({.type=StudioActionType::DeleteSelected}); ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Create", !document.playing)) {
        if (ImGui::MenuItem("Empty Entity")) queue_action({.type=StudioActionType::CreateEmptyEntity}); if (ImGui::MenuItem("Cube")) queue_action({.type=StudioActionType::CreateCube}); ImGui::Separator();
        if (ImGui::BeginMenu("Environment")) { if (ImGui::MenuItem("Sky")) queue_action({.type=StudioActionType::CreateSky}); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Light")) { if (ImGui::MenuItem("Directional Light")) queue_action({.type=StudioActionType::CreateDirectionalLight}); if (ImGui::MenuItem("Ambient Light")) queue_action({.type=StudioActionType::CreateAmbientLight}); ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Player")) { if (ImGui::MenuItem("First Person")) queue_action({.type=StudioActionType::CreateFirstPersonPlayer}); ImGui::MenuItem("Third Person",nullptr,false,false); ImGui::EndMenu(); }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Build")) { if (document.playing) { if (ImGui::MenuItem("Stop","F5")) queue_action({.type=StudioActionType::Stop}); } else if (ImGui::MenuItem("Play","F5")) queue_action({.type=StudioActionType::Play}); ImGui::Separator(); ImGui::MenuItem("Build Game...",nullptr,false,false); ImGui::EndMenu(); }
    std::string label=document.project_name+" — "+document.scene_name; if(document.dirty)label+=" *"; if(document.playing)label+="   [PLAY]";
    const float width=ImGui::CalcTextSize(label.c_str()).x; ImGui::SameLine(std::max(450.0f,ImGui::GetWindowWidth()-width-18.0f)); if(document.playing)ImGui::Text("%s",label.c_str());else ImGui::TextDisabled("%s",label.c_str()); ImGui::EndMainMenuBar();
}

void StudioUI::draw_project_dialogs() {
    const ImGuiWindowFlags flags=ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize;
    if(m_show_new_project_dialog){center_next_dialog();if(ImGui::Begin("New Seed Project",&m_show_new_project_dialog,flags)){ImGui::InputText("Project Name",m_project_name_buffer.data(),m_project_name_buffer.size());ImGui::InputText("Project Folder",m_project_folder_buffer.data(),m_project_folder_buffer.size());if(ImGui::Button("Create",{110,0})){queue_action({.type=StudioActionType::NewProject,.path=m_project_folder_buffer.data(),.project_name=m_project_name_buffer.data()});m_show_new_project_dialog=false;}ImGui::SameLine();if(ImGui::Button("Cancel",{110,0}))m_show_new_project_dialog=false;}ImGui::End();}
    if(m_show_open_project_dialog){center_next_dialog();if(ImGui::Begin("Open Seed Project",&m_show_open_project_dialog,flags)){ImGui::InputText("Project File",m_project_file_buffer.data(),m_project_file_buffer.size());if(ImGui::Button("Open",{110,0})){queue_action({.type=StudioActionType::OpenProject,.path=m_project_file_buffer.data()});m_show_open_project_dialog=false;}ImGui::SameLine();if(ImGui::Button("Cancel",{110,0}))m_show_open_project_dialog=false;}ImGui::End();}
    if(m_show_save_as_dialog){center_next_dialog();if(ImGui::Begin("Save Seed Project As",&m_show_save_as_dialog,flags)){ImGui::InputText("Project Name",m_project_name_buffer.data(),m_project_name_buffer.size());ImGui::InputText("Project Folder",m_project_folder_buffer.data(),m_project_folder_buffer.size());if(ImGui::Button("Save",{110,0})){queue_action({.type=StudioActionType::SaveAs,.path=m_project_folder_buffer.data(),.project_name=m_project_name_buffer.data()});m_show_save_as_dialog=false;}ImGui::SameLine();if(ImGui::Button("Cancel",{110,0}))m_show_save_as_dialog=false;}ImGui::End();}
    if(m_show_import_model_dialog){center_next_dialog();if(ImGui::Begin("Import 3D Model",&m_show_import_model_dialog,flags)){ImGui::TextUnformatted("Import a static glTF 2.0 model into this Seed project");ImGui::InputText("Model File",m_import_model_file_buffer.data(),m_import_model_file_buffer.size());ImGui::TextDisabled("Seed import v1: first triangle primitive + PBR texture maps.");if(ImGui::Button("Import",{110,0})&&m_import_model_file_buffer[0]!='\0'){queue_action({.type=StudioActionType::ImportModel,.path=m_import_model_file_buffer.data()});m_show_import_model_dialog=false;}ImGui::SameLine();if(ImGui::Button("Cancel",{110,0}))m_show_import_model_dialog=false;}ImGui::End();}
}

void StudioUI::draw_world_panel(Scene& scene) {
    const ImGuiIO& io=ImGui::GetIO();const float menu=ImGui::GetFrameHeight();const float height=std::max(120.0f,io.DisplaySize.y-menu-BottomPanelHeight);
    ImGui::SetNextWindowPos({0,menu},ImGuiCond_Always);ImGui::SetNextWindowSize({LeftPanelWidth,height},ImGuiCond_Always);
    if(ImGui::Begin("World",nullptr,fixed_panel_flags())){
        ImGui::TextDisabled("SCENE HIERARCHY");ImGui::Separator();
        std::unordered_map<EntityId,std::vector<EntityId>> children;std::vector<EntityId> roots;
        scene.for_each_entity([&](EntityId e,const std::string&){const EntityId p=parent_entity(scene,e);if(p==InvalidEntity)roots.push_back(e);else children[p].push_back(e);});
        auto by_name=[&](EntityId a,EntityId b){return scene.entity_name(a)<scene.entity_name(b);};std::sort(roots.begin(),roots.end(),by_name);for(auto&[_,v]:children)std::sort(v.begin(),v.end(),by_name);
        std::function<void(EntityId)> node=[&](EntityId e){const bool hc=!children[e].empty();ImGuiTreeNodeFlags f=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth;if(!hc)f|=ImGuiTreeNodeFlags_Leaf;if(e==m_selected_entity)f|=ImGuiTreeNodeFlags_Selected;
            const bool open=ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(e)),f,"%s",scene.entity_name(e).c_str());if(ImGui::IsItemClicked()&&!ImGui::IsItemToggledOpen())m_selected_entity=e;
            if(ImGui::BeginDragDropSource()){ImGui::SetDragDropPayload("SEED_ENTITY",&e,sizeof(e));ImGui::Text("Move %s",scene.entity_name(e).c_str());ImGui::EndDragDropSource();}
            if(ImGui::BeginDragDropTarget()){if(const ImGuiPayload* p=ImGui::AcceptDragDropPayload("SEED_ENTITY")){const EntityId child=*static_cast<const EntityId*>(p->Data);if(can_parent(scene,child,e)&&set_parent(scene,child,e,true)){m_selected_entity=child;m_scene_edited=true;}}ImGui::EndDragDropTarget();}
            if(ImGui::BeginPopupContextItem()){if(ImGui::MenuItem("Focus Selected","F")){m_selected_entity=e;queue_action({.type=StudioActionType::FocusSelected});}const bool hp=parent_entity(scene,e)!=InvalidEntity;if(ImGui::MenuItem("Move to Root",nullptr,false,hp)&&clear_parent(scene,e,true))m_scene_edited=true;ImGui::EndPopup();}
            if(open){for(EntityId c:children[e])node(c);ImGui::TreePop();}};for(EntityId r:roots)node(r);
        ImGui::Spacing();ImGui::Separator();ImGui::TextDisabled("Drop an entity onto another to parent it.");ImGui::Button("Drop here to move to root",{-1,0});if(ImGui::BeginDragDropTarget()){if(const ImGuiPayload*p=ImGui::AcceptDragDropPayload("SEED_ENTITY")){const EntityId c=*static_cast<const EntityId*>(p->Data);if(clear_parent(scene,c,true)){m_selected_entity=c;m_scene_edited=true;}}ImGui::EndDragDropTarget();}ImGui::TextDisabled("%zu entities",scene.entity_count());}
    ImGui::End();
}

void StudioUI::draw_inspector(Scene& scene,const StudioDocumentInfo& document) {
    const ImGuiIO& io=ImGui::GetIO();const float menu=ImGui::GetFrameHeight();const float height=std::max(120.0f,io.DisplaySize.y-menu-BottomPanelHeight);ImGui::SetNextWindowPos({io.DisplaySize.x-RightPanelWidth,menu},ImGuiCond_Always);ImGui::SetNextWindowSize({RightPanelWidth,height},ImGuiCond_Always);
    if(ImGui::Begin("Inspector",nullptr,fixed_panel_flags())){
        if(m_selected_entity==InvalidEntity||!scene.is_alive(m_selected_entity)){ImGui::TextDisabled("Select an entity from World or click one in the viewport.");ImGui::End();return;}
        ImGui::TextUnformatted(scene.entity_name(m_selected_entity).c_str());ImGui::TextDisabled("Runtime Entity #%llu",static_cast<unsigned long long>(m_selected_entity));const auto&id=scene.entity_persistent_id(m_selected_entity);ImGui::TextDisabled("Persistent %.8s...",id.c_str());if(const EntityId p=parent_entity(scene,m_selected_entity);p!=InvalidEntity)ImGui::TextDisabled("Parent: %s",scene.entity_name(p).c_str());if(document.playing)ImGui::TextDisabled("Play Mode — Inspector is read-only.");ImGui::Separator();
        const bool editor_only=editor_only_entity(scene,m_selected_entity);ImGui::BeginDisabled(document.playing);
        if(auto*t=scene.get_component<TransformComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){if(parent_entity(scene,m_selected_entity)!=InvalidEntity)ImGui::TextDisabled("Local transform relative to parent");bool c=false;c|=ImGui::DragFloat3("Position",&t->position.x,0.05f);c|=ImGui::DragFloat3("Rotation",&t->rotation_degrees.x,0.5f);c|=ImGui::DragFloat3("Scale",&t->scale.x,0.02f,0.01f,100.0f);if(c&&!editor_only)m_scene_edited=true;}}
        if(auto*c=scene.get_component<CameraComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Camera",ImGuiTreeNodeFlags_DefaultOpen)){bool ch=false;if(c->editor_only)ImGui::TextDisabled("Seed Studio viewport camera — not saved");ch|=ImGui::Checkbox("Primary",&c->primary);ch|=ImGui::Checkbox("Enabled",&c->enabled);ch|=ImGui::DragFloat("Field of View",&c->field_of_view_degrees,0.25f,20,150,"%.1f deg");ch|=ImGui::DragFloat("Near Plane",&c->near_plane,0.01f,0.01f,10,"%.2f");ch|=ImGui::DragFloat("Far Plane",&c->far_plane,1,10,10000,"%.0f");if(ch&&!c->editor_only)m_scene_edited=true;}}
        if(auto*l=scene.get_component<DirectionalLightComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Directional Light",ImGuiTreeNodeFlags_DefaultOpen)){bool c=false;c|=ImGui::Checkbox("Enabled##Directional",&l->enabled);c|=ImGui::ColorEdit3("Color##Directional",&l->color.x);c|=ImGui::DragFloat("Intensity##Directional",&l->intensity,0.02f,0,20,"%.2f");c|=ImGui::Checkbox("Cast Shadows",&l->casts_shadows);c|=ImGui::DragFloat("Shadow Distance",&l->shadow_distance,0.25f,5,150,"%.1f m");m_scene_edited|=c;}}
        if(auto*l=scene.get_component<AmbientLightComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Ambient Light",ImGuiTreeNodeFlags_DefaultOpen)){bool c=false;c|=ImGui::Checkbox("Enabled##Ambient",&l->enabled);c|=ImGui::ColorEdit3("Color##Ambient",&l->color.x);c|=ImGui::DragFloat("Intensity##Ambient",&l->intensity,0.01f,0,5,"%.2f");m_scene_edited|=c;}}
        if(auto*s=scene.get_component<SkyComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Sky",ImGuiTreeNodeFlags_DefaultOpen)){bool c=false;c|=ImGui::Checkbox("Enabled##Sky",&s->enabled);c|=ImGui::ColorEdit3("Zenith",&s->zenith_color.x);c|=ImGui::ColorEdit3("Horizon",&s->horizon_color.x);c|=ImGui::DragFloat("Intensity##Sky",&s->intensity,0.01f,0,5,"%.2f");ImGui::TextDisabled("Sky also drives Seed environment lighting / IBL.");m_scene_edited|=c;}}
        if(auto*m=scene.get_component<MeshComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Mesh",ImGuiTreeNodeFlags_DefaultOpen)){bool c=false;c|=ImGui::Checkbox("Visible",&m->visible);c|=ImGui::Checkbox("Cast Shadows",&m->cast_shadows);c|=ImGui::Checkbox("Receive Shadows",&m->receive_shadows);m_scene_edited|=c;ImGui::TextWrapped("Asset: %s",m->asset_id.c_str());ImGui::TextDisabled("Runtime Mesh #%u",m->mesh.value);}}
        if(auto*m=scene.get_component<MaterialComponent>(m_selected_entity)){if(ImGui::CollapsingHeader("Material — PBR",ImGuiTreeNodeFlags_DefaultOpen)){ImGui::TextWrapped("Asset: %s",m->asset_id.c_str());bool c=false;ImGui::SeparatorText("Surface");c|=ImGui::ColorEdit4("Base Color",&m->base_color.x);c|=ImGui::SliderFloat("Metallic",&m->metallic,0,1,"%.2f");c|=ImGui::SliderFloat("Roughness",&m->roughness,0.04f,1,"%.2f");ImGui::SeparatorText("Texture Maps");if(m->base_color_texture)c|=ImGui::Checkbox("Use Base Color Map",&m->use_base_color_texture);else ImGui::TextDisabled("Base Color Map: None");if(m->metallic_roughness_texture)c|=ImGui::Checkbox("Use Metallic/Roughness Map",&m->use_metallic_roughness_texture);else ImGui::TextDisabled("Metallic/Roughness Map: None");if(m->normal_texture){c|=ImGui::Checkbox("Use Normal Map",&m->use_normal_texture);ImGui::BeginDisabled(!m->use_normal_texture);c|=ImGui::SliderFloat("Normal Strength",&m->normal_scale,0,4,"%.2f");ImGui::EndDisabled();}else ImGui::TextDisabled("Normal Map: None");if(c){m->use_asset_defaults=false;m_scene_edited=true;}}}
        if(!editor_only){m_scene_edited|=draw_gameplay_components(scene,m_selected_entity);ImGui::Spacing();if(ImGui::Button("+ Add Gameplay",{-1,0}))ImGui::OpenPopup("AddGameplayPopup");if(ImGui::BeginPopup("AddGameplayPopup")){if(draw_add_gameplay_popup(scene,m_selected_entity)){m_scene_edited=true;ImGui::CloseCurrentPopup();}ImGui::EndPopup();}}else ImGui::TextDisabled("Editor-only entities cannot receive gameplay components.");
        ImGui::EndDisabled();}
    ImGui::End();
}

void StudioUI::draw_assets_panel(const StudioDocumentInfo& document) {
    const ImGuiIO& io=ImGui::GetIO();ImGui::SetNextWindowPos({0,io.DisplaySize.y-BottomPanelHeight},ImGuiCond_Always);ImGui::SetNextWindowSize({io.DisplaySize.x,BottomPanelHeight},ImGuiCond_Always);
    if(ImGui::Begin("Assets",nullptr,fixed_panel_flags())){ImGui::TextDisabled(document.playing?"PLAY MODE":"PROJECT ASSETS");ImGui::Separator();if(document.playing){if(!document.gameplay_status.empty())ImGui::TextWrapped("%s",document.gameplay_status.c_str());if(!document.gameplay_prompt.empty())ImGui::Text("Interaction: %s",document.gameplay_prompt.c_str());ImGui::SeparatorText("Player Inventory");if(document.gameplay_inventory.empty())ImGui::TextDisabled("Empty");else for(const auto&[id,q]:document.gameplay_inventory)ImGui::Text("%s  x%d",id.c_str(),q);ImGui::TextDisabled("F5 Stop   RMB Look   WASD Walk   Space Jump   Shift Sprint   E Interact");}else{ImGui::BeginDisabled(!document.has_project);if(ImGui::Button("+ Import 3D Model..."))m_show_import_model_dialog=true;ImGui::EndDisabled();ImGui::SameLine();ImGui::TextDisabled(document.has_project?"Static .glb / .gltf":"Save project before importing");ImGui::TextUnformatted("Seed Cube Mesh");ImGui::SameLine(180);ImGui::TextDisabled("builtin:cube");ImGui::TextUnformatted("Default Seed PBR Material");ImGui::SameLine(180);ImGui::TextDisabled("builtin:seed_default");for(const auto&a:document.project_assets){ImGui::TextUnformatted("Imported Model");ImGui::SameLine(180);ImGui::TextDisabled("%s",a.c_str());}if(!document.status_message.empty())ImGui::TextWrapped("%s",document.status_message.c_str());}}ImGui::End();
}

void StudioUI::draw_viewport_toolbar(const StudioDocumentInfo& document) {
    if(document.playing)return;const float menu=ImGui::GetFrameHeight();ImGui::SetNextWindowPos({LeftPanelWidth+12,menu+12},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(0.92f);const ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove;
    if(ImGui::Begin("##SeedViewportToolbar",nullptr,flags)){auto button=[&](const char*text,GizmoTool tool){if(m_gizmo_tool==tool)ImGui::PushStyleColor(ImGuiCol_Button,ImVec4{0.12f,0.39f,0.21f,1});if(ImGui::Button(text))m_gizmo_tool=tool;if(m_gizmo_tool==tool)ImGui::PopStyleColor();ImGui::SameLine();};button("Q Select",GizmoTool::Select);button("W Move",GizmoTool::Move);button("E Rotate",GizmoTool::Rotate);button("R Scale",GizmoTool::Scale);if(ImGui::Button(m_local_space?"Local":"World"))m_local_space=!m_local_space;ImGui::SameLine();ImGui::Checkbox("Snap",&m_snap_enabled);if(m_snap_enabled){ImGui::SameLine();float*v=m_gizmo_tool==GizmoTool::Rotate?&m_rotate_snap:(m_gizmo_tool==GizmoTool::Scale?&m_scale_snap:&m_move_snap);ImGui::SetNextItemWidth(75);ImGui::DragFloat("##SnapValue",v,m_gizmo_tool==GizmoTool::Rotate?1.0f:0.05f,0.001f,100,m_gizmo_tool==GizmoTool::Rotate?"%.0f°":"%.2f");}}ImGui::End();
}

void StudioUI::draw_viewport_frame(Scene& scene,const StudioDocumentInfo& document,EntityId viewport_camera) {
    const ImGuiIO& io=ImGui::GetIO();const float menu=ImGui::GetFrameHeight();const ImVec2 min{LeftPanelWidth,menu};const ImVec2 max{std::max(LeftPanelWidth+20.0f,io.DisplaySize.x-RightPanelWidth),std::max(menu+20.0f,io.DisplaySize.y-BottomPanelHeight)};ImDrawList*dl=ImGui::GetForegroundDrawList();dl->AddRect(min,max,document.playing?IM_COL32(70,190,105,255):IM_COL32(44,72,55,255),0,0,document.playing?2.0f:1.0f);draw_viewport_toolbar(document);
    Mat4 view{},projection{},vp{};const bool camera=build_camera_matrices(scene,viewport_camera,view,projection,vp);
    if(!document.playing&&camera&&m_selected_entity!=InvalidEntity&&scene.is_alive(m_selected_entity)&&scene.has_component<TransformComponent>(m_selected_entity)&&!editor_only_entity(scene,m_selected_entity)){
        Mat4 world=world_transform_matrix(scene,m_selected_entity);ImGuizmo::SetOrthographic(false);ImGuizmo::SetDrawlist(dl);ImGuizmo::SetRect(0,0,io.DisplaySize.x,io.DisplaySize.y);
        if(m_gizmo_tool!=GizmoTool::Select){ImGuizmo::OPERATION op=ImGuizmo::TRANSLATE;if(m_gizmo_tool==GizmoTool::Rotate)op=ImGuizmo::ROTATE;if(m_gizmo_tool==GizmoTool::Scale)op=ImGuizmo::SCALE;float snap[3]={m_move_snap,m_move_snap,m_move_snap};if(m_gizmo_tool==GizmoTool::Rotate)snap[0]=snap[1]=snap[2]=m_rotate_snap;if(m_gizmo_tool==GizmoTool::Scale)snap[0]=snap[1]=snap[2]=m_scale_snap;ImGuizmo::Manipulate(view.data(),projection.data(),op,m_local_space?ImGuizmo::LOCAL:ImGuizmo::WORLD,world.data(),nullptr,m_snap_enabled?snap:nullptr);if(ImGuizmo::IsUsing()){Mat4 local=world;const EntityId p=parent_entity(scene,m_selected_entity);if(p!=InvalidEntity){Mat4 inv{};if(inverse_matrix(world_transform_matrix(scene,p),inv))local=inv*world;}Vec3 pos{},rot{},scale{1,1,1};if(decompose_transform_matrix(local,pos,rot,scale)){auto*t=scene.get_component<TransformComponent>(m_selected_entity);t->position=pos;t->rotation_degrees=rot;t->scale=scale;m_scene_edited=true;}}}
        ImVec2 screen{};float depth=0;if(project_to_screen(vp,matrix_position(world_transform_matrix(scene,m_selected_entity)),screen,depth))dl->AddCircle(screen,18,IM_COL32(86,235,130,230),24,2);
    }
    const bool inside=io.MousePos.x>=min.x&&io.MousePos.x<=max.x&&io.MousePos.y>=min.y&&io.MousePos.y<=max.y;if(!document.playing&&camera&&inside&&!io.WantCaptureMouse&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&!ImGuizmo::IsOver()&&!ImGuizmo::IsUsing()){EntityId best=InvalidEntity;float score=1e9f;scene.for_each<TransformComponent,MeshComponent>([&](EntityId e,TransformComponent&,MeshComponent&m){if(!m.visible)return;ImVec2 s{};float d=0;if(!project_to_screen(vp,matrix_position(world_transform_matrix(scene,e)),s,d))return;const float dx=s.x-io.MousePos.x,dy=s.y-io.MousePos.y,dist=std::sqrt(dx*dx+dy*dy);if(dist>42)return;const float candidate=dist+(d+1)*3;if(candidate<score){score=candidate;best=e;}});m_selected_entity=best;}
    const char*controls=document.playing?"RMB Look   WASD Walk   Space Jump   Shift Sprint   E Interact   F5 Stop":"Click Select   Q Select   W Move   E Rotate   R Scale   F Focus   RMB+WASD Navigate";const ImVec2 ts=ImGui::CalcTextSize(controls);dl->AddText({min.x+(max.x-min.x-ts.x)*0.5f,max.y-ts.y-10},IM_COL32(160,188,170,220),controls);
    if(document.playing&&!document.gameplay_prompt.empty()){const ImVec2 ps=ImGui::CalcTextSize(document.gameplay_prompt.c_str());const ImVec2 pmin{min.x+(max.x-min.x-ps.x)*0.5f-14,max.y-88};const ImVec2 pmax{pmin.x+ps.x+28,pmin.y+ps.y+16};dl->AddRectFilled(pmin,pmax,IM_COL32(4,9,6,220),5);dl->AddText({pmin.x+14,pmin.y+8},IM_COL32(230,245,234,255),document.gameplay_prompt.c_str());}
}

StudioUI::HistoryEntry StudioUI::capture_history(const Scene& scene) const {
    HistoryEntry entry;entry.scene=scene;if(m_selected_entity!=InvalidEntity&&scene.is_alive(m_selected_entity))entry.selected_persistent_id=scene.entity_persistent_id(m_selected_entity);return entry;
}

void StudioUI::restore_history(Scene& scene,const HistoryEntry& entry) {
    scene=entry.scene;m_selected_entity=entry.selected_persistent_id.empty()?InvalidEntity:scene.find_entity_by_persistent_id(entry.selected_persistent_id);m_history_baseline=capture_history(scene);m_has_history_baseline=true;m_history_structural_signature=structural_signature(scene);m_history_transaction_active=false;m_scene_edited=true;
}

std::string StudioUI::structural_signature(const Scene& scene) const {
    std::vector<std::string> parts;parts.reserve(scene.entity_count());scene.for_each_entity([&](EntityId e,const std::string&){std::string part=scene.entity_persistent_id(e);if(const auto*p=scene.get_component<ParentComponent>(e))part+="@"+p->parent_persistent_id;parts.push_back(std::move(part));});std::sort(parts.begin(),parts.end());std::ostringstream stream;for(const auto&p:parts)stream<<p<<';';return stream.str();
}

void StudioUI::begin_history_frame(Scene& scene,const StudioDocumentInfo& document) {
    const std::string key=document.project_name+"|"+document.scene_name+"|"+(document.has_project?"1":"0");
    if(!m_has_history_baseline||key!=m_history_document_key){m_undo_stack.clear();m_redo_stack.clear();m_history_baseline=capture_history(scene);m_has_history_baseline=true;m_history_document_key=key;m_history_structural_signature=structural_signature(scene);m_history_transaction_active=false;return;}
    const std::string current=structural_signature(scene);if(current!=m_history_structural_signature&&!m_history_transaction_active){m_undo_stack.push_back(m_history_baseline);if(m_undo_stack.size()>HistoryLimit)m_undo_stack.erase(m_undo_stack.begin());m_redo_stack.clear();m_history_baseline=capture_history(scene);m_history_structural_signature=current;}
}

void StudioUI::finish_history_frame(Scene& scene) {
    const bool active=scene_edit_active();
    if(m_scene_edited){if(!m_history_transaction_active){if(m_has_history_baseline){m_undo_stack.push_back(m_history_baseline);if(m_undo_stack.size()>HistoryLimit)m_undo_stack.erase(m_undo_stack.begin());}m_redo_stack.clear();m_history_transaction_active=active;if(!active){m_history_baseline=capture_history(scene);m_history_structural_signature=structural_signature(scene);}}else if(!active){m_history_transaction_active=false;m_history_baseline=capture_history(scene);m_history_structural_signature=structural_signature(scene);}}
    else if(m_history_transaction_active&&!active){m_history_transaction_active=false;m_history_baseline=capture_history(scene);m_history_structural_signature=structural_signature(scene);}
}

void StudioUI::undo(Scene& scene) {
    if(m_undo_stack.empty())return;m_redo_stack.push_back(capture_history(scene));HistoryEntry entry=std::move(m_undo_stack.back());m_undo_stack.pop_back();restore_history(scene,entry);
}

void StudioUI::redo(Scene& scene) {
    if(m_redo_stack.empty())return;m_undo_stack.push_back(capture_history(scene));HistoryEntry entry=std::move(m_redo_stack.back());m_redo_stack.pop_back();restore_history(scene,entry);
}

void StudioUI::focus_selected(Scene& scene,EntityId viewport_camera) {
    if(m_selected_entity==InvalidEntity||viewport_camera==InvalidEntity||!scene.is_alive(m_selected_entity)||!scene.is_alive(viewport_camera))return;auto*camera=scene.get_component<TransformComponent>(viewport_camera);if(!camera)return;const TransformComponent target=world_transform(scene,m_selected_entity);const TransformComponent camera_world=world_transform(scene,viewport_camera);const Vec3 forward=forward_from_euler(camera_world.rotation_degrees);const float scale=std::max({target.scale.x,target.scale.y,target.scale.z,1.0f});camera->position=target.position-forward*(4.0f*scale+1.0f);
}

} // namespace seed::studio
