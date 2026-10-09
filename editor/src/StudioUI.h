#pragma once

#include "seed/core/Types.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace seed {
class Scene;
}

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
    CreateFirstPersonPlayer,
    DuplicateSelected,
    DeleteSelected,
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
};

class StudioUI {
public:
    bool initialize(void* window_handle);
    void shutdown();

    void begin_frame();
    void draw(Scene& scene, const StudioDocumentInfo& document);
    void render();

    void select_entity(EntityId entity) noexcept { m_selected_entity = entity; }
    EntityId selected_entity() const noexcept { return m_selected_entity; }

    StudioAction take_action();
    bool consume_scene_edited() noexcept;

    bool wants_mouse() const noexcept;
    bool wants_keyboard() const noexcept;

    ~StudioUI();

private:
    void queue_action(StudioAction action);
    void draw_main_menu(const StudioDocumentInfo& document);
    void draw_project_dialogs();
    void draw_world_panel(Scene& scene);
    void draw_inspector(Scene& scene, const StudioDocumentInfo& document);
    void draw_assets_panel(const StudioDocumentInfo& document);
    void draw_viewport_frame(const StudioDocumentInfo& document);

    EntityId m_selected_entity{InvalidEntity};
    StudioAction m_pending_action{};
    bool m_scene_edited{false};
    bool m_initialized{false};
    bool m_show_new_project_dialog{false};
    bool m_show_open_project_dialog{false};
    bool m_show_save_as_dialog{false};
    bool m_show_import_model_dialog{false};

    std::array<char, 128> m_project_name_buffer{};
    std::array<char, 512> m_project_folder_buffer{};
    std::array<char, 512> m_project_file_buffer{};
    std::array<char, 512> m_import_model_file_buffer{};
};

} // namespace seed::studio
