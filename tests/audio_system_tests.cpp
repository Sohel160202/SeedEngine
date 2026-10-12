#include "seed/gameplay/AudioSystem.h"

#include <cassert>
#include <iostream>
#include <vector>

namespace {

class RecordingAudioOutput final : public seed::AudioOutput {
public:
    void play(const seed::AudioPlaybackRequest& request) override {
        requests.push_back(request);
    }

    std::vector<seed::AudioPlaybackRequest> requests;
};

} // namespace

int main() {
    seed::GameplayFeedbackBus feedback;
    feedback.publish({
        1,
        seed::GameplayFeedbackType::Audio,
        "Footstep",
        0.25f,
        "Audio/Footsteps/Stone.wav",
        "",
        0.65f,
        0.0f
    });
    feedback.publish({
        2,
        seed::GameplayFeedbackType::Visual,
        "DoorOpened",
        0.5f,
        "",
        "VFX/DoorGlow",
        1.0f,
        0.3f
    });
    feedback.publish({
        3,
        seed::GameplayFeedbackType::Audio,
        "PickupCollected",
        0.75f,
        "",
        "",
        1.0f,
        0.0f
    });

    RecordingAudioOutput output;
    seed::AudioSystem::consume_feedback(feedback, output);

    assert(output.requests.size() == 1);
    assert(output.requests[0].source_entity == 1);
    assert(output.requests[0].asset == "Audio/Footsteps/Stone.wav");
    assert(output.requests[0].volume == 0.65f);

    assert(feedback.pending().size() == 1);
    assert(feedback.pending()[0].type == seed::GameplayFeedbackType::Visual);

    std::cout << "SeedAudioSystemTests passed.\n";
    return 0;
}
