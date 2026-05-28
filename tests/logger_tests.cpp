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

#if defined(_MSC_VER)
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
    logger.setLogLevel(kera::LogLevel::Debug);

    const std::filesystem::path levelLog = makeLogPath("kera_logger_level_tests.log");
    removeFile(levelLog);
    ASSERT_TRUE(logger.setLogFilePath(levelLog.string()));

    logger.debug("debug-visible");
    logger.info("info-visible");
    logger.warning("warning-visible");
    logger.error("error-visible");
    logger.flush();
    logger.clearLogFile();

    std::string content = readTextFile(levelLog);
    EXPECT_TRUE(contains(content, "[DEBUG] debug-visible"));
    EXPECT_TRUE(contains(content, "[INFO] info-visible"));
    EXPECT_TRUE(contains(content, "[WARN] warning-visible"));
    EXPECT_TRUE(contains(content, "[ERROR] error-visible"));

    const std::filesystem::path filterLog = makeLogPath("kera_logger_filter_tests.log");
    removeFile(filterLog);
    ASSERT_TRUE(logger.setLogFilePath(filterLog.string()));
    logger.setLogLevel(kera::LogLevel::Warning);
    logger.info("filtered-info");
    logger.warning("kept-warning");
    logger.flush();
    logger.clearLogFile();

    content = readTextFile(filterLog);
    EXPECT_FALSE(contains(content, "filtered-info"));
    EXPECT_TRUE(contains(content, "[WARN] kept-warning"));

    setEnv("KERA_LOG_LEVEL", "error");
    logger.setLogLevel(kera::LogLevel::Debug);
    logger.configureFromEnvironment();
    EXPECT_EQ(logger.getLogLevel(), kera::LogLevel::Error);
    clearEnv("KERA_LOG_LEVEL");

    const std::filesystem::path fatalLog = makeLogPath("kera_logger_fatal_tests.log");
    removeFile(fatalLog);
    ASSERT_TRUE(logger.setLogFilePath(fatalLog.string()));
    logger.setLogLevel(kera::LogLevel::Debug);
    logger.fatal("fatal-without-abort");
    logger.flush();
    logger.clearLogFile();

    content = readTextFile(fatalLog);
    EXPECT_TRUE(contains(content, "[FATAL] fatal-without-abort"));

    bool assertHandlerCalled = false;
    kera::Validation::setAssertHandler(
        [&assertHandlerCalled](const std::string& condition, const std::string& message, const char*, int)
        {
            assertHandlerCalled = true;
            EXPECT_EQ(condition, "false");
            EXPECT_EQ(message, "handler-before-fatal");
        });
    kera::Validation::assertCondition(false, "false", "handler-before-fatal", "logger_tests.cpp", 123);
    EXPECT_TRUE(assertHandlerCalled);
    kera::Validation::setAssertHandler(nullptr);

    logger.clearLogFile();
    logger.setLogLevel(kera::LogLevel::Info);
    logger.setAbortOnFatal(true);
    removeFile(levelLog);
    removeFile(filterLog);
    removeFile(fatalLog);
}
