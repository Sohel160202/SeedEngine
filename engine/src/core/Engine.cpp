#include "seed/core/Engine.h"

#include <iostream>
#include <utility>

namespace seed {

Engine::Engine(EngineConfig config)
    : m_config(std::move(config)) {
}

void Engine::start() {
    if (m_running) {
        return;
    }

    m_running = true;
    std::cout << "[Seed] Starting " << m_config.application_name << '\n';
}

void Engine::shutdown() {
    if (!m_running) {
        return;
    }

    std::cout << "[Seed] Shutting down " << m_config.application_name << '\n';
    m_running = false;
}

} // namespace seed
