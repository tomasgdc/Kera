#pragma once

#include "kera/renderer/interfaces.h"

#include <functional>

namespace kera
{

    class RenderContext
    {
    public:
        RenderContext(IRenderer& renderer, FrameHandle frame);

        IRenderer& renderer() const
        {
            return m_renderer;
        }
        FrameHandle frame() const
        {
            return m_frame;
        }
        bool hasRenderedBackbuffer() const
        {
            return m_renderedBackbuffer;
        }

        void renderToBackbuffer(const ClearColorValue& clearColor, const std::function<void(FrameHandle)>& draw);
        void renderToTexture(RenderTargetHandle target, const ClearColorValue& clearColor,
                             const std::function<void(FrameHandle)>& draw);

    private:
        IRenderer& m_renderer;
        FrameHandle m_frame;
        bool m_renderedBackbuffer = false;
    };

}  // namespace kera
