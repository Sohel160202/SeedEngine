#pragma once

#include "seed/core/Types.h"

#include <algorithm>
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
    std::string audio_asset{};
    std::string visual_asset{};
    float volume{1.0f};
    float camera_shake{0.0f};
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

    std::vector<GameplayFeedbackRequest> drain_type(GameplayFeedbackType type) {
        std::vector<GameplayFeedbackRequest> selected;
        std::vector<GameplayFeedbackRequest> remaining;
        selected.reserve(m_pending.size());
        remaining.reserve(m_pending.size());

        for (auto& request : m_pending) {
            if (request.type == type) {
                selected.push_back(std::move(request));
            } else {
                remaining.push_back(std::move(request));
            }
        }

        m_pending = std::move(remaining);
        return selected;
    }

    void clear() noexcept {
        m_pending.clear();
    }

private:
    std::vector<GameplayFeedbackRequest> m_pending;
};

} // namespace seed
