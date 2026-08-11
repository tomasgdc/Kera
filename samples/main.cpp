// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "sample_utils.h"
#include "samples.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    kera::SampleRunOptions options{};
    for (int index = 1; index < argc; ++index)
    {
        const std::string arg = argv[index];
        if (arg == "--smoke-test")
        {
            if (options.max_frames == 0)
            {
                options.max_frames = 1;
            }
        }
        else if (arg == "--smoke-frames")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--smoke-frames requires a frame count");
                return EXIT_FAILURE;
            }
            options.max_frames = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        }
        else if (arg == "--resize-smoke")
        {
            options.resize_smoke = true;
        }
        else if (arg == "--zero-resize-smoke")
        {
            options.zero_resize_smoke = true;
        }
        else if (arg == "--multi-pass-resize-smoke")
        {
            options.multi_pass_resize_smoke = true;
        }
        else if (arg == "--multi-pass-capture-smoke")
        {
            options.multi_pass_capture_smoke = true;
        }
        else if (arg == "--multi-pass-shadows-disabled")
        {
            options.multi_pass_shadows_enabled = false;
        }
        else if (arg == "--multi-pass-preview-shadow-map")
        {
            options.multi_pass_preview_shadow_map = true;
        }
        else if (arg == "--multi-pass-msaa-4x")
        {
            options.multi_pass_msaa_enabled = true;
            options.multi_pass_requested_msaa_samples = 4;
        }
        else if (arg == "--attachment-msaa")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--attachment-msaa requires auto, 1, 2, 4, or 8");
                return EXIT_FAILURE;
            }
            const std::string value = argv[++index];
            if (value == "auto")
            {
                options.multi_pass_requested_msaa_samples = 0;
            }
            else if (value == "1" || value == "2" || value == "4" || value == "8")
            {
                options.multi_pass_requested_msaa_samples =
                    static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            }
            else
            {
                kera::sampleLogError("--attachment-msaa requires auto, 1, 2, 4, or 8");
                return EXIT_FAILURE;
            }
        }
        else if (arg == "--attachment-shadows")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--attachment-shadows requires on or off");
                return EXIT_FAILURE;
            }
            const std::string value = argv[++index];
            if (value == "on")
            {
                options.multi_pass_shadows_enabled = true;
            }
            else if (value == "off")
            {
                options.multi_pass_shadows_enabled = false;
            }
            else
            {
                kera::sampleLogError("--attachment-shadows requires on or off");
                return EXIT_FAILURE;
            }
        }
        else if (arg == "--attachment-shadow-resolution")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--attachment-shadow-resolution requires 1024, 2048, or 4096");
                return EXIT_FAILURE;
            }
            const uint32_t resolution = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
            if (resolution != 1024 && resolution != 2048 && resolution != 4096)
            {
                kera::sampleLogError("--attachment-shadow-resolution requires 1024, 2048, or 4096");
                return EXIT_FAILURE;
            }
            options.multi_pass_shadow_resolution = resolution;
        }
        else if (arg == "--attachment-preview-shadow-map")
        {
            options.multi_pass_preview_shadow_map = true;
        }
        else if (arg == "--attachment-shadow-inset")
        {
            options.multi_pass_preview_shadow_inset = true;
        }
        else if (arg == "--attachment-msaa-comparison")
        {
            options.multi_pass_preview_msaa_comparison = true;
        }
        else if (arg == "--attachment-shadow-comparison")
        {
            options.multi_pass_preview_shadow_comparison = true;
        }
        else if (arg == "--attachment-reconfigure-smoke")
        {
            options.multi_pass_reconfigure_smoke = true;
        }
        else if (arg == "--attachment-performance-smoke")
        {
            options.multi_pass_performance_smoke = true;
        }
        else if (arg == "--attachment-sun-orbit-degrees")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--attachment-sun-orbit-degrees requires a value from 0 through 360");
                return EXIT_FAILURE;
            }
            const float degrees = std::strtof(argv[++index], nullptr);
            if (!std::isfinite(degrees) || degrees < 0.0f || degrees > 360.0f)
            {
                kera::sampleLogError("--attachment-sun-orbit-degrees requires a value from 0 through 360");
                return EXIT_FAILURE;
            }
            options.multi_pass_sun_orbit_phase_radians = degrees == 360.0f ? 0.0f : degrees * 0.017453292519943295f;
            options.multi_pass_sun_orbit_enabled = false;
        }
        else if (arg == "--hide-stats-overlay")
        {
            options.show_stats_overlay = false;
        }
        else if (arg == "--hide-sample-ui")
        {
            options.show_sample_ui = false;
        }
        else if (arg == "--damaged-helmet-debug-view")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--damaged-helmet-debug-view requires a mode index");
                return EXIT_FAILURE;
            }
            options.damaged_helmet_debug_view = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        }
        else if (arg == "--damaged-helmet-fixed-yaw-degrees")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--damaged-helmet-fixed-yaw-degrees requires a degree value");
                return EXIT_FAILURE;
            }
            options.damaged_helmet_fixed_yaw = true;
            options.damaged_helmet_yaw_radians = std::strtof(argv[++index], nullptr) * 0.017453292519943295f;
        }
        else if (arg == "--sample")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--sample requires a sample id");
                return EXIT_FAILURE;
            }
            options.initial_sample_id = argv[++index];
        }
        else if (arg == "--sample-index")
        {
            if (index + 1 >= argc)
            {
                kera::sampleLogError("--sample-index requires a sample index");
                return EXIT_FAILURE;
            }
            options.initial_sample_index = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: kera_samples [--smoke-test] [--smoke-frames N] [--resize-smoke] "
                         "[--zero-resize-smoke] [--multi-pass-resize-smoke] [--multi-pass-capture-smoke] "
                         "[--attachment-msaa auto|1|2|4|8] [--attachment-shadows on|off] "
                         "[--attachment-shadow-resolution 1024|2048|4096] [--attachment-preview-shadow-map] "
                         "[--attachment-shadow-inset] [--attachment-msaa-comparison] [--attachment-shadow-comparison] "
                         "[--attachment-reconfigure-smoke] [--attachment-performance-smoke] "
                         "[--attachment-sun-orbit-degrees 0..360] "
                         "[--sample attachment-playground] [--sample-index N] "
                         "[--multi-pass-shadows-disabled] [--multi-pass-preview-shadow-map] [--multi-pass-msaa-4x] "
                         "[--hide-stats-overlay] [--hide-sample-ui] "
                         "[--damaged-helmet-debug-view N] "
                         "[--damaged-helmet-fixed-yaw-degrees N]\n";
            return EXIT_SUCCESS;
        }
        else
        {
            kera::sampleLogError("Unknown argument: " + arg);
            return EXIT_FAILURE;
        }
    }

    kera::SampleApplication app;
    app.run(options);
    return EXIT_SUCCESS;
}
