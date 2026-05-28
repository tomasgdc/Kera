// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace kera
{

    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    class Logger
    {
    public:
        static Logger& getInstance();

        void configureFromEnvironment();
        bool setLogFilePath(const std::string& path);
        void clearLogFile();
        void flush();

        void setLogLevel(LogLevel level)
        {
            log_level_ = level;
        }
        LogLevel getLogLevel() const
        {
            return log_level_;
        }
        void setAbortOnFatal(bool enabled)
        {
            abort_on_fatal_ = enabled;
        }
        bool getAbortOnFatal() const
        {
            return abort_on_fatal_;
        }

        void log(LogLevel level, const std::string& message);
        void debug(const std::string& message)
        {
            log(LogLevel::Debug, message);
        }
        void info(const std::string& message)
        {
            log(LogLevel::Info, message);
        }
        void warning(const std::string& message)
        {
            log(LogLevel::Warning, message);
        }
        void error(const std::string& message)
        {
            log(LogLevel::Error, message);
        }
        void fatal(const std::string& message)
        {
            log(LogLevel::Fatal, message);
        }

    private:
        Logger();
        ~Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        LogLevel log_level_ = LogLevel::Info;
        bool abort_on_fatal_ = true;
    };

}  // namespace kera
