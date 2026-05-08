#pragma once

#include "kera/renderer/interfaces.h"

#include <memory>
#include <string>
#include <vector>

namespace kera
{

    class Sample;
    class IRenderer;
    class Window;
    class RenderContext;
    class StatsOverlay;

    class SampleApplication
    {
    public:
        SampleApplication();
        ~SampleApplication();

        void run();
        void addSample(std::unique_ptr<Sample> sample);
        void setActiveSample(int index);

    private:
        bool initializeRenderer();
        bool recreateSwapchainResources();
        void cleanupRenderer();
        void switchActiveSample(int offset);

        std::vector<std::unique_ptr<Sample>> m_samples;
        int m_activeSampleIndex;
        std::unique_ptr<Window> m_window;
        std::unique_ptr<IRenderer> m_renderer;
        std::unique_ptr<StatsOverlay> m_statsOverlay;
    };

    class Sample
    {
    public:
        Sample(const std::string& name) : m_name(name) {}
        virtual ~Sample() = default;

        const std::string& getName() const
        {
            return m_name;
        }

        virtual void initialize() = 0;
        virtual void update(float deltaTime) = 0;
        virtual void resize(Extent2D) {}
        virtual void render(RenderContext& context) = 0;
        virtual void drawUi() {}
        virtual ClearColorValue getClearColor() const
        {
            return {0.0f, 0.0f, 0.1f, 1.0f};
        }
        virtual void cleanup() = 0;

    private:
        std::string m_name;
    };

}  // namespace kera
