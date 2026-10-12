#include "seed/gameplay/GameplayEvents.h"
#include "seed/gameplay/GameplayFeedbackSystem.h"

#include <cassert>
#include <iostream>

int main() {
    seed::GameplayEventBus events;
    events.publish({
        7,
        seed::GameplayEventType::Footstep,
        "Footstep",
        1.25f
    });
    events.publish({
        9,
        seed::GameplayEventType::Interaction,
        "PickupCollected",
        1.5f
    });
    events.publish({
        11,
        seed::GameplayEventType::Animation,
        "AttackStart",
        1.75f
    });

    seed::GameplayFeedbackBus feedback;
    seed::GameplayFeedbackSystem::consume_events(events, feedback);

    assert(events.pending().size() == 3);
    assert(feedback.pending().size() == 2);
    assert(feedback.pending()[0].source_entity == 7);
    assert(feedback.pending()[0].type == seed::GameplayFeedbackType::Audio);
    assert(feedback.pending()[0].cue == "Footstep");
    assert(feedback.pending()[1].source_entity == 9);
    assert(feedback.pending()[1].type == seed::GameplayFeedbackType::Visual);
    assert(feedback.pending()[1].cue == "PickupCollected");

    const auto requests = feedback.drain();
    assert(requests.size() == 2);
    assert(feedback.pending().empty());

    std::cout << "SeedGameplayFeedbackTests passed.\n";
    return 0;
}
