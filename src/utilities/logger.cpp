// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

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
        spdlog::level::level_enum toSpdlogLevel(ELogLevel level)
        {
            switch (level)
            {
                case ELogLevel::DEBUG:
                    return spdlog::level::debug;
                case ELogLevel::INFO:
                    return spdlog::level::info;
                case ELogLevel::WARNING:
                    return spdlog::level::warn;
                case ELogLevel::ERROR:
                    return spdlog::level::err;
                case ELogLevel::FATAL:
                    return spdlog::level::critical;
            }

            return spdlog::level::info;
        }

        bool parseLogLevel(std::string_view value, ELogLevel& out_level)
        {
            if (value == "debug" || value == "DEBUG" || value == "Debug")
            {
                out_level = ELogLevel::DEBUG;
                return true;
            }
            if (value == "info" || value == "INFO" || value == "Info")
            {
                out_level = ELogLevel::INFO;
                return true;
            }
            if (value == "warn" || value == "WARN" || value == "warning" || value == "WARNING" || value == "Warning")
            {
                out_level = ELogLevel::WARNING;
                return true;
            }
            if (value == "error" || value == "ERROR" || value == "Error")
            {
                out_level = ELogLevel::ERROR;
                return true;
            }
            if (value == "fatal" || value == "FATAL" || value == "Fatal" || value == "critical" || value == "CRITICAL")
            {
                out_level = ELogLevel::FATAL;
                return true;
            }
            return false;
        }

        bool getEnvironmentValue(const char* name, std::string& out_value)
        {
#if defined(_WIN32)
            char* value = nullptr;
            size_t value_size = 0;
            if (_dupenv_s(&value, &value_size, name) != 0 || !value)
            {
                return false;
            }

            out_value.assign(value);
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
            explicit KeraFileSink(std::FILE* file) : m_file(file) {}

            ~KeraFileSink() override
            {
                if (m_file)
                {
                    std::fclose(m_file);
                    m_file = nullptr;
                }
            }

        protected:
            void sink_it_(const spdlog::details::log_msg& msg) override
            {
                if (!m_file)
                {
                    return;
                }

                spdlog::memory_buf_t formatted;
                formatter_->format(msg, formatted);
                std::fwrite(formatted.data(), 1, formatted.size(), m_file);
            }

            void flush_() override
            {
                if (m_file)
                {
                    std::fflush(m_file);
                }
            }

        private:
            std::FILE* m_file = nullptr;
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
                auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
                auto created_logger = std::make_shared<spdlog::logger>("kera", console_sink);
                created_logger->set_level(spdlog::level::trace);
                created_logger->set_formatter(makeFormatter());
                created_logger->flush_on(spdlog::level::err);
                return created_logger;
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
            ELogLevel parsed_level = m_log_level;
            if (parseLogLevel(value, parsed_level))
            {
                setLogLevel(parsed_level);
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

        auto new_sink = std::make_shared<KeraFileSink>(file);
        new_sink->set_formatter(makeFormatter());

        std::lock_guard<std::mutex> lock(getLoggerMutex());
        std::shared_ptr<spdlog::logger> logger = getSpdlogLoggerPtr();
        std::vector<spdlog::sink_ptr>& sinks = logger->sinks();
        const std::shared_ptr<KeraFileSink> old_sink = getFileSink();
        if (old_sink)
        {
            sinks.erase(std::remove(sinks.begin(), sinks.end(), old_sink), sinks.end());
        }
        sinks.push_back(new_sink);
        getFileSink() = new_sink;
        return true;
    }

    void Logger::clearLogFile()
    {
        std::lock_guard<std::mutex> lock(getLoggerMutex());
        std::shared_ptr<KeraFileSink> old_sink = getFileSink();
        if (!old_sink)
        {
            return;
        }

        std::shared_ptr<spdlog::logger> logger = getSpdlogLoggerPtr();
        std::vector<spdlog::sink_ptr>& sinks = logger->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), old_sink), sinks.end());
        getFileSink().reset();
    }

    void Logger::flush()
    {
        getSpdlogLogger().flush();
    }

    void Logger::log(ELogLevel level, const std::string& message)
    {
        if (level < m_log_level)
        {
            return;
        }

        getSpdlogLogger().log(toSpdlogLevel(level), message);

        if (level == ELogLevel::FATAL && m_abort_on_fatal)
        {
            getSpdlogLogger().flush();
            std::abort();
        }
    }
}  // namespace kera
