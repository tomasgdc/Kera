#include "samples.h"
#include "basic_triangle_sample.h"
#include "compute_sample.h"
#include "kera/renderer/instance.h"
#include "kera/renderer/device.h"
#include "kera/renderer/physical_device.h"
#include "kera/renderer/surface.h"
#include "kera/renderer/swapchain.h"
#include "kera/core/window.h"
#include "kera/utilities/logger.h"
#include <iostream>

namespace kera {

    // SampleApplication implementation
    SampleApplication::SampleApplication() : activeSampleIndex_(-1) {
        // Samples will be registered after renderer is initialized
    }

    SampleApplication::~SampleApplication() {
        if (activeSampleIndex_ >= 0 && activeSampleIndex_ < samples_.size()) {
            samples_[activeSampleIndex_]->cleanup();
        }
        cleanupRenderer();
    }

    void SampleApplication::addSample(std::unique_ptr<Sample> sample) {
        samples_.push_back(std::move(sample));
    }

    void SampleApplication::setActiveSample(int index) {
        if (index < 0 || index >= static_cast<int>(samples_.size())) {
            Logger::getInstance().error("Invalid sample index: " + std::to_string(index));
            return;
        }

        // Cleanup current sample
        if (activeSampleIndex_ >= 0 && activeSampleIndex_ < static_cast<int>(samples_.size())) {
            samples_[activeSampleIndex_]->cleanup();
        }

        activeSampleIndex_ = index;
        samples_[activeSampleIndex_]->initialize();
        Logger::getInstance().info("Switched to sample: " + samples_[activeSampleIndex_]->getName());
    }

    bool SampleApplication::initializeRenderer() {
        Logger::getInstance().info("Initializing Vulkan renderer");

        // Create window
        window_ = std::make_unique<Window>();
        if (!window_->initialize("Kera Triangle Sample", 1280, 720)) {
            Logger::getInstance().error("Failed to create window");
            return false;
        }
        Logger::getInstance().info("Window created successfully (1280x720)");

        // Create Vulkan instance
        instance_ = std::make_shared<Instance>();
        if (!instance_->initialize("Kera Sample", VK_MAKE_VERSION(0, 1, 0), true)) {
            Logger::getInstance().error("Failed to create Vulkan instance");
            return false;
        }
        Logger::getInstance().info("Vulkan instance created successfully");

        // Create surface from window
        surface_ = std::make_shared<Surface>();
        if (!surface_->create(instance_->getVulkanInstance(), *window_)) {
            Logger::getInstance().error("Failed to create Vulkan surface");
            return false;
        }
        Logger::getInstance().info("Vulkan surface created successfully");

        // Create physical device
        physicalDevice_ = std::make_shared<PhysicalDevice>();
        if (!physicalDevice_->initialize(instance_->getVulkanInstance(), surface_->getVulkanSurface())) {
            Logger::getInstance().error("Failed to select physical device");
            return false;
        }
        Logger::getInstance().info("Physical device selected successfully");

        // Create logical device
        device_ = std::make_shared<Device>();
        if (!device_->initialize(*physicalDevice_)) {
            Logger::getInstance().error("Failed to create logical device");
            return false;
        }
        Logger::getInstance().info("Logical device created successfully");

        // Create swapchain
        swapchain_ = std::make_shared<SwapChain>();
        if (!swapchain_->initialize(*physicalDevice_, *device_, surface_->getVulkanSurface(), 
                                    static_cast<uint32_t>(window_->getWidth()), 
                                    static_cast<uint32_t>(window_->getHeight()))) {
            Logger::getInstance().error("Failed to create swapchain");
            return false;
        }
        Logger::getInstance().info("Swapchain created successfully");

        Logger::getInstance().info("Vulkan renderer initialized successfully");
        return true;
    }

    bool SampleApplication::recreateSwapchainResources() {
        if (!window_ || !device_ || !surface_ || !instance_ || !physicalDevice_ || !swapchain_) {
            return false;
        }

        const int width = window_->getWidth();
        const int height = window_->getHeight();
        if (width <= 0 || height <= 0) {
            return false;
        }

        Logger::getInstance().info(
            "Recreating swapchain resources for window size " +
            std::to_string(width) + "x" + std::to_string(height));

        vkDeviceWaitIdle(device_->getVulkanDevice());

        if (activeSampleIndex_ >= 0 && activeSampleIndex_ < static_cast<int>(samples_.size())) {
            samples_[activeSampleIndex_]->cleanup();
        }

        swapchain_->shutdown();

        if (!physicalDevice_->initialize(instance_->getVulkanInstance(), surface_->getVulkanSurface())) {
            Logger::getInstance().error("Failed to refresh physical device swapchain support");
            return false;
        }

        if (!swapchain_->initialize(
                *physicalDevice_,
                *device_,
                surface_->getVulkanSurface(),
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height))) {
            Logger::getInstance().error("Failed to recreate swapchain");
            return false;
        }

        if (activeSampleIndex_ >= 0 && activeSampleIndex_ < static_cast<int>(samples_.size())) {
            samples_[activeSampleIndex_]->initialize();
        }

        window_->clearResizeFlag();
        Logger::getInstance().info("Swapchain resources recreated successfully");
        return true;
    }

    void SampleApplication::cleanupRenderer() {
        if (swapchain_) {
            swapchain_->shutdown();
        }
        if (surface_) {
            surface_->destroy();
        }
        if (device_) {
            device_->shutdown();
        }
        if (physicalDevice_) {
            // PhysicalDevice doesn't have shutdown - it's just a wrapper
        }
        if (instance_) {
            instance_->shutdown();
        }
        if (window_) {
            window_->shutdown();
        }
    }

    void SampleApplication::run() {
        Logger::getInstance().info("Starting Kera Sample Application");

        // Initialize renderer
        if (!initializeRenderer()) {
            Logger::getInstance().error("Failed to initialize renderer");
            return;
        }

        // Register available samples with renderer resources
        addSample(std::make_unique<BasicTriangleSample>(instance_, device_, surface_, swapchain_));
        // addSample(std::make_unique<ComputeSample>());

        Logger::getInstance().info("Available samples:");
        for (size_t i = 0; i < samples_.size(); ++i) {
            Logger::getInstance().info("  " + std::to_string(i) + ": " + samples_[i]->getName());
        }
        
        if (samples_.empty()) {
            Logger::getInstance().warning("No samples registered");
            return;
        }

        setActiveSample(0);
        Logger::getInstance().info("Running windowed render loop for sample 0");

        constexpr float deltaTime = 1.0f / 60.0f;
        while (!window_->shouldClose()) {
            window_->processEvents();
            if (activeSampleIndex_ < 0 || activeSampleIndex_ >= static_cast<int>(samples_.size())) {
                break;
            }

            if (window_->wasResized()) {
                if (window_->getWidth() == 0 || window_->getHeight() == 0) {
                    continue;
                }

                if (!recreateSwapchainResources()) {
                    Logger::getInstance().error("Failed to handle window resize");
                    break;
                }
            }

            samples_[activeSampleIndex_]->update(deltaTime);
            samples_[activeSampleIndex_]->render();
        }

        if (device_) {
            vkDeviceWaitIdle(device_->getVulkanDevice());
        }
    }
} // namespace kera
