#pragma once

#include <map>
#include <string>
#include <vector>
#include <variant>

namespace kera {

class JsonValue {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue>;

    JsonValue() = default;
    JsonValue(std::nullptr_t) : value_(nullptr) {}
    JsonValue(bool value) : value_(value) {}
    JsonValue(double value) : value_(value) {}
    JsonValue(const std::string& value) : value_(value) {}
    JsonValue(std::string&& value) : value_(std::move(value)) {}
    JsonValue(const char* value) : value_(std::string(value)) {}
    JsonValue(const Array& value) : value_(value) {}
    JsonValue(Array&& value) : value_(std::move(value)) {}
    JsonValue(const Object& value) : value_(value) {}
    JsonValue(Object&& value) : value_(std::move(value)) {}

    bool isNull() const { return std::holds_alternative<std::nullptr_t>(value_); }
    bool isBool() const { return std::holds_alternative<bool>(value_); }
    bool isNumber() const { return std::holds_alternative<double>(value_); }
    bool isString() const { return std::holds_alternative<std::string>(value_); }
    bool isArray() const { return std::holds_alternative<Array>(value_); }
    bool isObject() const { return std::holds_alternative<Object>(value_); }

    bool asBool() const { return std::get<bool>(value_); }
    double asNumber() const { return std::get<double>(value_); }
    const std::string& asString() const { return std::get<std::string>(value_); }
    const Array& asArray() const { return std::get<Array>(value_); }
    const Object& asObject() const { return std::get<Object>(value_); }

    const JsonValue* getObjectItem(const std::string& key) const {
        if (!isObject()) {
            return nullptr;
        }
        const auto& object = std::get<Object>(value_);
        auto it = object.find(key);
        return it != object.end() ? &it->second : nullptr;
    }

private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_;
};

class Json {
public:
    // Returns true on success, false on failure (logs error internally)
    static bool parse(const std::string& text, JsonValue& outValue);
};

} // namespace kera
