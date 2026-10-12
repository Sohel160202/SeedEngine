#include "seed/gameplay/GameplayFeedbackSystem.h"

namespace seed {
namespace {

GameplayFeedbackType feedback_type_for(GameplayEventType type) {
    return type == GameplayEventType::Interaction
        ? GameplayFeedbackType::Visual
        : GameplayFeedbackType::Audio;
}

bool is_supported_event(const GameplayEvent& event) {
    if (event.type == GameplayEventType::Footstep) {
        return event.name == "Footstep";
    }
    if (event.type == GameplayEventType::Interaction) {
        return event.name == "DialogueStarted" ||
            event.name == "PickupCollected" ||
            event.name == "DoorOpened" ||
            event.name == "DoorClosed" ||
            event.name == "Interact";
    }
    return false;
}

} // namespace

void GameplayFeedbackSystem::consume_events(
    const GameplayEventBus& events,
    GameplayFeedbackBus& feedback
) {
    for (const GameplayEvent& event : events.pending()) {
        if (!is_supported_event(event)) {
            continue;
        }

        feedback.publish({
            event.source_entity,
            feedback_type_for(event.type),
            event.name,
            event.time_seconds
        });
    }
}

} // namespace seed
