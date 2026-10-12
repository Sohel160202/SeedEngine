#pragma once

#include "seed/core/Types.h"
#include "seed/gameplay/GameplayFeedback.h"

#include <memory>
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

class MiniaudioOutput final : public AudioOutput {
public:
    MiniaudioOutput();
    ~MiniaudioOutput() override;

    MiniaudioOutput(const MiniaudioOutput&) = delete;
    MiniaudioOutput& operator=(const MiniaudioOutput&) = delete;

    bool initialize();
    void shutdown() noexcept;
    bool initialized() const noexcept;

    void play(const AudioPlaybackRequest& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

class AudioSystem {
public:
    static void consume_feedback(
        GameplayFeedbackBus& feedback,
        AudioOutput& output
    );
};

} // namespace seed
