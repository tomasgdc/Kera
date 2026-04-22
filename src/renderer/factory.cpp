#include "kera/renderer/factory.h"

#include "kera/renderer/vulkan_renderer.h"

namespace kera {

Result<std::shared_ptr<IRenderer>> CreateRenderer(GraphicsBackend backend, Window& window) {
    switch (backend) {
        case GraphicsBackend::Vulkan: {
            auto renderer = std::make_shared<VulkanRenderer>();
            if (!renderer->initialize(window)) {
                return failure<std::shared_ptr<IRenderer>>("Failed to initialize Vulkan renderer.");
            }
            return success<std::shared_ptr<IRenderer>>(std::move(renderer));
        }
        default:
            return failure<std::shared_ptr<IRenderer>>("Requested graphics backend is not supported.");
    }
}

} // namespace kera
