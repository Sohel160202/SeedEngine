#include "seed/gameplay/GameplayFeedbackSystem.h"

#include "seed/gameplay/Components.h"
#include "seed/scene/Scene.h"

#include <algorithm>
#include <utility>

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
    GameplayFeedbackBus& feedback,
    const Scene* scene
) {
    for (const GameplayEvent& event : events.pending()) {
        if (!is_supported_event(event)) {
            continue;
        }

        const FeedbackCueBinding* binding = nullptr;
        if (scene != nullptr && scene->is_alive(event.source_entity)) {
            if (const auto* cues = scene->get_component<FeedbackCueComponent>(event.source_entity)) {
                for (const auto& candidate : cues->bindings) {
                    if (candidate.event_name == event.name) {
                        binding = &candidate;
                        break;
                    }
                }
            }
        }

        GameplayFeedbackRequest request{
            event.source_entity,
            feedback_type_for(event.type),
            event.name,
            event.time_seconds
        };
        if (binding != nullptr) {
            request.audio_asset = binding->audio_asset;
            request.visual_asset = binding->visual_asset;
            request.volume = std::max(0.0f, binding->volume);
            request.camera_shake = std::max(0.0f, binding->camera_shake);
        }
        feedback.publish(std::move(request));
    }
}

} // namespace seed
