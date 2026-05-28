// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/descriptors.h"
#include "kera/renderer/interfaces.h"

#include <memory>

namespace kera
{

    class Window;

    std::unique_ptr<IRenderer> CreateRenderer(GraphicsBackend backend, Window& window);

}  // namespace kera
