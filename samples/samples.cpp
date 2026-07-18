// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "samples.h"

#include "basic_triangle_sample.h"
#include "damaged_helmet_ibl_lighting_sample.h"
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

        void cleanupActiveSample(std::vector<std::unique_ptr<Sample>>& samples, int& active_sample_index)
        {
            if (active_sample_index < 0 || active_sample_index >= static_cast<int>(samples.size()))
            {
                return;
            }

            samples[active_sample_index]->cleanup();
            active_sample_index = -1;
        }

    }  // namespace

    struct SampleApplication::SampleWindow
    {
        SDL_Window* window = nullptr;
        int width = 0;
        int height = 0;
        bool should_close = false;
        bool was_resized = false;

        bool initialize(const char* title, int initial_width, int initial_height)
        {
            if (!SDL_Init(SDL_INIT_VIDEO))
            {
                sampleLogError("Failed to initialize SDL: " + std::string(SDL_GetError()));
                return false;
            }

            window = SDL_CreateWindow(title, initial_width, initial_height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
            if (!window)
            {
                sampleLogError("Failed to create SDL window: " + std::string(SDL_GetError()));
                return false;
            }

            width = initial_width;
            height = initial_height;
            should_close = false;
            was_resized = false;
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

        void processEvents(Renderer* renderer, StatsOverlay* stats_overlay, SampleApplication& app)
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
                        should_close = true;
                        break;
                    case SDL_EVENT_WINDOW_RESIZED:
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                        width = event.window.data1;
                        height = event.window.data2;
                        was_resized = true;
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
                            else if (isToggleStatsKey(event) && stats_overlay)
                            {
                                stats_overlay->toggleVisible();
                            }
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    };

    SampleApplication::SampleApplication() : m_active_sample_index(-1) {}

    SampleApplication::~SampleApplication()
    {
        cleanupActiveSample(m_samples, m_active_sample_index);
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

        cleanupActiveSample(m_samples, m_active_sample_index);

        m_active_sample_index = index;
        m_samples[m_active_sample_index]->initialize();
        sampleLogInfo("Switched to sample: " + m_samples[m_active_sample_index]->getName());
    }

    void SampleApplication::switchActiveSample(int offset)
    {
        if (m_samples.empty())
        {
            return;
        }

        const int sample_count = static_cast<int>(m_samples.size());
        const int current_index = m_active_sample_index < 0 ? 0 : m_active_sample_index;
        const int next_index = (current_index + offset + sample_count) % sample_count;
        if (next_index == m_active_sample_index)
        {
            return;
        }

        setActiveSample(next_index);
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
            .backend = static_cast<KeraGraphicsBackend>(EGraphicsBackend::VULKAN),
            .sdl_window = m_window->window,
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
        m_stats_overlay = std::make_unique<StatsOverlay>();

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

        if (m_active_sample_index >= 0 && m_active_sample_index < static_cast<int>(m_samples.size()))
        {
            m_samples[m_active_sample_index]->resize({
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height),
            });
        }

        m_window->was_resized = false;
        sampleLogInfo("Swapchain resources recreated successfully");
        return true;
    }

    void SampleApplication::cleanupRenderer()
    {
        cleanupActiveSample(m_samples, m_active_sample_index);

        if (m_stats_overlay)
        {
            m_stats_overlay.reset();
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
        if (m_stats_overlay)
        {
            m_stats_overlay->setVisible(options.show_stats_overlay);
        }

        addSample(std::make_unique<BasicTriangleSample>(*m_renderer));
        addSample(std::make_unique<InstancedTriangleSample>(*m_renderer));
        addSample(std::make_unique<InstancedTriangleManyLightsSample>(*m_renderer));
        addSample(std::make_unique<DamagedHelmetSample>(*m_renderer, options.damaged_helmet_debug_view,
                                                        options.damaged_helmet_fixed_yaw,
                                                        options.damaged_helmet_yaw_radians));
        addSample(std::make_unique<DamagedHelmetIBLLightingSample>(*m_renderer, options.damaged_helmet_debug_view,
                                                                   options.damaged_helmet_fixed_yaw,
                                                                   options.damaged_helmet_yaw_radians));

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

        if (options.initial_sample_index >= m_samples.size())
        {
            sampleLogError("Requested sample index is out of range: " + std::to_string(options.initial_sample_index));
            return;
        }

        setActiveSample(static_cast<int>(options.initial_sample_index));
        sampleLogInfo("Running windowed render loop");

        auto previous_frame_time = std::chrono::steady_clock::now();
        uint32_t rendered_frames = 0;
        bool resize_smoke_triggered = false;
        bool zero_resize_smoke_triggered = false;
        while (!m_window->should_close)
        {
            const auto current_frame_time = std::chrono::steady_clock::now();
            const std::chrono::duration<float> frame_delta = current_frame_time - previous_frame_time;
            previous_frame_time = current_frame_time;

            const float delta_time = frame_delta.count();
            const float frame_time_ms = delta_time * 1000.0f;

            m_window->processEvents(m_renderer.get(), m_stats_overlay.get(), *this);
            if (m_active_sample_index < 0 || m_active_sample_index >= static_cast<int>(m_samples.size()))
            {
                break;
            }
            if (m_window->width <= 0 || m_window->height <= 0)
            {
                continue;
            }

            if (options.resize_smoke && !resize_smoke_triggered && rendered_frames > 0)
            {
                sampleLogInfo("Running resize smoke step at 800x600.");
                if (!m_renderer->resize(kResizeSmokeExtent))
                {
                    sampleLogError("Resize smoke step failed.");
                    break;
                }
                m_samples[m_active_sample_index]->resize(kResizeSmokeExtent);
                resize_smoke_triggered = true;
            }
            if (options.zero_resize_smoke && !zero_resize_smoke_triggered && rendered_frames > 0)
            {
                sampleLogInfo("Running zero-size resize smoke step.");
                if (!m_renderer->resize(kZeroResizeSmokeExtent))
                {
                    sampleLogError("Zero-size resize smoke step failed.");
                    break;
                }
                m_samples[m_active_sample_index]->resize(kZeroResizeSmokeExtent);
                zero_resize_smoke_triggered = true;
                break;
            }

            if (m_window->was_resized)
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

            Sample& active_sample = *m_samples[m_active_sample_index];
            active_sample.update(delta_time);

            if (m_renderer->isUiAvailable())
            {
                m_renderer->beginUi();
                active_sample.drawUi();
                if (m_stats_overlay)
                {
                    m_stats_overlay->draw(*m_renderer, m_active_sample_index, active_sample.getName(), frame_time_ms);
                }
                m_renderer->endUi();
            }

            FrameHandle frame = m_renderer->beginFrame();
            if (!frame.isValid())
            {
                continue;
            }

            RenderContext context(*m_renderer, frame);
            active_sample.render(context);
            if (!context.hasRenderedBackbuffer())
            {
                context.renderToBackbuffer(active_sample.getClearColor(), [](FrameHandle) {});
            }

            if (!m_renderer->endFrame(frame))
            {
                sampleLogError("Failed to end frame");
                break;
            }

            ++rendered_frames;
            if (options.max_frames > 0 && rendered_frames >= options.max_frames)
            {
                sampleLogInfo("Sample smoke frame limit reached.");
                break;
            }
        }

        cleanupRenderer();
    }

}  // namespace kera
