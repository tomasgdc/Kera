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
            if (options.maxFrames == 0)
            {
                options.maxFrames = 1;
            }
        }
        else if (arg == "--smoke-frames")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "--smoke-frames requires a frame count\n";
                return EXIT_FAILURE;
            }
            options.maxFrames = static_cast<uint32_t>(std::strtoul(argv[++index], nullptr, 10));
        }
        else if (arg == "--resize-smoke")
        {
            options.resizeSmoke = true;
        }
        else if (arg == "--help")
        {
            std::cout << "Usage: kera_samples [--smoke-test] [--smoke-frames N] [--resize-smoke]\n";
            return EXIT_SUCCESS;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << '\n';
            return EXIT_FAILURE;
        }
    }

    kera::SampleApplication app;
    app.run(options);
    return EXIT_SUCCESS;
}
