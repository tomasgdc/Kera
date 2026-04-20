#pragma once

#include <string>
#include <functional>

namespace kera {

class Validation {
public:
    using AssertHandler = std::function<void(const std::string& condition, const std::string& message, const char* file, int line)>;

    static void setAssertHandler(AssertHandler handler);
    static void assertCondition(bool condition, const std::string& conditionStr, const std::string& message = "", const char* file = "", int line = 0);

    // Validation macros (defined in cpp file)
    #define KERA_ASSERT(condition) kera::Validation::assertCondition(condition, #condition, "", __FILE__, __LINE__)
    #define KERA_ASSERT_MSG(condition, message) kera::Validation::assertCondition(condition, #condition, message, __FILE__, __LINE__)

private:
    static AssertHandler assert_handler_;
};

} // namespace kera