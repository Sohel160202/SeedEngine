#pragma once

#include "seed/render/IRenderer.h"

#include <memory>

namespace seed {

std::unique_ptr<IRenderer> create_renderer(RendererBackend backend);

} // namespace seed
