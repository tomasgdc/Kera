// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/renderer/factory.h"

#include "kera/renderer/backend/vulkan/vulkan_renderer.h"
#include "kera/utilities/logger.h"

namespace kera
{

    std::unique_ptr<IRenderer> createRenderer(EGraphicsBackend backend, Window& window)
    {
        switch (backend)
        {
            case EGraphicsBackend::VULKAN:
            {
                auto renderer = std::make_unique<VulkanRenderer>();
                if (!renderer->initialize(window))
                {
                    Logger::getInstance().error("Failed to initialize Vulkan renderer.");
                    return nullptr;
                }
                return renderer;
            }
            default:
                Logger::getInstance().error("Requested graphics backend is not supported.");
                return nullptr;
        }
    }

}  // namespace kera
