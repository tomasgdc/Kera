// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/utilities/logger.h"
#include "kera/utilities/validation.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace
{
#if defined(_CPPUNWIND) || defined(__EXCEPTIONS)
#error "Kera tests must compile with C++ exceptions disabled."
#endif

#if defined(_MSC_VER) && defined(_HAS_EXCEPTIONS) && (_HAS_EXCEPTIONS == 0)
    static_assert(_HAS_EXCEPTIONS == 0, "Kera MSVC builds must compile the STL with exceptions disabled.");
#endif

    std::filesystem::path makeLogPath(const std::string& name)
    {
        std::error_code error;
        std::filesystem::path directory = std::filesystem::temp_directory_path(error);
        if (error)
        {
            directory = std::filesystem::current_path(error);
        }
        if (error)
        {
            directory = ".";
        }

        return directory / name;
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

    bool contains(const std::string& haystack, const std::string& needle)
    {
        return haystack.find(needle) != std::string::npos;
    }

    void setEnv(const char* name, const char* value)
    {
#if defined(_WIN32)
        _putenv_s(name, value);
#else
        setenv(name, value, 1);
#endif
    }

    void clearEnv(const char* name)
    {
#if defined(_WIN32)
        _putenv_s(name, "");
#else
        unsetenv(name);
#endif
    }

    void removeFile(const std::filesystem::path& path)
    {
        std::remove(path.string().c_str());
    }
}  // namespace

TEST(KeraLogger, WritesLevelsFiltersEnvironmentFatalAndValidationMessages)
{
    kera::Logger& logger = kera::Logger::getInstance();
    logger.clearLogFile();
    logger.setAbortOnFatal(false);
    logger.setLogLevel(kera::ELogLevel::DEBUG);

    const std::filesystem::path level_log = makeLogPath("kera_logger_level_tests.log");
    removeFile(level_log);
    ASSERT_TRUE(logger.setLogFilePath(level_log.string()));

    logger.debug("debug-visible");
    logger.info("info-visible");
    logger.warning("warning-visible");
    logger.error("error-visible");
    logger.flush();
    logger.clearLogFile();

    std::string content = readTextFile(level_log);
    EXPECT_TRUE(contains(content, "[DEBUG] debug-visible"));
    EXPECT_TRUE(contains(content, "[INFO] info-visible"));
    EXPECT_TRUE(contains(content, "[WARN] warning-visible"));
    EXPECT_TRUE(contains(content, "[ERROR] error-visible"));

    const std::filesystem::path filter_log = makeLogPath("kera_logger_filter_tests.log");
    removeFile(filter_log);
    ASSERT_TRUE(logger.setLogFilePath(filter_log.string()));
    logger.setLogLevel(kera::ELogLevel::WARNING);
    logger.info("filtered-info");
    logger.warning("kept-warning");
    logger.flush();
    logger.clearLogFile();

    content = readTextFile(filter_log);
    EXPECT_FALSE(contains(content, "filtered-info"));
    EXPECT_TRUE(contains(content, "[WARN] kept-warning"));

    setEnv("KERA_LOG_LEVEL", "error");
    logger.setLogLevel(kera::ELogLevel::DEBUG);
    logger.configureFromEnvironment();
    EXPECT_EQ(logger.getLogLevel(), kera::ELogLevel::ERROR);
    clearEnv("KERA_LOG_LEVEL");

    const std::filesystem::path fatal_log = makeLogPath("kera_logger_fatal_tests.log");
    removeFile(fatal_log);
    ASSERT_TRUE(logger.setLogFilePath(fatal_log.string()));
    logger.setLogLevel(kera::ELogLevel::DEBUG);
    logger.fatal("fatal-without-abort");
    logger.flush();
    logger.clearLogFile();

    content = readTextFile(fatal_log);
    EXPECT_TRUE(contains(content, "[FATAL] fatal-without-abort"));

    bool assert_handler_called = false;
    kera::Validation::setAssertHandler(
        [&assert_handler_called](const std::string& condition, const std::string& message, const char*, int)
        {
            assert_handler_called = true;
            EXPECT_EQ(condition, "false");
            EXPECT_EQ(message, "handler-before-fatal");
        });
    kera::Validation::assertCondition(false, "false", "handler-before-fatal", "logger_tests.cpp", 123);
    EXPECT_TRUE(assert_handler_called);
    kera::Validation::setAssertHandler(nullptr);

    logger.clearLogFile();
    logger.setLogLevel(kera::ELogLevel::INFO);
    logger.setAbortOnFatal(true);
    removeFile(level_log);
    removeFile(filter_log);
    removeFile(fatal_log);
}
