// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "samples.h"

#include "basic_triangle_sample.h"
#include "damaged_helmet_sample.h"
#include "instanced_triangle_many_lights_sample.h"
#include "instanced_triangle_sample.h"
#include "render_context.h"
#include "sample_utils.h"
#include "stats_overlay.h"

#include <SDL3/SDL.h>

#include <chrono>

namespace kera
{

    namespace
    {
        constexpr int kInitialWindowWidth = 1280;
        constexpr int kInitialWindowHeight = 720;
        constexpr Extent2D kResizeSmokeExtent{800, 600};
        constexpr Extent2D kZeroResizeSmokeExtent{0, 0};

        bool isPreviousSampleKey(const SDL_Event& event)
        {
            return event.key.key == SDLK_A;
        }

        bool isNextSampleKey(const SDL_Event& event)
        {
            return event.key.key == SDLK_D;
        }

        bool isToggleStatsKey(const SDL_Event& event)
        {
            return event.key.key == SDLK_F1;
        }

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

    struct SampleApplication::SampleWindow
    {
        SDL_Window* window = nullptr;
        int width = 0;
        int height = 0;
        bool shouldClose = false;
        bool wasResized = false;

        bool initialize(const char* title, int initialWidth, int initialHeight)
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                sampleLogError("Failed to initialize SDL: " + std::string(SDL_GetError()));
                return false;
            }

            window = SDL_CreateWindow(title, initialWidth, initialHeight, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
            if (!window)
            {
                sampleLogError("Failed to create SDL window: " + std::string(SDL_GetError()));
                return false;
            }

            width = initialWidth;
            height = initialHeight;
            shouldClose = false;
            wasResized = false;
            return true;
        }

        void shutdown()
        {
            if (window)
            {
                SDL_DestroyWindow(window);
                window = nullptr;
            }
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }

        void processEvents(Renderer* renderer, StatsOverlay* statsOverlay, SampleApplication& app)
        {
            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                if (renderer)
                {
                    renderer->handleUiEvent(&event);
                }

                switch (event.type)
                {
                    case SDL_EVENT_QUIT:
                        shouldClose = true;
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        width = event.window.data1;
                        height = event.window.data2;
                        wasResized = true;
                        break;
                    case SDL_EVENT_KEY_DOWN:
                        if (!event.key.repeat)
                        {
                            if (isPreviousSampleKey(event))
                            {
                                app.switchActiveSample(-1);
                            }
                            else if (isNextSampleKey(event))
                            {
                                app.switchActiveSample(1);
                            }
                            else if (isToggleStatsKey(event) && statsOverlay)
                            {
                                statsOverlay->toggleVisible();
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    };

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
            sampleLogError("Invalid sample index: " + std::to_string(index));
            return;
        }

        cleanupActiveSample(m_samples, m_activeSampleIndex);

        m_activeSampleIndex = index;
        m_samples[m_activeSampleIndex]->initialize();
        sampleLogInfo("Switched to sample: " + m_samples[m_activeSampleIndex]->getName());
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
        sampleLogInfo("Initializing renderer");

        m_window = std::make_unique<SampleWindow>();
        if (!m_window->initialize("Kera Samples", kInitialWindowWidth, kInitialWindowHeight))
        {
            sampleLogError("Failed to create window");
            return false;
        }
        sampleLogInfo("Window created successfully (" + std::to_string(kInitialWindowWidth) + "x" +
                      std::to_string(kInitialWindowHeight) + ")");

        auto renderer = Renderer::create({
            .backend = GraphicsBackend::Vulkan,
            .sdlWindow = m_window->window,
            .width = static_cast<uint32_t>(m_window->width),
            .height = static_cast<uint32_t>(m_window->height),
        });
        if (!renderer.isValid())
        {
            sampleLogError("Failed to create renderer");
            return false;
        }
        m_renderer = std::make_unique<Renderer>(std::move(renderer));

        m_renderer->initializeUi();
        m_statsOverlay = std::make_unique<StatsOverlay>();

        sampleLogInfo("Renderer initialized successfully");
        return true;
    }

    bool SampleApplication::recreateSwapchainResources()
    {
        if (!m_window || !m_renderer)
        {
            return false;
        }

        const int width = m_window->width;
        const int height = m_window->height;
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        sampleLogInfo("Recreating swapchain resources for window size " + std::to_string(width) + "x" +
                      std::to_string(height));

        if (!m_renderer->resize({
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            }))
        {
            sampleLogError("Failed to resize renderer");
            return false;
        }

        if (m_activeSampleIndex >= 0 && m_activeSampleIndex < static_cast<int>(m_samples.size()))
        {
            m_samples[m_activeSampleIndex]->resize({
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            });
        }

        m_window->wasResized = false;
        sampleLogInfo("Swapchain resources recreated successfully");
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
            m_window.reset();
        }
    }

