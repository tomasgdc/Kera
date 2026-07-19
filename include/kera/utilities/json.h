// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace kera
{

    class JsonValue
    {
    public:
        using Array = std::vector<JsonValue>;
        using Object = std::map<std::string, JsonValue>;

        JsonValue() = default;
        JsonValue(std::nullptr_t) : m_value(nullptr) {}
        JsonValue(bool value) : m_value(value) {}
        JsonValue(double value) : m_value(value) {}
        JsonValue(const std::string& value) : m_value(value) {}
        JsonValue(std::string&& value) : m_value(std::move(value)) {}
        JsonValue(const char* value) : m_value(std::string(value)) {}
        JsonValue(const Array& value) : m_value(value) {}
        JsonValue(Array&& value) : m_value(std::move(value)) {}
        JsonValue(const Object& value) : m_value(value) {}
        JsonValue(Object&& value) : m_value(std::move(value)) {}

        bool isNull() const
        {
            return std::holds_alternative<std::nullptr_t>(m_value);
        }
        bool isBool() const
        {
            return std::holds_alternative<bool>(m_value);
        }
        bool isNumber() const
        {
            return std::holds_alternative<double>(m_value);
        }
        bool isString() const
        {
            return std::holds_alternative<std::string>(m_value);
        }
        bool isArray() const
        {
            return std::holds_alternative<Array>(m_value);
        }
        bool isObject() const
        {
            return std::holds_alternative<Object>(m_value);
        }

        bool asBool() const
        {
            return std::get<bool>(m_value);
        }
        double asNumber() const
        {
            return std::get<double>(m_value);
        }
        const std::string& asString() const
        {
            return std::get<std::string>(m_value);
        }
        const Array& asArray() const
        {
            return std::get<Array>(m_value);
        }
        const Object& asObject() const
        {
            return std::get<Object>(m_value);
        }

        const JsonValue* getObjectItem(const std::string& key) const
        {
            if (!isObject())
            {
                return nullptr;
            }
            const auto& object = std::get<Object>(m_value);
            auto it = object.find(key);
            return it != object.end() ? &it->second : nullptr;
        }

    private:
        std::variant<std::nullptr_t, bool, double, std::string, Array, Object> m_value;
    };

    class Json
    {
    public:
        // Returns true on success, false on failure (logs error internally)
        static bool parse(const std::string& text, JsonValue& out_value);
    };

}  // namespace kera
