// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace kera
{

    enum class ELogLevel
    {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    class Logger
    {
    public:
        static Logger& getInstance();

        void configureFromEnvironment();
        bool setLogFilePath(const std::string& path);
        void clearLogFile();
        void flush();

        void setLogLevel(ELogLevel level)
        {
            m_log_level = level;
        }
        ELogLevel getLogLevel() const
        {
            return m_log_level;
        }
        void setAbortOnFatal(bool enabled)
        {
            m_abort_on_fatal = enabled;
        }
        bool getAbortOnFatal() const
        {
            return m_abort_on_fatal;
        }

        void log(ELogLevel level, const std::string& message);
        void debug(const std::string& message)
        {
            log(ELogLevel::DEBUG, message);
        }
        void info(const std::string& message)
        {
            log(ELogLevel::INFO, message);
        }
        void warning(const std::string& message)
        {
            log(ELogLevel::WARNING, message);
        }
        void error(const std::string& message)
        {
            log(ELogLevel::ERROR, message);
        }
        void fatal(const std::string& message)
        {
            log(ELogLevel::FATAL, message);
        }

    private:
        Logger();
        ~Logger() = default;

        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        ELogLevel m_log_level = ELogLevel::INFO;
        bool m_abort_on_fatal = true;
    };

}  // namespace kera
