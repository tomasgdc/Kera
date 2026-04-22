#include "samples.h"
#include "basic_triangle_sample.h"
#include "compute_sample.h"
#include "kera/core/window.h"
#include "kera/renderer/factory.h"
#include "kera/utilities/logger.h"

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
        Logger::getInstance().info("Initializing renderer");

        // Create window
        window_ = std::make_unique<Window>();
        if (!window_->initialize("Kera Triangle Sample", 1280, 720)) {
            Logger::getInstance().error("Failed to create window");
            return false;
        }
        Logger::getInstance().info("Window created successfully (1280x720)");

        const auto createRendererResult = CreateRenderer(GraphicsBackend::Vulkan, *window_);
        if (createRendererResult.hasError()) {
            Logger::getInstance().error(createRendererResult.error());
            return false;
        }
        renderer_ = createRendererResult.value();

        Logger::getInstance().info("Renderer initialized successfully");
        return true;
    }

    bool SampleApplication::recreateSwapchainResources() {
        if (!window_ || !renderer_) {
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

        const auto resizeResult = renderer_->resize({
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height),
        });
        if (resizeResult.hasError()) {
            Logger::getInstance().error(resizeResult.error());
            return false;
        }

        window_->clearResizeFlag();
        Logger::getInstance().info("Swapchain resources recreated successfully");
        return true;
    }

    void SampleApplication::cleanupRenderer() {
        if (renderer_) {
            renderer_->shutdown();
            renderer_.reset();
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
        addSample(std::make_unique<BasicTriangleSample>(renderer_));
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

        if (renderer_) {
            renderer_->shutdown();
            renderer_.reset();
        }
    }
} // namespace kera
