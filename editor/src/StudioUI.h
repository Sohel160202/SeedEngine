#pragma once

#include "seed/core/Types.h"

namespace seed {
class Scene;
}

namespace seed::studio {

class StudioUI {
public:
    bool initialize(void* window_handle);
    void shutdown();

    void begin_frame();
    void draw(Scene& scene);
    void render();

    void select_entity(EntityId entity) noexcept { m_selected_entity = entity; }
    EntityId selected_entity() const noexcept { return m_selected_entity; }

    bool wants_mouse() const noexcept;
    bool wants_keyboard() const noexcept;

    ~StudioUI();

private:
    void draw_main_menu();
    void draw_world_panel(Scene& scene);
    void draw_inspector(Scene& scene);
    void draw_assets_panel();
    void draw_viewport_frame();

    EntityId m_selected_entity{InvalidEntity};
    bool m_initialized{false};
};

} // namespace seed::studio
