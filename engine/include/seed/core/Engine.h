#pragma once

#include "seed/scene/Scene.h"

#include <string>

namespace seed {

struct EngineConfig {
    std::string application_name{"Seed Application"};
};

class Engine {
public:
    explicit Engine(EngineConfig config = {});

    void start();
    void shutdown();

    bool is_running() const noexcept { return m_running; }
    const EngineConfig& config() const noexcept { return m_config; }

    Scene& scene() noexcept { return m_scene; }
    const Scene& scene() const noexcept { return m_scene; }

private:
    EngineConfig m_config;
    Scene m_scene;
    bool m_running{false};
};

} // namespace seed
