#include "seed/gameplay/AudioSystem.h"

#include "miniaudio.h"

#include <algorithm>
#include <utility>

namespace seed {

struct MiniaudioOutput::Impl {
    ma_engine engine{};
    bool initialized{false};
};

MiniaudioOutput::MiniaudioOutput()
    : m_impl(std::make_unique<Impl>()) {}

MiniaudioOutput::~MiniaudioOutput() {
    shutdown();
}

bool MiniaudioOutput::initialize() {
    if (m_impl->initialized) {
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    if (ma_engine_init(&config, &m_impl->engine) != MA_SUCCESS) {
        return false;
    }

    m_impl->initialized = true;
    return true;
}

void MiniaudioOutput::shutdown() noexcept {
    if (!m_impl || !m_impl->initialized) {
        return;
    }

    ma_engine_uninit(&m_impl->engine);
    m_impl->initialized = false;
}

bool MiniaudioOutput::initialized() const noexcept {
    return m_impl != nullptr && m_impl->initialized;
}

void MiniaudioOutput::play(const AudioPlaybackRequest& request) {
    if (!initialized() || request.asset.empty()) {
        return;
    }

    ma_engine_play_sound(&m_impl->engine, request.asset.c_str(), nullptr);
}

void AudioSystem::consume_feedback(
    GameplayFeedbackBus& feedback,
    AudioOutput& output
) {
    const auto requests = feedback.drain_type(GameplayFeedbackType::Audio);
    for (const auto& request : requests) {
        if (request.audio_asset.empty()) {
            continue;
        }

        output.play({
            request.source_entity,
            request.audio_asset,
            std::max(0.0f, request.volume)
        });
    }
}

} // namespace seed
