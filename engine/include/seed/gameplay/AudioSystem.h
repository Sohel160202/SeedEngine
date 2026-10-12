#pragma once

#include "seed/core/Types.h"
#include "seed/gameplay/GameplayFeedback.h"

#include <string>

namespace seed {

struct AudioPlaybackRequest {
    EntityId source_entity{InvalidEntity};
    std::string asset;
    float volume{1.0f};
};

class AudioOutput {
public:
    virtual ~AudioOutput() = default;
    virtual void play(const AudioPlaybackRequest& request) = 0;
};

// Safe default output for headless/editor/runtime builds without an audio device.
class SilentAudioOutput final : public AudioOutput {
public:
    void play(const AudioPlaybackRequest&) override {}
};

class AudioSystem {
public:
    static void consume_feedback(
        GameplayFeedbackBus& feedback,
        AudioOutput& output
    );
};

} // namespace seed
