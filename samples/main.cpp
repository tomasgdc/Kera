// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "sample_utils.h"
#include "samples.h"

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
        else if (arg == "--hide-stats-overlay")
        {
            options.show_stats_overlay = false;
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
                         "[--zero-resize-smoke] [--sample-index N] [--hide-stats-overlay] "
                         "[--damaged-helmet-debug-view N] [--damaged-helmet-fixed-yaw-degrees N]\n";
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
