#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayEvents.h"
#include "seed/gameplay/GameplayFeedback.h"
#include "seed/gameplay/GameplayFeedbackSystem.h"
#include "seed/project/SceneSerializer.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_feedback_cue_persistence_test";
    const auto scene_file = root / "Feedback.seedscene";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    seed::Scene source;
    const auto entity = source.create_entity("Hero", "hero-1");
    source.add_component<seed::TransformComponent>(entity);

    seed::FeedbackCueComponent cues;
    cues.bindings.push_back({
        "Footstep",
        "Audio/Footsteps/Stone.wav",
        "",
        0.65f,
        0.0f
    });
    cues.bindings.push_back({
        "DoorOpened",
        "",
        "VFX/DoorGlow",
        1.0f,
        0.35f
    });
    source.add_component<seed::FeedbackCueComponent>(entity, cues);

    std::string error;
    assert(seed::SceneSerializer::save(source, scene_file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, scene_file, &error));
    const auto loaded_entity = loaded.find_entity_by_persistent_id("hero-1");
    assert(loaded_entity != seed::InvalidEntity);

    const auto* loaded_cues = loaded.get_component<seed::FeedbackCueComponent>(loaded_entity);
    assert(loaded_cues != nullptr);
    assert(loaded_cues->bindings.size() == 2);
    assert(loaded_cues->bindings[0].audio_asset == "Audio/Footsteps/Stone.wav");
    assert(loaded_cues->bindings[0].volume == 0.65f);
    assert(loaded_cues->bindings[1].visual_asset == "VFX/DoorGlow");
    assert(loaded_cues->bindings[1].camera_shake == 0.35f);

    seed::GameplayEventBus events;
    events.publish({
        loaded_entity,
        seed::GameplayEventType::Footstep,
        "Footstep",
        2.0f
    });
    seed::GameplayFeedbackBus feedback;
    seed::GameplayFeedbackSystem::consume_events(events, feedback, &loaded);

    assert(feedback.pending().size() == 1);
    assert(feedback.pending()[0].audio_asset == "Audio/Footsteps/Stone.wav");
    assert(feedback.pending()[0].volume == 0.65f);

    std::filesystem::remove_all(root);
    std::cout << "SeedFeedbackCuePersistenceTests passed.\n";
    return 0;
}
