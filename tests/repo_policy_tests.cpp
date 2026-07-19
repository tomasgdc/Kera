// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#error "Kera tests must compile with C++ exceptions disabled."
#endif

#if defined(_MSC_VER) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 0)
    static_assert(_HAS_EXCEPTIONS == 0, "Kera MSVC builds must compile the STL with exceptions disabled.");
#endif

    std::filesystem::path sourceRoot()
    {
        return std::filesystem::path(KERA_SOURCE_DIR);
    }

    std::string readTextFile(const std::filesystem::path& path)
    {
        std::ifstream file(path);
        std::string content;
        std::string line;
        while (std::getline(file, line))
        {
            content += line;
            content += '\n';
        }
        return content;
    }

    bool contains(std::string_view haystack, std::string_view needle)
    {
        return haystack.find(needle) != std::string_view::npos;
    }

    bool hasDirectConsoleOutput(std::string_view content)
    {
        return contains(content, "std::cout") || contains(content, "std::cerr") || contains(content, "printf(") ||
               contains(content, "printf (") || contains(content, "fprintf(") || contains(content, "fprintf (");
    }

    bool hasRawSpdlogUsage(std::string_view content)
    {
        return contains(content, "spdlog::") || contains(content, "#include <spdlog");
    }

    std::vector<std::filesystem::path> collectFiles(const std::vector<std::filesystem::path>& roots,
                                                    const std::vector<std::string>& extensions)
    {
        std::vector<std::filesystem::path> files;
        for (const std::filesystem::path& root : roots)
        {
            std::error_code error;
            if (!std::filesystem::exists(root, error))
            {
                continue;
            }

            std::filesystem::recursive_directory_iterator it(
                root, std::filesystem::directory_options::skip_permission_denied, error);
            const std::filesystem::recursive_directory_iterator end;
            while (!error && it != end)
            {
                if (it->is_regular_file(error))
                {
                    const std::string extension = it->path().extension().string();
                    for (const std::string& expected : extensions)
                    {
                        if (extension == expected)
                        {
                            files.push_back(it->path());
                            break;
                        }
                    }
                }
                it.increment(error);
            }
            EXPECT_FALSE(error) << "Failed to scan " << root.string() << ": " << error.message();
        }
        return files;
    }

    std::filesystem::path normalized(const std::filesystem::path& path)
    {
        std::error_code error;
        std::filesystem::path normalized_path = std::filesystem::weakly_canonical(path, error);
        if (error)
        {
            normalized_path = path.lexically_normal();
        }
        return normalized_path;
    }
}  // namespace

TEST(KeraRepoPolicy, PublicAbiHeadersAreStlFree)
{
    const std::filesystem::path root = sourceRoot();
    const std::vector<std::filesystem::path> public_headers{
        root / "include/kera/kera.h",
        root / "include/kera/renderer/api.h",
        root / "include/kera/renderer/abi.h",
    };
    const std::vector<std::string_view> forbidden_patterns{
        "std::",
        "#include <string>",
        "#include <vector>",
        "#include <span>",
        "#include <memory>",
        "#include <optional>",
        "#include <functional>",
    };

    for (const std::filesystem::path& header : public_headers)
    {
        const std::string content = readTextFile(header);
        ASSERT_FALSE(content.empty()) << "Public ABI header should be readable: " << header.string();
        for (std::string_view pattern : forbidden_patterns)
        {
            EXPECT_FALSE(contains(content, pattern))
                << "Public ABI header " << header.string() << " contains forbidden STL pattern: " << pattern;
        }
    }
}

TEST(KeraRepoPolicy, SamplesUsePublicKeraApiOnly)
{
    const std::filesystem::path root = sourceRoot();
    const std::vector<std::filesystem::path> sample_files = collectFiles({root / "samples"}, {".h", ".cpp"});
    const std::vector<std::string_view> forbidden_headers{
        "kera/renderer/interfaces.h",
        "kera/renderer/factory.h",
        "kera/renderer/gltf_loader.h",
        "kera/renderer/reflection_contracts.h",
        "kera/renderer/descriptors.h",
        "kera/renderer/result.h",
        "kera/renderer/backend/",
        "kera/utilities/logger.h",
        "kera/core/",
    };

    ASSERT_FALSE(sample_files.empty());
    for (const std::filesystem::path& source : sample_files)
    {
        const std::string content = readTextFile(source);
        EXPECT_FALSE(contains(content, ".semantic("))
            << "Sample source " << source.string()
            << " must map vertex inputs by reflected field names, not shader semantic strings.";
        for (std::string_view pattern : forbidden_headers)
        {
            EXPECT_FALSE(contains(content, pattern))
                << "Sample source " << source.string()
                << " includes forbidden Kera private/internal header pattern: " << pattern;
        }
    }
}

TEST(KeraRepoPolicy, LoggingRulesStayBehindLoggerFacade)
{
    const std::filesystem::path root = sourceRoot();
    const std::filesystem::path allowed_console_file = normalized(root / "samples/main.cpp");
    const std::filesystem::path allowed_spdlog_file = normalized(root / "src/utilities/logger.cpp");
    const std::vector<std::filesystem::path> public_logging_headers{
        root / "include/kera/kera.h",
        root / "include/kera/renderer/api.h",
        root / "include/kera/renderer/abi.h",
    };
    const std::vector<std::filesystem::path> scanned_files = collectFiles(
        {
            root / "include",
            root / "src",
            root / "samples",
        },
        {".h", ".cpp"});

    ASSERT_FALSE(scanned_files.empty());
    for (const std::filesystem::path& file : scanned_files)
    {
        const std::filesystem::path normalized_file = normalized(file);
        const std::string content = readTextFile(file);

        if (normalized_file != allowed_console_file)
        {
            EXPECT_FALSE(hasDirectConsoleOutput(content)) << "Production logging must use kera::Logger, but "
                                                          << file.string() << " contains direct console output.";
        }

        if (normalized_file != allowed_spdlog_file)
        {
            EXPECT_FALSE(hasRawSpdlogUsage(content))
                << "Raw spdlog usage is only allowed in src/utilities/logger.cpp, but found it in " << file.string()
                << ".";
        }
    }

    for (const std::filesystem::path& header : public_logging_headers)
    {
        const std::string content = readTextFile(header);
        EXPECT_FALSE(hasRawSpdlogUsage(content))
            << "Installed public header " << header.string() << " must not expose spdlog.";
    }
}
