#include "kera/utilities/logger.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

namespace kera
{
    namespace
    {
        spdlog::level::level_enum toSpdlogLevel(LogLevel level)
        {
            switch (level)
            {
                case LogLevel::Debug:
                    return spdlog::level::debug;
                case LogLevel::Info:
                    return spdlog::level::info;
                case LogLevel::Warning:
                    return spdlog::level::warn;
                case LogLevel::Error:
                    return spdlog::level::err;
                case LogLevel::Fatal:
                    return spdlog::level::critical;
            }

            return spdlog::level::info;
        }

        bool parseLogLevel(std::string_view value, LogLevel& outLevel)
        {
            if (value == "debug" || value == "DEBUG" || value == "Debug")
            {
                outLevel = LogLevel::Debug;
                return true;
            }
            if (value == "info" || value == "INFO" || value == "Info")
            {
                outLevel = LogLevel::Info;
                return true;
            }
            if (value == "warn" || value == "WARN" || value == "warning" || value == "WARNING" || value == "Warning")
            {
                outLevel = LogLevel::Warning;
                return true;
            }
            if (value == "error" || value == "ERROR" || value == "Error")
            {
                outLevel = LogLevel::Error;
                return true;
            }
            if (value == "fatal" || value == "FATAL" || value == "Fatal" || value == "critical" || value == "CRITICAL")
            {
                outLevel = LogLevel::Fatal;
                return true;
            }
            return false;
        }

        bool getEnvironmentValue(const char* name, std::string& outValue)
        {
#if defined(_WIN32)
            char* value = nullptr;
            size_t valueSize = 0;
            if (_dupenv_s(&value, &valueSize, name) != 0 || !value)
            {
                return false;
            }

            outValue.assign(value);
            std::free(value);
            return true;
#else
            const char* value = std::getenv(name);
            if (!value)
            {
                return false;
            }

            outValue = value;
            return true;
#endif
        }

        std::FILE* openAppendFile(const std::string& path)
        {
#if defined(_WIN32)
            std::FILE* file = nullptr;
            if (fopen_s(&file, path.c_str(), "a") != 0)
            {
                return nullptr;
            }
            return file;
#else
            return std::fopen(path.c_str(), "a");
#endif
        }

        const char* toLevelLabel(spdlog::level::level_enum level)
        {
            switch (level)
            {
                case spdlog::level::debug:
                    return "DEBUG";
                case spdlog::level::info:
                    return "INFO";
                case spdlog::level::warn:
                    return "WARN";
                case spdlog::level::err:
                    return "ERROR";
                case spdlog::level::critical:
                    return "FATAL";
                case spdlog::level::trace:
                    return "TRACE";
                case spdlog::level::off:
                    return "OFF";
                case spdlog::level::n_levels:
                    break;
            }

            return "INFO";
        }

        class KeraLevelFlagFormatter final : public spdlog::custom_flag_formatter
        {
        public:
            void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override
            {
                msg.color_range_start = dest.size();
                dest.push_back('[');
                const char* label = toLevelLabel(msg.level);
                while (*label != '\0')
                {
                    dest.push_back(*label);
                    ++label;
                }
                dest.push_back(']');
                msg.color_range_end = dest.size();
            }

            std::unique_ptr<custom_flag_formatter> clone() const override
            {
                return std::make_unique<KeraLevelFlagFormatter>();
            }
        };

        std::unique_ptr<spdlog::formatter> makeFormatter()
        {
            auto formatter = std::make_unique<spdlog::pattern_formatter>();
            formatter->add_flag<KeraLevelFlagFormatter>('*').set_pattern("[%Y-%m-%d %H:%M:%S.%e] %* %v");
            return formatter;
        }

        class KeraFileSink final : public spdlog::sinks::base_sink<std::mutex>
        {
        public:
            explicit KeraFileSink(std::FILE* file) : file_(file) {}

