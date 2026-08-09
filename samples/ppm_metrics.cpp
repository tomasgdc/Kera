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
        double min_msaa_marker_coverage = 0.0;
        double min_shadow_marker_coverage = 0.0;
        double min_shadow_pane_delta = 0.0;
        double min_shadow_pane_luma_range = 0.0;
        double min_shadow_diagonal_contrast = 0.0;
        double min_shadow_diagonal_mean_delta = 0.0;
        double min_ui_status_coverage = 0.0;
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
            else if (arg == "--min-msaa-marker-coverage")
            {
                options.min_msaa_marker_coverage = value;
            }
            else if (arg == "--min-shadow-marker-coverage")
            {
                options.min_shadow_marker_coverage = value;
            }
            else if (arg == "--min-shadow-pane-delta")
            {
                options.min_shadow_pane_delta = value;
            }
            else if (arg == "--min-shadow-pane-luma-range")
            {
                options.min_shadow_pane_luma_range = value;
            }
            else if (arg == "--min-shadow-diagonal-contrast")
            {
                options.min_shadow_diagonal_contrast = value;
            }
            else if (arg == "--min-shadow-diagonal-mean-delta")
            {
                options.min_shadow_diagonal_mean_delta = value;
            }
            else if (arg == "--min-ui-status-coverage")
            {
                options.min_ui_status_coverage = value;
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
            "[--min-luma-range N] [--min-dark-coverage N] [--min-msaa-marker-coverage N] "
            "[--min-shadow-marker-coverage N] [--min-shadow-pane-delta N] "
            "[--min-shadow-pane-luma-range N] [--min-shadow-diagonal-contrast N] "
            "[--min-shadow-diagonal-mean-delta N] [--min-ui-status-coverage N]");
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

    size_t red_msaa_markers = 0;
    size_t green_msaa_markers = 0;
    size_t orange_shadow_markers = 0;
    size_t cyan_shadow_markers = 0;
    size_t green_ui_status_markers = 0;
    // Vulkan screenshot rows are top-to-bottom while the diagnostic shader's UV origin is bottom-left.
    for (int y = static_cast<int>(height * 0.69); y < static_cast<int>(height * 0.95); ++y)
    {
        for (int x = static_cast<int>(width * 0.41); x < static_cast<int>(width * 0.96); ++x)
        {
            const double r = sample_pixel(x, y, 0);
            const double g = sample_pixel(x, y, 1);
            const double b = sample_pixel(x, y, 2);
            // The PPM screenshot is sRGB encoded, so compare against the encoded border colours.
            red_msaa_markers += r > 0.90 && g < 0.60 && b < 0.55 ? 1u : 0u;
            green_msaa_markers += r < 0.60 && g > 0.85 && b < 0.75 ? 1u : 0u;
            orange_shadow_markers += r > 0.90 && g > 0.55 && g < 0.80 && b < 0.45 ? 1u : 0u;
            cyan_shadow_markers += r < 0.50 && g > 0.85 && b > 0.90 ? 1u : 0u;
        }
    }
    // Attachment performance smoke draws this marker in a reserved normalized viewport rectangle.
    // Keep this check tied to that contract instead of accepting unrelated green scene pixels.
    const int ui_status_left = std::clamp(static_cast<int>(width * 0.015), 0, width);
    const int ui_status_right = std::clamp(static_cast<int>(width * 0.035), ui_status_left, width);
    const int ui_status_top = std::clamp(static_cast<int>(height * 0.015), 0, height);
    const int ui_status_bottom = std::clamp(static_cast<int>(height * 0.045), ui_status_top, height);
    for (int y = ui_status_top; y < ui_status_bottom; ++y)
    {
        for (int x = ui_status_left; x < ui_status_right; ++x)
        {
            const double r = sample_pixel(x, y, 0);
            const double g = sample_pixel(x, y, 1);
            const double b = sample_pixel(x, y, 2);
            green_ui_status_markers += r < 0.55 && g > 0.85 && b < 0.75 ? 1u : 0u;
        }
    }
    double max_shadow_pane_delta = 0.0;
    if (options.min_shadow_pane_delta > 0.0)
    {
        const int reference_left = static_cast<int>(width * 0.41);
        const int reference_right = static_cast<int>(width * 0.67);
        const int active_left = static_cast<int>(width * 0.70);
        const int comparison_top = static_cast<int>(height * 0.69);
        const int comparison_bottom = static_cast<int>(height * 0.95);
        const int block_size = std::max(4, static_cast<int>(std::min(width, height) / 60u));
        for (int y = comparison_top + block_size; y + block_size < comparison_bottom - block_size; y += block_size)
        {
            for (int x = reference_left + block_size; x + block_size < reference_right - block_size; x += block_size)
            {
                double block_delta = 0.0;
                for (int block_y = 0; block_y < block_size; ++block_y)
                {
                    for (int block_x = 0; block_x < block_size; ++block_x)
                    {
                        for (int channel = 0; channel < 3; ++channel)
                        {
                            block_delta += std::abs(
                                sample_pixel(x + block_x, y + block_y, channel) -
                                sample_pixel(active_left + x - reference_left + block_x, y + block_y, channel));
                        }
                    }
                }
                max_shadow_pane_delta =
                    std::max(max_shadow_pane_delta, block_delta / static_cast<double>(block_size * block_size * 3));
            }
        }
    }

    double shadow_pane_luma_range = 0.0;
    if (options.min_shadow_pane_luma_range > 0.0)
    {
        const int reference_left = static_cast<int>(width * 0.41);
        const int reference_right = static_cast<int>(width * 0.67);
        const int active_left = static_cast<int>(width * 0.70);
        const int comparison_top = static_cast<int>(height * 0.69);
        const int comparison_bottom = static_cast<int>(height * 0.95);
        const int border_size = std::max(2, static_cast<int>(std::min(width, height) / 180u));
        const auto pane_luma_range = [&](int left, int right)
        {
            double pane_min_luma = 1.0;
            double pane_max_luma = 0.0;
            for (int y = comparison_top + border_size; y < comparison_bottom - border_size; ++y)
            {
                for (int x = left + border_size; x < right - border_size; ++x)
                {
                    const size_t offset =
                        (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 3u;
                    const double pixel_luma = luma(pixels[offset + 0u], pixels[offset + 1u], pixels[offset + 2u]);
                    pane_min_luma = std::min(pane_min_luma, pixel_luma);
                    pane_max_luma = std::max(pane_max_luma, pixel_luma);
                }
            }
            return pane_max_luma - pane_min_luma;
        };
        const double reference_luma_range = pane_luma_range(reference_left, reference_right);
        const double active_luma_range = pane_luma_range(active_left, active_left + reference_right - reference_left);
        shadow_pane_luma_range = std::min(reference_luma_range, active_luma_range);
    }

    double shadow_diagonal_contrast = 0.0;
    double shadow_diagonal_mean_delta = 0.0;
    if (options.min_shadow_diagonal_contrast > 0.0 || options.min_shadow_diagonal_mean_delta > 0.0)
    {
        const int reference_left = static_cast<int>(width * 0.41);
        const int reference_right = static_cast<int>(width * 0.67);
        const int active_left = static_cast<int>(width * 0.70);
        const int comparison_top = static_cast<int>(height * 0.69);
        const int comparison_bottom = static_cast<int>(height * 0.95);
        const int border_size = std::max(2, static_cast<int>(std::min(width, height) / 180u));
        const auto pane_diagonal_contrast = [&](int left, int right)
        {
            const auto sample_pane_luma = [&](double pane_x, double pane_y)
            {
                constexpr int kBlockRadius = 2;
                const int center_x =
                    std::clamp(left + static_cast<int>(pane_x * (right - left)), left + border_size + kBlockRadius,
                               right - border_size - kBlockRadius - 1);
                const int center_y = std::clamp(
                    comparison_top + static_cast<int>(pane_y * (comparison_bottom - comparison_top)),
                    comparison_top + border_size + kBlockRadius, comparison_bottom - border_size - kBlockRadius - 1);
                double block_luma = 0.0;
                for (int block_y = -kBlockRadius; block_y <= kBlockRadius; ++block_y)
                {
                    for (int block_x = -kBlockRadius; block_x <= kBlockRadius; ++block_x)
                    {
                        const size_t offset = (static_cast<size_t>(center_y + block_y) * static_cast<size_t>(width) +
                                               static_cast<size_t>(center_x + block_x)) *
                                              3u;
                        block_luma += luma(pixels[offset + 0u], pixels[offset + 1u], pixels[offset + 2u]);
                    }
                }
                return block_luma / 25.0;
            };
            // Sample three locations along the flat brick-wall shadow boundary, avoiding the railings and arches.
            constexpr double kLineStartX = 0.20;
            constexpr double kLineStartY = 0.20;
            constexpr double kLineDeltaX = 0.38;
            constexpr double kLineDeltaY = 0.50;
            constexpr double kLeftNormalX = -0.796;
            constexpr double kLeftNormalY = 0.605;
            constexpr double kSideOffset = 0.08;
            double minimum_contrast = 1.0;
            for (int sample_index = 0; sample_index < 3; ++sample_index)
            {
                const double t = 0.15 + 0.20 * static_cast<double>(sample_index);
                const double center_x = kLineStartX + kLineDeltaX * t;
                const double center_y = kLineStartY + kLineDeltaY * t;
                const double left_luma =
                    sample_pane_luma(center_x + kLeftNormalX * kSideOffset, center_y + kLeftNormalY * kSideOffset);
                const double right_luma =
                    sample_pane_luma(center_x - kLeftNormalX * kSideOffset, center_y - kLeftNormalY * kSideOffset);
                minimum_contrast = std::min(minimum_contrast, std::abs(right_luma - left_luma));
            }
            return minimum_contrast;
        };
        shadow_diagonal_contrast =
            std::min(pane_diagonal_contrast(reference_left, reference_right),
                     pane_diagonal_contrast(active_left, active_left + reference_right - reference_left));

        const auto pane_diagonal_mean_delta = [&](int reference_left, int active_left, int pane_width)
        {
            double total_delta = 0.0;
            uint32_t sample_count = 0;
            for (int sample_index = 0; sample_index < 4; ++sample_index)
            {
                const double t = 0.20 + 0.20 * static_cast<double>(sample_index);
                const double center_x = 0.04 + 0.72 * t;
                const double center_y = 0.28 + 0.38 * t;
                for (int side_index = -1; side_index <= 1; ++side_index)
                {
                    const double pane_x = center_x + static_cast<double>(side_index) * -0.467 * 0.05;
                    const double pane_y = center_y + static_cast<double>(side_index) * 0.884 * 0.05;
                    const int reference_x =
                        std::clamp(reference_left + static_cast<int>(pane_x * pane_width), reference_left + border_size,
                                   reference_left + pane_width - border_size - 1);
                    const int active_x =
                        std::clamp(active_left + static_cast<int>(pane_x * pane_width), active_left + border_size,
                                   active_left + pane_width - border_size - 1);
                    const int y =
                        std::clamp(comparison_top + static_cast<int>(pane_y * (comparison_bottom - comparison_top)),
                                   comparison_top + border_size, comparison_bottom - border_size - 1);
                    for (int channel = 0; channel < 3; ++channel)
                    {
                        total_delta +=
                            std::abs(sample_pixel(reference_x, y, channel) - sample_pixel(active_x, y, channel));
                    }
                    ++sample_count;
                }
            }
            return total_delta / static_cast<double>(sample_count * 3u);
        };
        shadow_diagonal_mean_delta =
            pane_diagonal_mean_delta(reference_left, active_left, reference_right - reference_left);
    }

    const double coverage = static_cast<double>(covered) / static_cast<double>(pixel_count);
    const double dark_coverage = static_cast<double>(dark) / static_cast<double>(pixel_count);
    const double luma_range = max_luma - min_luma;
    const double red_msaa_marker_coverage = static_cast<double>(red_msaa_markers) / static_cast<double>(pixel_count);
    const double green_msaa_marker_coverage =
        static_cast<double>(green_msaa_markers) / static_cast<double>(pixel_count);
    const double orange_shadow_marker_coverage =
        static_cast<double>(orange_shadow_markers) / static_cast<double>(pixel_count);
    const double cyan_shadow_marker_coverage =
        static_cast<double>(cyan_shadow_markers) / static_cast<double>(pixel_count);
    const size_t ui_status_pixel_count =
        static_cast<size_t>(ui_status_right - ui_status_left) * static_cast<size_t>(ui_status_bottom - ui_status_top);
    const double green_ui_status_coverage = ui_status_pixel_count == 0 ? 0.0
                                                                       : static_cast<double>(green_ui_status_markers) /
                                                                             static_cast<double>(ui_status_pixel_count);
    kera::sampleLogInfo(
        "PPM metrics coverage=" + std::to_string(coverage) + " dark=" + std::to_string(dark_coverage) +
        " luma_range=" + std::to_string(luma_range) + " msaa_red_marker=" + std::to_string(red_msaa_marker_coverage) +
        " msaa_green_marker=" + std::to_string(green_msaa_marker_coverage) +
        " shadow_orange_marker=" + std::to_string(orange_shadow_marker_coverage) + " shadow_cyan_marker=" +
        std::to_string(cyan_shadow_marker_coverage) + " shadow_pane_delta=" + std::to_string(max_shadow_pane_delta) +
        " shadow_pane_luma_range=" + std::to_string(shadow_pane_luma_range) +
        " shadow_diagonal_contrast=" + std::to_string(shadow_diagonal_contrast) + " shadow_diagonal_mean_delta=" +
        std::to_string(shadow_diagonal_mean_delta) + " ui_status_marker=" + std::to_string(green_ui_status_coverage));

    if (coverage < options.min_coverage || coverage > options.max_coverage || luma_range < options.min_luma_range ||
        dark_coverage < options.min_dark_coverage ||
        (options.min_msaa_marker_coverage > 0.0 && (red_msaa_marker_coverage < options.min_msaa_marker_coverage ||
                                                    green_msaa_marker_coverage < options.min_msaa_marker_coverage)) ||
        (options.min_shadow_marker_coverage > 0.0 &&
         (orange_shadow_marker_coverage < options.min_shadow_marker_coverage ||
          cyan_shadow_marker_coverage < options.min_shadow_marker_coverage)) ||
        max_shadow_pane_delta < options.min_shadow_pane_delta ||
        shadow_pane_luma_range < options.min_shadow_pane_luma_range ||
        shadow_diagonal_contrast < options.min_shadow_diagonal_contrast ||
        shadow_diagonal_mean_delta < options.min_shadow_diagonal_mean_delta ||
        green_ui_status_coverage < options.min_ui_status_coverage)
    {
        kera::sampleLogError("PPM metrics failed thresholds for " + options.path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
