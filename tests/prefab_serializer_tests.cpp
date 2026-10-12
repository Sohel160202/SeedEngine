#include "seed/gameplay/Components.h"
#include "seed/project/PrefabSerializer.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <filesystem>
#include <string>

int main() {
    seed::Scene source;
    const auto enemy = source.create_entity("Enemy");
    auto& transform = source.add_component<seed::TransformComponent>(enemy);
    transform.position = {2.0f, 1.0f, -3.0f};
    auto& health = source.add_component<seed::HealthComponent>(enemy);
    health.maximum = 80.0f;
    health.current = 55.0f;
    auto& rules = source.add_component<seed::GameplayRulesComponent>(enemy);
    rules.rules.push_back(seed::GameplayRule{
        .when_event = "Death",
        .condition = "Always",
        .action = "Complete Game",
        .action_value = "Victory",
    });

    const auto prefab_path =
        std::filesystem::temp_directory_path() / "seed_prefab_serializer_test.seedprefab";
    std::string error;
    assert(seed::PrefabSerializer::save(source, enemy, prefab_path, &error));
    seed::Scene destination;
    seed::EntityId instance = seed::InvalidEntity;
    assert(seed::PrefabSerializer::instantiate(destination, prefab_path, &instance, &error));
    assert(instance != seed::InvalidEntity);
    assert(destination.entity_name(instance) == "Enemy");
    assert(destination.entity_persistent_id(instance) != source.entity_persistent_id(enemy));
    assert(destination.get_component<seed::TransformComponent>(instance)->position.x == 2.0f);
    assert(destination.get_component<seed::HealthComponent>(instance)->current == 55.0f);
    assert(destination.get_component<seed::GameplayRulesComponent>(instance)->rules.size() == 1);
    assert(destination.get_component<seed::ParentComponent>(instance) == nullptr);

    std::filesystem::remove(prefab_path);
    return 0;
}
