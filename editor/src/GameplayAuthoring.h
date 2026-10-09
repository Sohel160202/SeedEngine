#pragma once

#include "seed/core/Types.h"

namespace seed {
class Scene;
}

namespace seed::studio {

// Draws any beginner-facing gameplay components already attached to an entity.
// Returns true when scene data changed.
bool draw_gameplay_components(Scene& scene, EntityId entity);

// Draws the contents of the Add Gameplay popup. Components are attached directly
// to Seed Scene data. Returns true when a component was added.
bool draw_add_gameplay_popup(Scene& scene, EntityId entity);

} // namespace seed::studio
