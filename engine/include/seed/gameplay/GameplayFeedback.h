#pragma once

#include "seed/core/Types.h"

#include <string>
#include <utility>
#include <vector>

namespace seed {

enum class GameplayFeedbackType {
    Audio,
    Visual
};

struct GameplayFeedbackRequest {
    EntityId source_entity{InvalidEntity};
    GameplayFeedbackType type{GameplayFeedbackType::Audio};
    std::string cue;
    float time_seconds{0.0f};
};

// Frame-scoped semantic feedback requests. This layer deliberately has no
// dependency on audio files, particle assets, or a renderer.
class GameplayFeedbackBus {
public:
    void publish(GameplayFeedbackRequest request) {
        m_pending.push_back(std::move(request));
    }

    const std::vector<GameplayFeedbackRequest>& pending() const noexcept {
        return m_pending;
    }

    std::vector<GameplayFeedbackRequest> drain() {
        std::vector<GameplayFeedbackRequest> requests = std::move(m_pending);
        m_pending.clear();
        return requests;
    }

    void clear() noexcept {
        m_pending.clear();
    }

private:
    std::vector<GameplayFeedbackRequest> m_pending;
};

} // namespace seed
