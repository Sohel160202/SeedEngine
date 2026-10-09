#pragma once

#include "seed/core/Types.h"
#include "seed/scene/Scene.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace seed::studio {

enum class StudioActionType {
    None,
    NewProject,
    OpenProject,
    Save,
    SaveAs,
    NewScene,
    ImportModel,
    CreateEmptyEntity,
    CreateCube,
    CreateDirectionalLight,
    CreateAmbientLight,
    CreateSky,
    CreateFirstPersonPlayer,
    DuplicateSelected,
    DeleteSelected,
    FocusSelected,
    Undo,
    Redo,
    Play,
    Stop
};

struct StudioAction {
    StudioActionType type{StudioActionType::None};
    std::string path;
    std::string project_name;
};

struct StudioDocumentInfo {
    std::string project_name{"Untitled"};
    std::string scene_name{"Main.seedscene"};
    std::string status_message;
    std::string gameplay_prompt;
    std::string gameplay_status;
    std::vector<std::pair<std::string, int>> gameplay_inventory;
    std::vector<std::string> project_assets;
    bool dirty{true};
    bool has_project{false};
    bool playing{false};
    bool can_undo{false};
    bool can_redo{false};
};

enum class GizmoTool { Select, Move, Rotate, Scale };

class StudioUI {
public:
    bool initialize(void* window_handle);
    void shutdown();
    void begin_frame();
    void draw(Scene& scene, const StudioDocumentInfo& document, EntityId viewport_camera = InvalidEntity);
    void render();

    void select_entity(EntityId entity) noexcept { m_selected_entity = entity; }
    EntityId selected_entity() const noexcept { return m_selected_entity; }

    StudioAction take_action();
    bool consume_scene_edited() noexcept;
    bool scene_edit_active() const noexcept;
    bool wants_mouse() const noexcept;
    bool wants_keyboard() const noexcept;
    ~StudioUI();

private:
    struct HistoryEntry {
        Scene scene;
        std::string selected_persistent_id;
    };

    void queue_action(StudioAction action);
    void draw_main_menu(const StudioDocumentInfo& document);
    void draw_project_dialogs();
    void draw_world_panel(Scene& scene);
    void draw_inspector(Scene& scene, const StudioDocumentInfo& document);
    void draw_assets_panel(const StudioDocumentInfo& document);
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

    std::array<char, 128> m_project_name_buffer{};
    std::array<char, 512> m_project_folder_buffer{};
    std::array<char, 512> m_project_file_buffer{};
    std::array<char, 512> m_import_model_file_buffer{};
};

} // namespace seed::studio
