#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/interfaces.h"
#include "kera/result.h"
#include <memory>

namespace kera {

class Window;

Result<std::shared_ptr<IRenderer>> CreateRenderer(GraphicsBackend backend, Window& window);

} // namespace kera
