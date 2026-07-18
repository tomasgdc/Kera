// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "sample_utils.h"

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
        double min_coverage = 0.05;
        double max_coverage = 0.7;
        double min_luma_range = 0.18;
        double min_dark_coverage = 0.0;
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
                options.min_coverage = value;
            }
            else if (arg == "--max-coverage")
            {
                options.max_coverage = value;
            }
            else if (arg == "--min-luma-range")
            {
                options.min_luma_range = value;
            }
            else if (arg == "--min-dark-coverage")
            {
                options.min_dark_coverage = value;
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
        kera::sampleLogError(
            "Usage: kera_ppm_metrics <image.ppm> [--min-coverage N] [--max-coverage N] "
            "[--min-luma-range N] [--min-dark-coverage N]");
        return EXIT_FAILURE;
    }

    std::ifstream file(options.path, std::ios::binary);
    if (!file)
    {
        kera::sampleLogError("Failed to open PPM image: " + options.path);
        return EXIT_FAILURE;
    }

    std::string token;
    if (!readToken(file, token) || token != "P6")
    {
        kera::sampleLogError("PPM image is not binary P6: " + options.path);
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
    const int max_value = std::atoi(token.c_str());
    if (width <= 0 || height <= 0 || max_value != 255)
    {
        kera::sampleLogError("Unsupported PPM header in " + options.path);
        return EXIT_FAILURE;
    }

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    std::vector<uint8_t> pixels(pixel_count * 3);
    file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    if (static_cast<size_t>(file.gcount()) != pixels.size())
    {
        kera::sampleLogError("PPM image payload is truncated: " + options.path);
        return EXIT_FAILURE;
    }

    const auto sample_pixel = [&](int x, int y, int channel) -> double
    {
        const size_t offset = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u;
        return static_cast<double>(pixels[offset + static_cast<size_t>(channel)]) / 255.0;
    };

    double bg[3] = {};
    const int corner_size = std::max(1, std::min(width, height) / 32);
    int bg_samples = 0;
    for (int cy = 0; cy < 2; ++cy)
    {
        for (int cx = 0; cx < 2; ++cx)
        {
            const int x_start = cx == 0 ? 0 : width - corner_size;
            const int y_start = cy == 0 ? 0 : height - corner_size;
            for (int y = y_start; y < y_start + corner_size; ++y)
            {
                for (int x = x_start; x < x_start + corner_size; ++x)
                {
                    bg[0] += sample_pixel(x, y, 0);
                    bg[1] += sample_pixel(x, y, 1);
                    bg[2] += sample_pixel(x, y, 2);
                    ++bg_samples;
                }
            }
        }
    }
    bg[0] /= static_cast<double>(bg_samples);
    bg[1] /= static_cast<double>(bg_samples);
    bg[2] /= static_cast<double>(bg_samples);

    size_t covered = 0;
    size_t dark = 0;
    double min_luma = 1.0;
    double max_luma = 0.0;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel)
    {
        const size_t offset = pixel * 3u;
        const double r = static_cast<double>(pixels[offset + 0u]) / 255.0;
        const double g = static_cast<double>(pixels[offset + 1u]) / 255.0;
        const double b = static_cast<double>(pixels[offset + 2u]) / 255.0;
        const double diff = std::abs(r - bg[0]) + std::abs(g - bg[1]) + std::abs(b - bg[2]);
        const double pixel_luma = luma(pixels[offset + 0u], pixels[offset + 1u], pixels[offset + 2u]);
        min_luma = std::min(min_luma, pixel_luma);
        max_luma = std::max(max_luma, pixel_luma);
        if (diff > 0.08)
        {
            ++covered;
        }
        if (pixel_luma < 0.18)
        {
            ++dark;
        }
    }

    const double coverage = static_cast<double>(covered) / static_cast<double>(pixel_count);
    const double dark_coverage = static_cast<double>(dark) / static_cast<double>(pixel_count);
    const double luma_range = max_luma - min_luma;
    kera::sampleLogInfo("PPM metrics coverage=" + std::to_string(coverage) + " dark=" + std::to_string(dark_coverage) +
                        " luma_range=" + std::to_string(luma_range));

    if (coverage < options.min_coverage || coverage > options.max_coverage || luma_range < options.min_luma_range ||
        dark_coverage < options.min_dark_coverage)
    {
        kera::sampleLogError("PPM metrics failed thresholds for " + options.path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
