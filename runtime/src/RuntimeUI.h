#pragma once

#include "seed/gameplay/GameplayHUD.h"

namespace seed::runtime {

class RuntimeUI {
public:
    bool initialize(void* window_handle);
    void shutdown();
    void begin_frame();
    void draw(const GameplayHUDState& state, bool save_enabled);
    void render();

private:
    bool m_initialized{false};
};

} // namespace seed::runtime
