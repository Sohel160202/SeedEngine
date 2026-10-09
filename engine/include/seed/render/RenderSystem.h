#pragma once

namespace seed {

class IRenderer;
class Scene;

class RenderSystem {
public:
    static bool render(Scene& scene, IRenderer& renderer);
};

} // namespace seed
