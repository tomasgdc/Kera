#include "samples.h"

#include "basic_triangle_sample.h"
#include "compute_sample.h"
#include "instanced_triangle_many_lights_sample.h"
#include "instanced_triangle_sample.h"
#include "kera/core/input.h"
#include "kera/core/window.h"
#include "kera/renderer/factory.h"
#include "kera/utilities/logger.h"
#include "render_context.h"
#include "stats_overlay.h"

#include <chrono>

namespace kera
{

    namespace
    {

        void cleanupActiveSample(std::vector<std::unique_ptr<Sample>>& samples, int& activeSampleIndex)
        {
            if (activeSampleIndex < 0 || activeSampleIndex >= static_cast<int>(samples.size()))
            {
                return;
            }

            samples[activeSampleIndex]->cleanup();
            activeSampleIndex = -1;
        }

    }  // namespace

    SampleApplication::SampleApplication() : m_activeSampleIndex(-1) {}

    SampleApplication::~SampleApplication()
    {
        cleanupActiveSample(m_samples, m_activeSampleIndex);
        cleanupRenderer();
    }

    void SampleApplication::addSample(std::unique_ptr<Sample> sample)
    {
        m_samples.push_back(std::move(sample));
    }

    void SampleApplication::setActiveSample(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_samples.size()))
        {
            Logger::getInstance().error("Invalid sample index: " + std::to_string(index));
            return;
        }

        cleanupActiveSample(m_samples, m_activeSampleIndex);

        m_activeSampleIndex = index;
        m_samples[m_activeSampleIndex]->initialize();
        Logger::getInstance().info("Switched to sample: " + m_samples[m_activeSampleIndex]->getName());
    }

    void SampleApplication::switchActiveSample(int offset)
    {
        if (m_samples.empty())
        {
            return;
        }

        const int sampleCount = static_cast<int>(m_samples.size());
        const int currentIndex = m_activeSampleIndex < 0 ? 0 : m_activeSampleIndex;
        const int nextIndex = (currentIndex + offset + sampleCount) % sampleCount;
        if (nextIndex == m_activeSampleIndex)
        {
            return;
        }

        setActiveSample(nextIndex);
    }

    bool SampleApplication::initializeRenderer()
    {
        Logger::getInstance().info("Initializing renderer");

        m_window = std::make_unique<Window>();
        if (!m_window->initialize("Kera Triangle Sample", 1280, 720))
        {
            Logger::getInstance().error("Failed to create window");
            return false;
        }
        Logger::getInstance().info("Window created successfully (1280x720)");

        m_renderer = CreateRenderer(GraphicsBackend::Vulkan, *m_window);
        if (!m_renderer)
        {
            Logger::getInstance().error("Failed to create renderer");
            return false;
        }

        m_renderer->initializeUi();
        m_statsOverlay = std::make_unique<StatsOverlay>();

        m_window->setEventCallback(
            [this](const SDL_Event& event)
            {
                if (m_renderer)
                {
                    m_renderer->handleUiEvent(event);
                }
            });

        Logger::getInstance().info("Renderer initialized successfully");
        return true;
    }

    bool SampleApplication::recreateSwapchainResources()
    {
        if (!m_window || !m_renderer)
        {
            return false;
        }

        const int width = m_window->getWidth();
        const int height = m_window->getHeight();
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        Logger::getInstance().info("Recreating swapchain resources for window size " + std::to_string(width) + "x" +
                                   std::to_string(height));

        if (!m_renderer->resize({
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            }))
        {
            Logger::getInstance().error("Failed to resize renderer");
            return false;
        }

        if (m_activeSampleIndex >= 0 && m_activeSampleIndex < static_cast<int>(m_samples.size()))
        {
            m_samples[m_activeSampleIndex]->resize({
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            });
        }

        m_window->clearResizeFlag();
        Logger::getInstance().info("Swapchain resources recreated successfully");
        return true;
    }

    void SampleApplication::cleanupRenderer()
    {
        cleanupActiveSample(m_samples, m_activeSampleIndex);

        if (m_statsOverlay)
        {
            m_statsOverlay.reset();
        }

        if (m_renderer)
        {
            m_renderer->shutdownUi();
        }

        if (m_renderer)
        {
            m_renderer->shutdown();
            m_renderer.reset();
        }
        if (m_window)
        {
            m_window->shutdown();
        }
    }

    void SampleApplication::run()
    {
        Logger::getInstance().info("Starting Kera Sample Application");

        if (!initializeRenderer())
        {
            Logger::getInstance().error("Failed to initialize renderer");
            return;
        }

        addSample(std::make_unique<BasicTriangleSample>(*m_renderer));
        addSample(std::make_unique<InstancedTriangleSample>(*m_renderer));
        addSample(std::make_unique<InstancedTriangleManyLightsSample>(*m_renderer));

        Logger::getInstance().info("Available samples:");
        for (size_t i = 0; i < m_samples.size(); ++i)
        {
            Logger::getInstance().info("  " + std::to_string(i) + ": " + m_samples[i]->getName());
        }

        if (m_samples.empty())
        {
            Logger::getInstance().warning("No samples registered");
            return;
        }

        m_window->setKeyCallback(
            [this](Key key, bool pressed)
            {
                if (!pressed)
                {
                    return;
                }

                if (key == Key::A)
                {
                    switchActiveSample(-1);
                }
                else if (key == Key::D)
                {
                    switchActiveSample(1);
                }
                else if (key == Key::F1 && m_statsOverlay)
                {
                    m_statsOverlay->toggleVisible();
                }
            });

        setActiveSample(0);
        Logger::getInstance().info("Running windowed render loop");

        auto previousFrameTime = std::chrono::steady_clock::now();
        while (!m_window->shouldClose())
        {
            const auto currentFrameTime = std::chrono::steady_clock::now();
            const std::chrono::duration<float> frameDelta = currentFrameTime - previousFrameTime;
            previousFrameTime = currentFrameTime;

            const float deltaTime = frameDelta.count();
            const float frameTimeMs = deltaTime * 1000.0f;

            m_window->processEvents();
            if (m_activeSampleIndex < 0 || m_activeSampleIndex >= static_cast<int>(m_samples.size()))
            {
                break;
            }

            if (m_window->wasResized())
            {
                if (m_window->getWidth() == 0 || m_window->getHeight() == 0)
                {
                    continue;
                }

                if (!recreateSwapchainResources())
                {
                    Logger::getInstance().error("Failed to handle window resize");
                    break;
                }
            }

            Sample& activeSample = *m_samples[m_activeSampleIndex];
            activeSample.update(deltaTime);

            if (m_renderer->isUiAvailable())
            {
                m_renderer->beginUi();
                activeSample.drawUi();
                if (m_statsOverlay)
                {
                    m_statsOverlay->draw(*m_renderer, m_activeSampleIndex, activeSample.getName(), frameTimeMs);
                }
                m_renderer->endUi();
            }

            FrameHandle frame = m_renderer->beginFrame();
            if (!frame.isValid())
            {
                continue;
            }

            RenderContext context(*m_renderer, frame);
            activeSample.render(context);
            if (!context.hasRenderedBackbuffer())
            {
                context.renderToBackbuffer(activeSample.getClearColor(), [](FrameHandle) {});
            }

            if (!m_renderer->endFrame(frame))
            {
                Logger::getInstance().error("Failed to end frame");
                break;
            }
        }

        cleanupRenderer();
    }

}  // namespace kera
