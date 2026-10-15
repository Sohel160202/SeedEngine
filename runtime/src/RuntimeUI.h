#pragma once

#include "seed/gameplay/GameplayHUD.h"

namespace seed::runtime {

struct RuntimeUIAction {
    bool toggle_pause{false};
    bool save_requested{false};
    bool load_requested{false};
};

class RuntimeUI {
public:
    bool initialize(void* window_handle);
    void shutdown();
    void begin_frame();
    RuntimeUIAction draw(const GameplayHUDState& state, bool save_enabled, bool paused);
    void render();

private:
    bool m_initialized{false};
};

} // namespace seed::runtime
