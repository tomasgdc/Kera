// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "render_context.h"

#include "sample_utils.h"

namespace kera
{

    RenderContext::RenderContext(Renderer& renderer, FrameHandle frame) : m_renderer(renderer), m_frame(frame) {}

    void RenderContext::renderToBackbuffer(const ClearColorValue& clear_color,
                                           const std::function<void(FrameHandle)>& draw)
    {
        if (m_rendered_backbuffer)
        {
            sampleLogWarning("RenderContext currently supports one backbuffer pass per frame.");
            return;
        }

        m_rendered_backbuffer = true;
        m_renderer.beginRenderPass(m_frame, {.clear_color = clear_color, .clear_depth = 1.0f});
        draw(m_frame);
        m_renderer.renderUi(m_frame);
        m_renderer.endRenderPass(m_frame);
    }

    void RenderContext::renderToTexture(RenderTargetHandle target, const ClearColorValue& clear_color,
                                        const std::function<void(FrameHandle)>& draw)
    {
        if (!target.isValid())
        {
            sampleLogWarning("renderToTexture received an invalid render target.");
            return;
        }

        m_renderer.beginRenderPass(m_frame, target, {.clear_color = clear_color, .clear_depth = 1.0f});
        draw(m_frame);
        m_renderer.endRenderPass(m_frame);
    }

}  // namespace kera
