#include "seed/gameplay/Components.h"
#include "seed/project/SceneRevision.h"

#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    const auto root = std::filesystem::temp_directory_path() / "seed_scene_revision_tests";
    std::error_code cleanup_error;
    std::filesystem::remove_all(root, cleanup_error);

    seed::Scene scene;
    const auto player = scene.create_entity("Player", "player");
    auto& transform = scene.add_component<seed::TransformComponent>(player);
    transform.position.x = 3.0f;

    seed::SceneRevisionStore store(root);
    const auto first = store.create_revision(scene, "Initial Scene");
    assert(first.saved);
    assert(first.revision.id == "revision-000001");
    assert(first.revision.label == "Initial_Scene");

    transform.position.x = 8.0f;
    const auto second = store.create_revision(scene, "Moved Player");
    assert(second.saved);
    assert(second.revision.id == "revision-000002");

    const auto revisions = store.list();
    assert(revisions.size() == 2);
    assert(revisions.front().id == "revision-000002");

    seed::Scene restored;
    std::string error;
    assert(store.load_revision(restored, first.revision.id, &error));
    const auto restored_player = restored.find_entity_by_persistent_id("player");
    assert(restored_player != seed::InvalidEntity);
    assert(restored.get_component<seed::TransformComponent>(restored_player)->position.x == 3.0f);

    assert(!store.load_revision(restored, "../escape", &error));
    std::filesystem::remove_all(root, cleanup_error);

    std::cout << "Seed scene revision tests passed.\n";
    return 0;
}
