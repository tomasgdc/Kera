#pragma once

#include <memory>

#include "kera/renderer/descriptors.h"
#include "kera/renderer/interfaces.h"

namespace kera {

class Window;

std::unique_ptr<IRenderer> CreateRenderer(GraphicsBackend backend,
                                          Window& window);

}  // namespace kera
