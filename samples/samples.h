// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "kera/renderer/api.h"

#include <memory>
#include <string>
#include <vector>

namespace kera
{

    class Sample;
    class RenderContext;
    class StatsOverlay;

    struct AttachmentPlaygroundConfig
    {
        // Zero selects the highest complete attachment configuration supported by the device.
        uint32_t requested_msaa_samples = 4;
        uint32_t shadow_resolution = 2048;
        bool shadows_enabled = true;
        bool preview_shadow_map = false;
        bool preview_shadow_inset = false;
        bool preview_msaa_comparison = false;
        bool preview_shadow_comparison = false;
    };

    struct SampleRunOptions
    {
        uint32_t max_frames = 0;
        uint32_t initial_sample_index = 0;
        std::string initial_sample_id;
        bool resize_smoke = false;
        bool zero_resize_smoke = false;
        bool multi_pass_resize_smoke = false;
        bool multi_pass_capture_smoke = false;
        bool multi_pass_shadows_enabled = true;
        bool multi_pass_preview_shadow_map = false;
        bool multi_pass_preview_shadow_inset = false;
        bool multi_pass_preview_msaa_comparison = false;
        bool multi_pass_preview_shadow_comparison = false;
        bool multi_pass_msaa_enabled = false;
        uint32_t multi_pass_requested_msaa_samples = 4;
        uint32_t multi_pass_shadow_resolution = 2048;
        bool multi_pass_reconfigure_smoke = false;
        bool multi_pass_performance_smoke = false;
        float multi_pass_sun_orbit_phase_radians = 0.0f;
        bool multi_pass_sun_orbit_enabled = true;
        bool show_stats_overlay = true;
        bool show_sample_ui = true;
        uint32_t damaged_helmet_debug_view = 0;
        bool damaged_helmet_fixed_yaw = false;
        float damaged_helmet_yaw_radians = 0.0f;
    };

    class SampleApplication
    {
    public:
        SampleApplication();
        ~SampleApplication();

        void run(const SampleRunOptions& options = {});
        void addSample(std::unique_ptr<Sample> sample);
        void setActiveSample(int index);

    private:
        bool initializeRenderer();
        bool recreateSwapchainResources();
        void cleanupRenderer();
        void switchActiveSample(int offset);

        std::vector<std::unique_ptr<Sample>> m_samples;
        int m_active_sample_index;
        struct SampleWindow;
        std::unique_ptr<SampleWindow> m_window;
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<StatsOverlay> m_stats_overlay;
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
        virtual void update(float delta_time) = 0;
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
