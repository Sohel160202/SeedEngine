#include "seed/gameplay/Components.h"
#include "seed/gameplay/GameplayHUD.h"
#include "seed/gameplay/GameplayRuntime.h"
#include "seed/scene/Scene.h"

#include <cassert>
#include <iostream>

int main() {
    seed::Scene scene;
    const auto player = scene.create_entity("Player", "player");

    seed::HealthComponent health;
    health.maximum = 125.0f;
    health.current = 80.0f;
    scene.add_component<seed::HealthComponent>(player, health);

    seed::InventoryComponent inventory;
    inventory.items["CabinKey"] = 1;
    scene.add_component<seed::InventoryComponent>(player, inventory);

    seed::QuestComponent quest;
    quest.title = "Open the Cabin";
    quest.required_item = "CabinKey";
    quest.required_quantity = 1;
    quest.status = seed::QuestStatus::Active;
    scene.add_component<seed::QuestComponent>(player, quest);

    seed::GameplayRuntime runtime;
    assert(runtime.start(scene, player));

    const auto hud = seed::GameplayHUDSystem::capture(scene, runtime);
    assert(hud.active);
    assert(hud.health_visible);
    assert(hud.health_current == 80.0f);
    assert(hud.health_maximum == 125.0f);
    assert(hud.inventory.at("CabinKey") == 1);
    assert(hud.active_objectives.size() == 1);
    assert(hud.active_objectives.front() == "Open the Cabin (CabinKey 1)");
    assert(!hud.dialogue_visible);

    runtime.stop();
    assert(!seed::GameplayHUDSystem::capture(scene, runtime).active);

    std::cout << "SeedGameplayHUDTests passed.\n";
    return 0;
}