            ~KeraFileSink() override
            {
                if (file_)
                {
                    std::fclose(file_);
                    file_ = nullptr;
                }
            }

        protected:
            void sink_it_(const spdlog::details::log_msg& msg) override
            {
                if (!file_)
                {
                    return;
                }

                spdlog::memory_buf_t formatted;
                formatter_->format(msg, formatted);
                std::fwrite(formatted.data(), 1, formatted.size(), file_);
            }

            void flush_() override
            {
                if (file_)
                {
                    std::fflush(file_);
                }
            }

        private:
            std::FILE* file_ = nullptr;
        };

        std::shared_ptr<KeraFileSink>& getFileSink()
        {
            static std::shared_ptr<KeraFileSink> sink;
            return sink;
        }

        std::mutex& getLoggerMutex()
        {
            static std::mutex mutex;
            return mutex;
        }

        std::shared_ptr<spdlog::logger> getSpdlogLoggerPtr()
        {
            static auto logger = []
            {
                auto consoleSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
                auto createdLogger = std::make_shared<spdlog::logger>("kera", consoleSink);
                createdLogger->set_level(spdlog::level::trace);
                createdLogger->set_formatter(makeFormatter());
                createdLogger->flush_on(spdlog::level::err);
                return createdLogger;
            }();
            return logger;
        }

        spdlog::logger& getSpdlogLogger()
        {
            const std::shared_ptr<spdlog::logger> logger = getSpdlogLoggerPtr();
            return *logger;
        }
    }  // namespace

    Logger::Logger()
    {
        configureFromEnvironment();
    }

    Logger& Logger::getInstance()
    {
        static Logger instance;
        return instance;
    }

    void Logger::configureFromEnvironment()
    {
        std::string value;
        if (getEnvironmentValue("KERA_LOG_LEVEL", value))
        {
            LogLevel parsedLevel = log_level_;
            if (parseLogLevel(value, parsedLevel))
            {
                setLogLevel(parsedLevel);
            }
        }

        if (getEnvironmentValue("KERA_LOG_FILE", value))
        {
            if (!value.empty())
            {
                setLogFilePath(value);
            }
        }
    }

    bool Logger::setLogFilePath(const std::string& path)
    {
        if (path.empty())
        {
            clearLogFile();
            return true;
        }

        std::FILE* file = openAppendFile(path);
        if (!file)
        {
            return false;
        }

        auto newSink = std::make_shared<KeraFileSink>(file);
        newSink->set_formatter(makeFormatter());

        std::lock_guard<std::mutex> lock(getLoggerMutex());
        std::shared_ptr<spdlog::logger> logger = getSpdlogLoggerPtr();
        std::vector<spdlog::sink_ptr>& sinks = logger->sinks();
        const std::shared_ptr<KeraFileSink> oldSink = getFileSink();
        if (oldSink)
        {
            sinks.erase(std::remove(sinks.begin(), sinks.end(), oldSink), sinks.end());
        }
        sinks.push_back(newSink);
        getFileSink() = newSink;
        return true;
    }

    void Logger::clearLogFile()
    {
        std::lock_guard<std::mutex> lock(getLoggerMutex());
        std::shared_ptr<KeraFileSink> oldSink = getFileSink();
        if (!oldSink)
        {
            return;
        }

        std::shared_ptr<spdlog::logger> logger = getSpdlogLoggerPtr();
        std::vector<spdlog::sink_ptr>& sinks = logger->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), oldSink), sinks.end());
        getFileSink().reset();
    }

    void Logger::flush()
    {
        getSpdlogLogger().flush();
    }

    void Logger::log(LogLevel level, const std::string& message)
    {
        if (level < log_level_)
        {
            return;
        }

        getSpdlogLogger().log(toSpdlogLevel(level), message);

        if (level == LogLevel::Fatal && abort_on_fatal_)
        {
            getSpdlogLogger().flush();
            std::abort();
        }
    }
}  // namespace kera
