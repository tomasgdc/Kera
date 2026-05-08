#include "render_context.h"

#include "kera/utilities/logger.h"

namespace kera
{

    RenderContext::RenderContext(IRenderer& renderer, FrameHandle frame) : m_renderer(renderer), m_frame(frame) {}

    void RenderContext::renderToBackbuffer(const ClearColorValue& clearColor,
                                           const std::function<void(FrameHandle)>& draw)
    {
        if (m_renderedBackbuffer)
        {
            Logger::getInstance().warning("RenderContext currently supports one backbuffer pass per frame.");
            return;
        }

        m_renderedBackbuffer = true;
        m_renderer.beginRenderPass(m_frame, {.clearColor = clearColor});
        draw(m_frame);
        m_renderer.renderUi(m_frame);
        m_renderer.endRenderPass(m_frame);
    }

    void RenderContext::renderToTexture(RenderTargetHandle target, const ClearColorValue& clearColor,
                                        const std::function<void(FrameHandle)>& draw)
    {
        if (!target.isValid())
        {
            Logger::getInstance().warning("renderToTexture received an invalid render target.");
            return;
        }

        m_renderer.beginRenderPass(m_frame, target, {.clearColor = clearColor});
        draw(m_frame);
        m_renderer.endRenderPass(m_frame);
    }

}  // namespace kera
