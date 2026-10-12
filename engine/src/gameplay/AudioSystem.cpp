#include "seed/gameplay/AudioSystem.h"

#include <algorithm>
#include <utility>

namespace seed {

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
