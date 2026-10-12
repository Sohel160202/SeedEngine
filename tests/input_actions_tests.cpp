#include "seed/input/InputActions.h"

#include <algorithm>
#include <cassert>
#include <iostream>

int main() {
    seed::InputActionMap map;

    assert(map.action_for(seed::KeyCode::W) == seed::InputAction::MoveForward);
    const auto space_actions = map.actions_for(seed::KeyCode::Space);
    assert(std::find(
        space_actions.begin(),
        space_actions.end(),
        seed::InputAction::MoveUp
    ) != space_actions.end());
    assert(std::find(
        space_actions.begin(),
        space_actions.end(),
        seed::InputAction::Jump
    ) != space_actions.end());

    assert(map.bind(seed::InputAction::MoveForward, seed::KeyCode::I));
    assert(map.action_for(seed::KeyCode::I) == seed::InputAction::MoveForward);
    assert(map.bind(seed::InputAction::MoveBackward, seed::KeyCode::K));
    assert(map.action_for(seed::KeyCode::K) == seed::InputAction::MoveBackward);

    assert(map.unbind(seed::KeyCode::I));
    assert(map.action_for(seed::KeyCode::I) == seed::InputAction::None);
    assert(!map.bind(seed::InputAction::None, seed::KeyCode::P));
    assert(!map.bind(seed::InputAction::Interact, seed::KeyCode::Unknown));

    std::cout << "SeedInputActionsTests passed.\n";
    return 0;
}
