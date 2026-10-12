#pragma once

#include "seed/input/InputCodes.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace seed {

enum class InputAction {
    None,
    MoveForward,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveDown,
    MoveUp,
    Sprint,
    Interact,
    Jump,
    DialogueChoice1,
    DialogueChoice2,
    DialogueChoice3
};

class InputActionMap {
public:
    InputActionMap() {
        reset_defaults();
    }

    void reset_defaults() {
        m_bindings.clear();
        bind(InputAction::MoveForward, KeyCode::W);
        bind(InputAction::MoveBackward, KeyCode::S);
        bind(InputAction::MoveLeft, KeyCode::A);
        bind(InputAction::MoveRight, KeyCode::D);
        bind(InputAction::MoveDown, KeyCode::Q);
        bind(InputAction::MoveUp, KeyCode::Space);
        bind(InputAction::Sprint, KeyCode::LeftShift);
        bind(InputAction::Sprint, KeyCode::RightShift);
        bind(InputAction::Interact, KeyCode::E);
        bind(InputAction::Jump, KeyCode::Space);
        bind(InputAction::DialogueChoice1, KeyCode::Num1);
        bind(InputAction::DialogueChoice2, KeyCode::Num2);
        bind(InputAction::DialogueChoice3, KeyCode::Num3);
    }

    bool bind(InputAction action, KeyCode key) {
        if (action == InputAction::None || key == KeyCode::Unknown) {
            return false;
        }

        m_bindings.erase(
            std::remove_if(
                m_bindings.begin(),
                m_bindings.end(),
                [&](const Binding& binding) {
                    return binding.action == action && binding.key == key;
                }
            ),
            m_bindings.end()
        );
        m_bindings.push_back({.action = action, .key = key});
        return true;
    }

    bool unbind(KeyCode key) {
        const auto old_size = m_bindings.size();
        m_bindings.erase(
            std::remove_if(
                m_bindings.begin(),
                m_bindings.end(),
                [&](const Binding& binding) { return binding.key == key; }
            ),
            m_bindings.end()
        );
        return m_bindings.size() != old_size;
    }

    std::vector<InputAction> actions_for(KeyCode key) const {
        std::vector<InputAction> actions;
        for (const auto& binding : m_bindings) {
            if (binding.key == key) {
                actions.push_back(binding.action);
            }
        }
        return actions;
    }

    InputAction action_for(KeyCode key) const noexcept {
        const auto it = std::find_if(
            m_bindings.rbegin(),
            m_bindings.rend(),
            [&](const Binding& binding) { return binding.key == key; }
        );
        return it == m_bindings.rend() ? InputAction::None : it->action;
    }

private:
    struct Binding {
        InputAction action{InputAction::None};
        KeyCode key{KeyCode::Unknown};
    };

    std::vector<Binding> m_bindings;
};

} // namespace seed
