#include "kera/utilities/validation.h"
#include "kera/utilities/logger.h"
#include <iostream>
#include <cstdlib>

namespace kera 
{
    Validation::AssertHandler Validation::assert_handler_ = nullptr;

    void Validation::setAssertHandler(AssertHandler handler)
    {
        assert_handler_ = handler;
    }

    void Validation::assertCondition(bool condition, const std::string& conditionStr, const std::string& message, const char* file, int line) {
        if (!condition)
        {
            std::string fullMessage = "Assertion failed: " + conditionStr;
            if (!message.empty())
            {
                fullMessage += " (" + message + ")";
            }

            if (file && line > 0)
            {
                fullMessage += " at " + std::string(file) + ":" + std::to_string(line);
            }

            Logger::getInstance().fatal(fullMessage);

            if (assert_handler_)
            {
                assert_handler_(conditionStr, message, file, line);
            }
            else
            {
                // Default behavior: print and abort
                std::cerr << fullMessage << std::endl;
                std::abort();
            }
        }
    }
} // namespace kera