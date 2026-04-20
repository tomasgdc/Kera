#pragma once

#include <string>
#include <optional>
#include <variant>

namespace kera {

// Result type for operations that can succeed or fail
template<typename T>
class Result {
public:
    // Constructors
    Result() = default;
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const std::string& error) : data_(error) {}
    Result(std::string&& error) : data_(std::move(error)) {}

    // Check if result contains a value or error
    bool hasValue() const { return std::holds_alternative<T>(data_); }
    bool hasError() const { return std::holds_alternative<std::string>(data_); }

    // Get the value (undefined behavior if hasError())
    const T& value() const { return std::get<T>(data_); }
    T& value() { return std::get<T>(data_); }

    // Get the error message (undefined behavior if hasValue())
    const std::string& error() const { return std::get<std::string>(data_); }

    // Safe accessors
    std::optional<T> getValue() const {
        if (hasValue()) return std::get<T>(data_);
        return std::nullopt;
    }

    std::optional<std::string> getError() const {
        if (hasError()) return std::get<std::string>(data_);
        return std::nullopt;
    }

private:
    std::variant<T, std::string> data_;
};

// Specialization for void operations
template<>
class Result<void> {
public:
    Result() = default;
    Result(const std::string& error) : error_(error) {}

    bool hasValue() const { return !error_.has_value(); }
    bool hasError() const { return error_.has_value(); }

    const std::string& error() const { return error_.value(); }

    std::optional<std::string> getError() const { return error_; }

private:
    std::optional<std::string> error_;
};

// Helper functions for creating results
template<typename T>
Result<T> success(const T& value) {
    return Result<T>(value);
}

template<typename T>
Result<T> success(T&& value) {
    return Result<T>(std::move(value));
}

inline Result<void> success() {
    return Result<void>();
}

template<typename T>
Result<T> failure(const std::string& error) {
    return Result<T>(error);
}

inline Result<void> failure(const std::string& error) {
    return Result<void>(error);
}

} // namespace kera