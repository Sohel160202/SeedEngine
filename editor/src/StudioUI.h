#pragma once

#include "seed/core/Types.h"
#include "seed/project/SceneRevision.h"
#include "seed/scene/Scene.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace seed::studio {

enum class StudioActionType {
    None, NewProject, OpenProject, Save, SaveAs, NewScene, ImportModel,
    CreateEmptyEntity, CreateCube, CreateDirectionalLight, CreateAmbientLight,
    CreateSky, CreateFirstPersonPlayer, CreateThirdPersonPlayer, CreateBeginnerPreset, BuildGame, OpenExample, DuplicateSelected, DeleteSelected,
    FocusSelected, Undo, Redo, Play, Stop, OpenSemanticMerge, SaveRevision, LoadRevision,
    SavePrefab, InstantiatePrefab, RefreshLinkedPrefabs
};

struct StudioAction {
    StudioActionType type{StudioActionType::None};
    std::string path;
    std::string project_name;
    std::string path2;
    std::string path3;
};

struct StudioDocumentInfo {
    std::string project_name{"Untitled"};
    std::string scene_name{"Main.seedscene"};
    std::string status_message;
    std::string gameplay_prompt;
    std::string gameplay_status;
    bool gameplay_health_visible{false};
    float gameplay_health_current{0.0f};
    float gameplay_health_maximum{0.0f};
    bool gameplay_dialogue_visible{false};
    std::vector<std::string> gameplay_objectives;
    std::vector<std::pair<std::string, int>> gameplay_inventory;
    std::vector<std::string> project_assets;
    std::vector<std::string> project_prefabs;
    std::vector<std::string> project_asset_files;
    std::vector<SceneRevision> revisions;
    bool dirty{true};
    bool has_project{false};
    bool playing{false};
    bool can_undo{false};
    bool can_redo{false};
};

enum class GizmoTool { Select, Move, Rotate, Scale };

class StudioUI {
public:
    StudioUI();

    bool initialize(void* window_handle);
    void shutdown();
    void begin_frame();
    void draw(Scene& scene, const StudioDocumentInfo& document);
    void draw(Scene& scene, const StudioDocumentInfo& document, EntityId viewport_camera);
    void render();

    void select_entity(EntityId entity) noexcept { m_selected_entity = entity; }
    EntityId selected_entity() const noexcept { return m_selected_entity; }

    StudioAction take_action();
    bool consume_scene_edited() noexcept;
    bool scene_edit_active() const noexcept;
    bool wants_mouse() const noexcept;
    bool wants_keyboard() const noexcept;

    // Play Changes Review and future semantic merge tools can begin one external
    // authoring transaction without owning a second undo system. The pre-change
    // Studio baseline becomes one Undo entry and the normal history frame logic
    // adopts the post-change scene on the following frame.
    void begin_external_history_transaction();
    static StudioUI* active_instance() noexcept;

    ~StudioUI();

private:
    struct HistoryEntry { Scene scene; std::string selected_persistent_id; };

    void queue_action(StudioAction action);
    void draw_main_menu(const StudioDocumentInfo& document);
    void draw_project_dialogs(const StudioDocumentInfo& document);
    void draw_world_panel(Scene& scene);
    void draw_inspector(Scene& scene, const StudioDocumentInfo& document);
    void draw_assets_panel(Scene& scene, const StudioDocumentInfo& document);
    void draw_staff_panel(Scene& scene, const StudioDocumentInfo& document);
    void draw_viewport_frame(Scene& scene, const StudioDocumentInfo& document, EntityId viewport_camera);
    void draw_viewport_toolbar(const StudioDocumentInfo& document);
    void begin_history_frame(Scene& scene, const StudioDocumentInfo& document);
    void finish_history_frame(Scene& scene);
    void undo(Scene& scene);
    void redo(Scene& scene);
    void focus_selected(Scene& scene, EntityId viewport_camera);
    HistoryEntry capture_history(const Scene& scene) const;
    void restore_history(Scene& scene, const HistoryEntry& entry);
    std::string structural_signature(const Scene& scene) const;

    EntityId m_selected_entity{InvalidEntity};
    StudioAction m_pending_action{};
    bool m_scene_edited{false};
    bool m_initialized{false};
    bool m_show_new_project_dialog{false};
    bool m_show_open_project_dialog{false};
    bool m_show_save_as_dialog{false};
    bool m_show_import_model_dialog{false};
    bool m_show_prefab_dialog{false};
    bool m_prefab_dialog_is_save{true};
    bool m_prefab_dialog_is_refresh{false};
    bool m_show_semantic_merge_dialog{false};
    bool m_show_revision_dialog{false};
    bool m_show_beginner_guide_dialog{false};
    bool m_last_document_playing{false};

    GizmoTool m_gizmo_tool{GizmoTool::Move};
    bool m_local_space{false};
    bool m_snap_enabled{false};
    float m_move_snap{0.5f};
    float m_rotate_snap{15.0f};
    float m_scale_snap{0.1f};

    std::vector<HistoryEntry> m_undo_stack;
    std::vector<HistoryEntry> m_redo_stack;
    HistoryEntry m_history_baseline{};
    bool m_has_history_baseline{false};
    bool m_history_transaction_active{false};
    std::string m_history_document_key;
    std::string m_history_structural_signature;
    std::string m_asset_preview_path;

    std::array<char,128> m_project_name_buffer{};
    std::array<char,512> m_project_folder_buffer{};
    std::array<char,512> m_project_file_buffer{};
    std::array<char,512> m_import_model_file_buffer{};
    std::array<char,512> m_prefab_file_buffer{};
    std::array<char,512> m_merge_base_file_buffer{};
    std::array<char,512> m_merge_ours_file_buffer{};
    std::array<char,512> m_merge_theirs_file_buffer{};
    int m_project_template_index{1};
    int m_beginner_tutorial_step{0};
    int m_management_panel_tab{0};
    int m_merge_base_revision_index{-1};
    int m_merge_ours_revision_index{-1};
    int m_merge_theirs_revision_index{-1};
    std::array<char,192> m_revision_label_buffer{};
};

} // namespace seed::studio
