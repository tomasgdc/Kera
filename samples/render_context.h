// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"

#include <functional>

namespace kera
{

    class RenderContext
    {
    public:
        RenderContext(Renderer& renderer, FrameHandle frame);

        Renderer& renderer() const
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
        Renderer& m_renderer;
        FrameHandle m_frame;
        bool m_renderedBackbuffer = false;
    };

}  // namespace kera
