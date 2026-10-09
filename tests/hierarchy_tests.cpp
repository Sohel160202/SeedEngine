#include "seed/gameplay/Components.h"
#include "seed/project/SceneSerializer.h"
#include "seed/scene/Hierarchy.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace {
bool near(float a, float b, float epsilon = 0.001f) { return std::fabs(a - b) <= epsilon; }
}

int main() {
    seed::Scene scene;
    const auto parent = scene.create_entity("House");
    auto& parent_transform = scene.add_component<seed::TransformComponent>(parent);
    parent_transform.position = {10.0f, 0.0f, 0.0f};

    const auto child = scene.create_entity("Door");
    auto& child_transform = scene.add_component<seed::TransformComponent>(child);
    child_transform.position = {2.0f, 1.0f, 0.0f};

    assert(seed::set_parent(scene, child, parent, true));
    assert(seed::parent_entity(scene, child) == parent);
    auto world = seed::world_transform(scene, child);
    assert(near(world.position.x, 2.0f));
    assert(near(world.position.y, 1.0f));

    parent_transform.position.x = 20.0f;
    world = seed::world_transform(scene, child);
    assert(near(world.position.x, 12.0f));
    assert(!seed::can_parent(scene, parent, child));
    assert(!seed::set_parent(scene, parent, child, true));

    const auto root = std::filesystem::temp_directory_path() / "seed_hierarchy_test";
    const auto file = root / "Hierarchy.seedscene";
    std::filesystem::remove_all(root);
    std::string error;
    assert(seed::SceneSerializer::save(scene, file, &error));

    seed::Scene loaded;
    assert(seed::SceneSerializer::load(loaded, file, &error));
    const auto loaded_parent = loaded.find_entity_by_persistent_id(scene.entity_persistent_id(parent));
    const auto loaded_child = loaded.find_entity_by_persistent_id(scene.entity_persistent_id(child));
    assert(loaded_parent != seed::InvalidEntity);
    assert(loaded_child != seed::InvalidEntity);
    assert(seed::parent_entity(loaded, loaded_child) == loaded_parent);
    const auto loaded_world = seed::world_transform(loaded, loaded_child);
    assert(near(loaded_world.position.x, 12.0f));
    assert(near(loaded_world.position.y, 1.0f));

    assert(seed::clear_parent(loaded, loaded_child, true));
    assert(seed::parent_entity(loaded, loaded_child) == seed::InvalidEntity);
    const auto unparented_world = seed::world_transform(loaded, loaded_child);
    assert(near(unparented_world.position.x, 12.0f));
    assert(near(unparented_world.position.y, 1.0f));

    std::filesystem::remove_all(root);
    std::cout << "SeedHierarchyTests passed.\n";
    return 0;
}
