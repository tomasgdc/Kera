#include "kera/utilities/logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace kera
{
    Logger& Logger::getInstance()
    {
        static Logger instance;
        return instance;
    }

    void Logger::log(LogLevel level, const std::string& message) 
    {
        if (level < log_level_) 
        {
            return;
        }

        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        struct tm timeinfo;
        errno_t err = localtime_s(&timeinfo, &time);

        std::stringstream ss;
        if (err == 0) 
        {
            ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
        }
        else 
        {
            ss << "Unknown Time";
        }

        ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count();

        // Level string
        std::string levelStr;
        switch (level) 
        {
            case LogLevel::Debug: levelStr = "DEBUG"; break;
            case LogLevel::Info: levelStr = "INFO"; break;
            case LogLevel::Warning: levelStr = "WARN"; break;
            case LogLevel::Error: levelStr = "ERROR"; break;
            case LogLevel::Fatal: levelStr = "FATAL"; break;
        }

        // Output stream
        std::ostream& out = (level >= LogLevel::Error) ? std::cerr : std::cout;
        out << "[" << ss.str() << "] [" << levelStr << "] " << message << std::endl;

        // For fatal errors, also abort
        if (level == LogLevel::Fatal)
        {
            std::abort();
        }
    }
} // namespace kera