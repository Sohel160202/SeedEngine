#pragma once

#include "seed/core/Types.h"

namespace seed {

class IRenderer;
class Scene;

class RenderSystem {
public:
    static bool render(
        Scene& scene,
        IRenderer& renderer,
        EntityId camera_entity = InvalidEntity
    );
};

} // namespace seed
