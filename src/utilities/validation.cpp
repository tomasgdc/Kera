// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/utilities/validation.h"

#include "kera/utilities/logger.h"

namespace kera
{
    Validation::AssertHandler Validation::g_assertHandler = nullptr;

    void Validation::setAssertHandler(AssertHandler handler)
    {
        g_assertHandler = handler;
    }

    void Validation::assertCondition(bool condition, const std::string& condition_str, const std::string& message,
                                     const char* file, int line)
    {
        if (!condition)
        {
            std::string full_message = "Assertion failed: " + condition_str;
            if (!message.empty())
            {
                full_message += " (" + message + ")";
            }

            if (file && line > 0)
            {
                full_message += " at " + std::string(file) + ":" + std::to_string(line);
            }

            if (g_assertHandler)
            {
                g_assertHandler(condition_str, message, file, line);
            }

            Logger::getInstance().fatal(full_message);
        }
    }
}  // namespace kera
