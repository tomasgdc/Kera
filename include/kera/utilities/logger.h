#pragma once

#include <string>
#include <iostream>

namespace kera {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Fatal
};

class Logger {
public:
    static Logger& getInstance();

    void setLogLevel(LogLevel level) { log_level_ = level; }
    LogLevel getLogLevel() const { return log_level_; }

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message) { log(LogLevel::Debug, message); }
    void info(const std::string& message) { log(LogLevel::Info, message); }
    void warning(const std::string& message) { log(LogLevel::Warning, message); }
    void error(const std::string& message) { log(LogLevel::Error, message); }
    void fatal(const std::string& message) { log(LogLevel::Fatal, message); }

private:
    Logger() = default;
    ~Logger() = default;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel log_level_ = LogLevel::Info;
};

} // namespace kera