#include "kera/utilities/logger.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    struct Options
    {
        std::string path;
        double minCoverage = 0.05;
        double maxCoverage = 0.7;
        double minLumaRange = 0.18;
        double minDarkCoverage = 0.0;
    };

    bool readToken(std::ifstream& file, std::string& token)
    {
        token.clear();
        char ch = 0;
        while (file.get(ch))
        {
            if (ch == '#')
            {
                std::string ignored;
                std::getline(file, ignored);
                continue;
            }
            if (ch > ' ')
            {
                token.push_back(ch);
                break;
            }
        }
        while (file.get(ch))
        {
            if (ch <= ' ')
            {
                break;
            }
            token.push_back(ch);
        }
        return !token.empty();
    }

    bool parseDoubleArg(const char* text, double& value)
    {
        char* end = nullptr;
        const double parsed = std::strtod(text, &end);
        if (!end || *end != '\0')
        {
            return false;
        }
        value = parsed;
        return true;
    }

    bool parseArgs(int argc, char** argv, Options& options)
    {
        if (argc < 2)
        {
            return false;
        }
        options.path = argv[1];
        for (int i = 2; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (i + 1 >= argc)
            {
                return false;
            }
            double value = 0.0;
            if (!parseDoubleArg(argv[++i], value))
            {
                return false;
            }
            if (arg == "--min-coverage")
            {
                options.minCoverage = value;
            }
            else if (arg == "--max-coverage")
            {
                options.maxCoverage = value;
            }
            else if (arg == "--min-luma-range")
            {
                options.minLumaRange = value;
            }
            else if (arg == "--min-dark-coverage")
            {
                options.minDarkCoverage = value;
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    double luma(uint8_t r, uint8_t g, uint8_t b)
    {
        return (0.2126 * static_cast<double>(r) + 0.7152 * static_cast<double>(g) + 0.0722 * static_cast<double>(b)) /
               255.0;
    }
}

int main(int argc, char** argv)
{
    Options options;
    if (!parseArgs(argc, argv, options))
    {
        kera::Logger::getInstance().error(
            "Usage: kera_ppm_metrics <image.ppm> [--min-coverage N] [--max-coverage N] "
            "[--min-luma-range N] [--min-dark-coverage N]");
        return EXIT_FAILURE;
    }

    std::ifstream file(options.path, std::ios::binary);
    if (!file)
    {
        kera::Logger::getInstance().error("Failed to open PPM image: " + options.path);
        return EXIT_FAILURE;
    }

    std::string token;
    if (!readToken(file, token) || token != "P6")
    {
        kera::Logger::getInstance().error("PPM image is not binary P6: " + options.path);
        return EXIT_FAILURE;
    }

    if (!readToken(file, token))
    {
        return EXIT_FAILURE;
    }
    const int width = std::atoi(token.c_str());
    if (!readToken(file, token))
    {
        return EXIT_FAILURE;
    }
    const int height = std::atoi(token.c_str());
    if (!readToken(file, token))
    {
        return EXIT_FAILURE;
    }
    const int maxValue = std::atoi(token.c_str());
    if (width <= 0 || height <= 0 || maxValue != 255)
    {
        kera::Logger::getInstance().error("Unsupported PPM header in " + options.path);
        return EXIT_FAILURE;
    }

    const size_t pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> pixels(pixelCount * 3);
    file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (static_cast<size_t>(file.gcount()) != pixels.size())
    {
        kera::Logger::getInstance().error("PPM image payload is truncated: " + options.path);
        return EXIT_FAILURE;
    }

    const auto samplePixel = [&](int x, int y, int channel) -> double
    {
        const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u;
        return static_cast<double>(pixels[offset + static_cast<size_t>(channel)]) / 255.0;
    };

    double bg[3] = {};
    const int cornerSize = std::max(1, std::min(width, height) / 32);
    int bgSamples = 0;
    for (int cy = 0; cy < 2; ++cy)
    {
        for (int cx = 0; cx < 2; ++cx)
        {
            const int xStart = cx == 0 ? 0 : width - cornerSize;
            const int yStart = cy == 0 ? 0 : height - cornerSize;
            for (int y = yStart; y < yStart + cornerSize; ++y)
            {
                for (int x = xStart; x < xStart + cornerSize; ++x)
                {
                    bg[0] += samplePixel(x, y, 0);
                    bg[1] += samplePixel(x, y, 1);
                    bg[2] += samplePixel(x, y, 2);
                    ++bgSamples;
                }
            }
        }
    }
    bg[0] /= static_cast<double>(bgSamples);
    bg[1] /= static_cast<double>(bgSamples);
    bg[2] /= static_cast<double>(bgSamples);

    size_t covered = 0;
    size_t dark = 0;
    double minLuma = 1.0;
    double maxLuma = 0.0;
    for (size_t pixel = 0; pixel < pixelCount; ++pixel)
    {
        const size_t offset = pixel * 3u;
        const double r = static_cast<double>(pixels[offset + 0u]) / 255.0;
        const double g = static_cast<double>(pixels[offset + 1u]) / 255.0;
        const double b = static_cast<double>(pixels[offset + 2u]) / 255.0;
        const double diff = std::abs(r - bg[0]) + std::abs(g - bg[1]) + std::abs(b - bg[2]);
        const double pixelLuma = luma(pixels[offset + 0u], pixels[offset + 1u], pixels[offset + 2u]);
        minLuma = std::min(minLuma, pixelLuma);
        maxLuma = std::max(maxLuma, pixelLuma);
        if (diff > 0.08)
        {
            ++covered;
        }
        if (pixelLuma < 0.18)
        {
            ++dark;
        }
    }

    const double coverage = static_cast<double>(covered) / static_cast<double>(pixelCount);
    const double darkCoverage = static_cast<double>(dark) / static_cast<double>(pixelCount);
    const double lumaRange = maxLuma - minLuma;
    kera::Logger::getInstance().info("PPM metrics coverage=" + std::to_string(coverage) + " dark=" +
                                     std::to_string(darkCoverage) + " luma_range=" + std::to_string(lumaRange));

    if (coverage < options.minCoverage || coverage > options.maxCoverage || lumaRange < options.minLumaRange ||
        darkCoverage < options.minDarkCoverage)
    {
        kera::Logger::getInstance().error("PPM metrics failed thresholds for " + options.path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
