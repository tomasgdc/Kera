#include "kera/utilities/logger.h"
#include "kera/utilities/validation.h"

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

    bool require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "FAILED: %s\n", message);
            return false;
        }
        return true;
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

int main()
{
    kera::Logger& logger = kera::Logger::getInstance();
    logger.clearLogFile();
    logger.setAbortOnFatal(false);
    logger.setLogLevel(kera::LogLevel::Debug);

    const std::filesystem::path levelLog = makeLogPath("kera_logger_level_tests.log");
    removeFile(levelLog);
    if (!require(logger.setLogFilePath(levelLog.string()), "set level test log file"))
    {
        return EXIT_FAILURE;
    }

    logger.debug("debug-visible");
    logger.info("info-visible");
    logger.warning("warning-visible");
    logger.error("error-visible");
    logger.flush();
    logger.clearLogFile();

    std::string content = readTextFile(levelLog);
    if (!require(contains(content, "[DEBUG] debug-visible"), "debug message should be logged") ||
        !require(contains(content, "[INFO] info-visible"), "info message should be logged") ||
        !require(contains(content, "[WARN] warning-visible"), "warning message should be logged") ||
        !require(contains(content, "[ERROR] error-visible"), "error message should be logged"))
    {
        return EXIT_FAILURE;
    }

    const std::filesystem::path filterLog = makeLogPath("kera_logger_filter_tests.log");
    removeFile(filterLog);
    if (!require(logger.setLogFilePath(filterLog.string()), "set filter test log file"))
    {
        return EXIT_FAILURE;
    }
    logger.setLogLevel(kera::LogLevel::Warning);
    logger.info("filtered-info");
    logger.warning("kept-warning");
    logger.flush();
    logger.clearLogFile();

    content = readTextFile(filterLog);
    if (!require(!contains(content, "filtered-info"), "info message should be filtered") ||
        !require(contains(content, "[WARN] kept-warning"), "warning message should be kept"))
    {
        return EXIT_FAILURE;
    }

    setEnv("KERA_LOG_LEVEL", "error");
    logger.setLogLevel(kera::LogLevel::Debug);
    logger.configureFromEnvironment();
    if (!require(logger.getLogLevel() == kera::LogLevel::Error, "KERA_LOG_LEVEL should set error level"))
    {
        return EXIT_FAILURE;
    }
    clearEnv("KERA_LOG_LEVEL");

    const std::filesystem::path fatalLog = makeLogPath("kera_logger_fatal_tests.log");
    removeFile(fatalLog);
    if (!require(logger.setLogFilePath(fatalLog.string()), "set fatal test log file"))
    {
        return EXIT_FAILURE;
    }
    logger.setLogLevel(kera::LogLevel::Debug);
    logger.fatal("fatal-without-abort");
    logger.flush();
    logger.clearLogFile();

    content = readTextFile(fatalLog);
    if (!require(contains(content, "[FATAL] fatal-without-abort"), "fatal message should be logged without abort"))
    {
        return EXIT_FAILURE;
    }

    bool assertHandlerCalled = false;
    kera::Validation::setAssertHandler(
        [&assertHandlerCalled](const std::string& condition, const std::string& message, const char*, int)
        {
            assertHandlerCalled = true;
            require(condition == "false", "assert handler should receive condition");
            require(message == "handler-before-fatal", "assert handler should receive message");
        });
    kera::Validation::assertCondition(false, "false", "handler-before-fatal", "logger_tests.cpp", 123);
    if (!require(assertHandlerCalled, "assert handler should run before fatal log"))
    {
        return EXIT_FAILURE;
    }
    kera::Validation::setAssertHandler(nullptr);

    logger.clearLogFile();
    logger.setLogLevel(kera::LogLevel::Info);
    logger.setAbortOnFatal(true);
    removeFile(levelLog);
    removeFile(filterLog);
    removeFile(fatalLog);

    return 0;
}
