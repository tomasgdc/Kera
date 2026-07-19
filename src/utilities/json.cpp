// Copyright 2026 Tomas Mikalauskas
// SPDX-License-Identifier: Apache-2.0

#include "kera/utilities/json.h"

#include "kera/utilities/logger.h"

#include <cctype>
#include <charconv>
#include <cmath>
#include <system_error>

namespace kera
{

    namespace
    {

        void skipWhitespace(const std::string& text, size_t& offset)
        {
            while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset])))
            {
                ++offset;
            }
        }

        bool parseString(const std::string& text, size_t& offset, std::string& out_value)
        {
            if (offset >= text.size() || text[offset] != '"')
            {
                Logger::getInstance().error("Expected JSON string");
                return false;
            }

            ++offset;
            out_value.clear();

            while (offset < text.size())
            {
                char current = text[offset++];
                if (current == '"')
                {
                    return true;
                }

                if (current == '\\')
                {
                    if (offset >= text.size())
                    {
                        break;
                    }

                    char escape = text[offset++];
                    switch (escape)
                    {
                        case '"':
                            out_value.push_back('"');
                            break;
                        case '\\':
                            out_value.push_back('\\');
                            break;
                        case '/':
                            out_value.push_back('/');
                            break;
                        case 'b':
                            out_value.push_back('\b');
                            break;
                        case 'f':
                            out_value.push_back('\f');
                            break;
                        case 'n':
                            out_value.push_back('\n');
                            break;
                        case 'r':
                            out_value.push_back('\r');
                            break;
                        case 't':
                            out_value.push_back('\t');
                            break;
                        default:
                            Logger::getInstance().error("Unsupported JSON escape sequence");
                            return false;
                    }
                }
                else
                {
                    out_value.push_back(current);
                }
            }

            Logger::getInstance().error("Unterminated JSON string");
            return false;
        }

        bool parseValue(const std::string& text, size_t& offset, JsonValue& out_value);

        bool parseArray(const std::string& text, size_t& offset, JsonValue& out_value)
        {
            if (offset >= text.size() || text[offset] != '[')
            {
                Logger::getInstance().error("Expected '[' to begin JSON array");
                return false;
            }

            ++offset;
            skipWhitespace(text, offset);
            JsonValue::Array elements;

            if (offset < text.size() && text[offset] == ']')
            {
                ++offset;
                out_value = JsonValue(std::move(elements));
                return true;
            }

            while (true)
            {
                JsonValue element;
                if (!parseValue(text, offset, element))
                {
                    return false;
                }

                elements.push_back(std::move(element));
                skipWhitespace(text, offset);

                if (offset >= text.size())
                {
                    Logger::getInstance().error("Unterminated JSON array");
                    return false;
                }

                if (text[offset] == ']')
                {
                    ++offset;
                    break;
                }

                if (text[offset] != ',')
                {
                    Logger::getInstance().error("Expected ',' between JSON array values");
                    return false;
                }

                ++offset;
                skipWhitespace(text, offset);
            }

            out_value = JsonValue(std::move(elements));
            return true;
        }

        bool parseObject(const std::string& text, size_t& offset, JsonValue& out_value)
        {
            if (offset >= text.size() || text[offset] != '{')
            {
                Logger::getInstance().error("Expected '{' to begin JSON object");
                return false;
            }

            ++offset;
            skipWhitespace(text, offset);
            JsonValue::Object members;

            if (offset < text.size() && text[offset] == '}')
            {
                ++offset;
                out_value = JsonValue(std::move(members));
                return true;
            }

            while (true)
            {
                skipWhitespace(text, offset);
                std::string key;
                if (!parseString(text, offset, key))
                {
                    return false;
                }

                skipWhitespace(text, offset);
                if (offset >= text.size() || text[offset] != ':')
                {
                    Logger::getInstance().error("Expected ':' after JSON object key");
                    return false;
                }

                ++offset;
                skipWhitespace(text, offset);
                JsonValue value;
                if (!parseValue(text, offset, value))
                {
                    return false;
                }

                members.emplace(std::move(key), std::move(value));
                skipWhitespace(text, offset);

                if (offset >= text.size())
                {
                    Logger::getInstance().error("Unterminated JSON object");
                    return false;
                }

                if (text[offset] == '}')
                {
                    ++offset;
                    break;
                }

                if (text[offset] != ',')
                {
                    Logger::getInstance().error("Expected ',' between JSON object members");
                    return false;
                }

                ++offset;
                skipWhitespace(text, offset);
            }

            out_value = JsonValue(std::move(members));
            return true;
        }

        bool parseNumber(const std::string& text, size_t& offset, JsonValue& out_value)
        {
            const size_t start = offset;
            if (offset < text.size() && (text[offset] == '-' || text[offset] == '+'))
            {
                ++offset;
            }

            while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset])))
            {
                ++offset;
            }

            if (offset < text.size() && text[offset] == '.')
            {
                ++offset;
                while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset])))
                {
                    ++offset;
                }
            }

            if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E'))
            {
                ++offset;
                if (offset < text.size() && (text[offset] == '+' || text[offset] == '-'))
                {
                    ++offset;
                }

                while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset])))
                {
                    ++offset;
                }
            }

            if (start == offset)
            {
                Logger::getInstance().error("Invalid JSON number");
                return false;
            }

            const std::string number_text = text.substr(start, offset - start);
            double value = 0.0;
            const char* number_begin = number_text.data();
            const char* number_end = number_begin + number_text.size();
            const std::from_chars_result result = std::from_chars(number_begin, number_end, value);
            if (result.ec != std::errc{} || result.ptr != number_end)
            {
                Logger::getInstance().error("Failed to parse JSON number");
                return false;
            }
            out_value = JsonValue(value);
            return true;
        }

        bool parseLiteral(const std::string& text, size_t& offset, JsonValue& out_value)
        {
            if (text.compare(offset, 4, "null") == 0)
            {
                offset += 4;
                out_value = JsonValue(nullptr);
                return true;
            }

            if (text.compare(offset, 4, "true") == 0)
            {
                offset += 4;
                out_value = JsonValue(true);
                return true;
            }

            if (text.compare(offset, 5, "false") == 0)
            {
                offset += 5;
                out_value = JsonValue(false);
                return true;
            }

            Logger::getInstance().error("Invalid JSON literal");
            return false;
        }

        bool parseValue(const std::string& text, size_t& offset, JsonValue& out_value)
        {
            skipWhitespace(text, offset);

            if (offset >= text.size())
            {
                Logger::getInstance().error("Unexpected end of JSON input");
                return false;
            }

            switch (text[offset])
            {
                case '{':
                    return parseObject(text, offset, out_value);
                case '[':
                    return parseArray(text, offset, out_value);
                case '"':
                {
                    std::string str;
                    if (!parseString(text, offset, str))
                    {
                        return false;
                    }
                    out_value = JsonValue(std::move(str));
                    return true;
                }
                case 't':
                case 'f':
                case 'n':
                    return parseLiteral(text, offset, out_value);
                default:
                    return parseNumber(text, offset, out_value);
            }
        }

    }  // namespace

    bool Json::parse(const std::string& text, JsonValue& out_value)
    {
        size_t offset = 0;
        skipWhitespace(text, offset);
        if (!parseValue(text, offset, out_value))
        {
            return false;
        }

        skipWhitespace(text, offset);
        if (offset != text.size())
        {
            Logger::getInstance().error("Unexpected characters after JSON value");
            return false;
        }

        return true;
    }
}  // namespace kera