    void SampleApplication::run(const SampleRunOptions& options)
    {
        sampleLogInfo("Starting Kera Sample Application");

        if (!initializeRenderer())
        {
            sampleLogError("Failed to initialize renderer");
            return;
        }
        if (m_statsOverlay)
        {
            m_statsOverlay->setVisible(options.showStatsOverlay);
        }

        addSample(std::make_unique<BasicTriangleSample>(*m_renderer));
        addSample(std::make_unique<InstancedTriangleSample>(*m_renderer));
        addSample(std::make_unique<InstancedTriangleManyLightsSample>(*m_renderer));
        addSample(std::make_unique<DamagedHelmetSample>(*m_renderer, options.damagedHelmetDebugView,
                                                        options.damagedHelmetFixedYaw,
                                                        options.damagedHelmetYawRadians));

        sampleLogInfo("Available samples:");
        for (size_t i = 0; i < m_samples.size(); ++i)
        {
            sampleLogInfo("  " + std::to_string(i) + ": " + m_samples[i]->getName());
        }

        if (m_samples.empty())
        {
            sampleLogWarning("No samples registered");
            return;
        }

        if (options.initialSampleIndex >= m_samples.size())
        {
            sampleLogError("Requested sample index is out of range: " + std::to_string(options.initialSampleIndex));
            return;
        }

        setActiveSample(static_cast<int>(options.initialSampleIndex));
        sampleLogInfo("Running windowed render loop");

        auto previousFrameTime = std::chrono::steady_clock::now();
        uint32_t renderedFrames = 0;
        bool resizeSmokeTriggered = false;
        bool zeroResizeSmokeTriggered = false;
        while (!m_window->shouldClose)
        {
            const auto currentFrameTime = std::chrono::steady_clock::now();
            const std::chrono::duration<float> frameDelta = currentFrameTime - previousFrameTime;
            previousFrameTime = currentFrameTime;

            const float deltaTime = frameDelta.count();
            const float frameTimeMs = deltaTime * 1000.0f;

            m_window->processEvents(m_renderer.get(), m_statsOverlay.get(), *this);
            if (m_activeSampleIndex < 0 || m_activeSampleIndex >= static_cast<int>(m_samples.size()))
            {
                break;
            }
            if (m_window->width <= 0 || m_window->height <= 0)
            {
                continue;
            }

            if (options.resizeSmoke && !resizeSmokeTriggered && renderedFrames > 0)
            {
                sampleLogInfo("Running resize smoke step at 800x600.");
                if (!m_renderer->resize(kResizeSmokeExtent))
                {
                    sampleLogError("Resize smoke step failed.");
                    break;
                }
                m_samples[m_activeSampleIndex]->resize(kResizeSmokeExtent);
                resizeSmokeTriggered = true;
            }
            if (options.zeroResizeSmoke && !zeroResizeSmokeTriggered && renderedFrames > 0)
            {
                sampleLogInfo("Running zero-size resize smoke step.");
                if (!m_renderer->resize(kZeroResizeSmokeExtent))
                {
                    sampleLogError("Zero-size resize smoke step failed.");
                    break;
                }
                m_samples[m_activeSampleIndex]->resize(kZeroResizeSmokeExtent);
                zeroResizeSmokeTriggered = true;
                break;
            }

            if (m_window->wasResized)
            {
                if (m_window->width == 0 || m_window->height == 0)
                {
                    continue;
                }

                if (!recreateSwapchainResources())
                {
                    sampleLogError("Failed to handle window resize");
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
                sampleLogError("Failed to end frame");
                break;
            }

            ++renderedFrames;
            if (options.maxFrames > 0 && renderedFrames >= options.maxFrames)
            {
                sampleLogInfo("Sample smoke frame limit reached.");
                break;
            }
        }

        cleanupRenderer();
    }

}  // namespace kera
