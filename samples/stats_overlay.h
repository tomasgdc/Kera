// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace kera
{

    class Renderer;

    class StatsOverlay
    {
    public:
        void toggleVisible()
        {
            m_visible = !m_visible;
        }
        void setVisible(bool visible)
        {
            m_visible = visible;
        }
        bool isVisible() const
        {
            return m_visible;
        }

        void draw(const Renderer& renderer, int active_sample_index, const std::string& active_sample_name,
                  float frame_time_ms);

    private:
        bool m_visible = true;
    };

}  // namespace kera
