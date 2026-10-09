#pragma once

#include "seed/platform/IPlatform.h"

#include <memory>

namespace seed {

std::unique_ptr<IPlatform> create_platform();

} // namespace seed
