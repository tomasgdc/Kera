#include "kera/utilities/json.h"
#include "kera/utilities/logger.h"
#include <charconv>
#include <cctype>
#include <cmath>

namespace kera {

namespace {

void skipWhitespace(const std::string& text, size_t& offset) {
    while (offset < text.size() && std::isspace(static_cast<unsigned char>(text[offset]))) {
        ++offset;
    }
}

bool parseString(const std::string& text, size_t& offset, std::string& outValue) {
    if (offset >= text.size() || text[offset] != '"') {
        Logger::getInstance().error("Expected JSON string");
        return false;
    }

    ++offset;
    outValue.clear();

    while (offset < text.size()) {
        char current = text[offset++];
        if (current == '"') {
            return true;
        }

        if (current == '\\') {
            if (offset >= text.size()) {
                break;
            }

            char escape = text[offset++];
            switch (escape) {
                case '"': outValue.push_back('"'); break;
                case '\\': outValue.push_back('\\'); break;
                case '/': outValue.push_back('/'); break;
                case 'b': outValue.push_back('\b'); break;
                case 'f': outValue.push_back('\f'); break;
                case 'n': outValue.push_back('\n'); break;
                case 'r': outValue.push_back('\r'); break;
                case 't': outValue.push_back('\t'); break;
                default:
                    Logger::getInstance().error("Unsupported JSON escape sequence");
                    return false;
            }
        } else {
            outValue.push_back(current);
        }
    }

    Logger::getInstance().error("Unterminated JSON string");
    return false;
}

bool parseValue(const std::string& text, size_t& offset, JsonValue& outValue);

bool parseArray(const std::string& text, size_t& offset, JsonValue& outValue) {
    if (offset >= text.size() || text[offset] != '[') {
        Logger::getInstance().error("Expected '[' to begin JSON array");
        return false;
    }

    ++offset;
    skipWhitespace(text, offset);
    JsonValue::Array elements;

    if (offset < text.size() && text[offset] == ']') {
        ++offset;
        outValue = JsonValue(std::move(elements));
        return true;
    }

    while (true) {
        JsonValue element;
        if (!parseValue(text, offset, element)) {
            return false;
        }
        elements.push_back(std::move(element));

        skipWhitespace(text, offset);
        if (offset >= text.size()) {
            Logger::getInstance().error("Unterminated JSON array");
            return false;
        }

        if (text[offset] == ']') {
            ++offset;
            break;
        }

        if (text[offset] != ',') {
            Logger::getInstance().error("Expected ',' between JSON array values");
            return false;
        }

        ++offset;
        skipWhitespace(text, offset);
    }

    outValue = JsonValue(std::move(elements));
    return true;
}

bool parseObject(const std::string& text, size_t& offset, JsonValue& outValue) {
    if (offset >= text.size() || text[offset] != '{') {
        Logger::getInstance().error("Expected '{' to begin JSON object");
        return false;
    }

    ++offset;
    skipWhitespace(text, offset);
    JsonValue::Object members;

    if (offset < text.size() && text[offset] == '}') {
        ++offset;
        outValue = JsonValue(std::move(members));
        return true;
    }

    while (true) {
        skipWhitespace(text, offset);
        std::string key;
        if (!parseString(text, offset, key)) {
            return false;
        }

        skipWhitespace(text, offset);
        if (offset >= text.size() || text[offset] != ':') {
            Logger::getInstance().error("Expected ':' after JSON object key");
            return false;
        }

        ++offset;
        skipWhitespace(text, offset);
        JsonValue value;
        if (!parseValue(text, offset, value)) {
            return false;
        }

        members.emplace(std::move(key), std::move(value));
        skipWhitespace(text, offset);

        if (offset >= text.size()) {
            Logger::getInstance().error("Unterminated JSON object");
            return false;
        }

        if (text[offset] == '}') {
            ++offset;
            break;
        }

        if (text[offset] != ',') {
            Logger::getInstance().error("Expected ',' between JSON object members");
            return false;
        }

        ++offset;
        skipWhitespace(text, offset);
    }

    outValue = JsonValue(std::move(members));
    return true;
}

bool parseNumber(const std::string& text, size_t& offset, JsonValue& outValue) {
    const size_t start = offset;
    if (offset < text.size() && (text[offset] == '-' || text[offset] == '+')) {
        ++offset;
    }

    while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset]))) {
        ++offset;
    }

    if (offset < text.size() && text[offset] == '.') {
        ++offset;
        while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset]))) {
            ++offset;
        }
    }

    if (offset < text.size() && (text[offset] == 'e' || text[offset] == 'E')) {
        ++offset;
        if (offset < text.size() && (text[offset] == '+' || text[offset] == '-')) {
            ++offset;
        }
        while (offset < text.size() && std::isdigit(static_cast<unsigned char>(text[offset]))) {
            ++offset;
        }
    }

    if (start == offset) {
        Logger::getInstance().error("Invalid JSON number");
        return false;
    }

    const std::string numberText = text.substr(start, offset - start);
    try {
        double value = std::stod(numberText);
        outValue = JsonValue(value);
        return true;
    } catch (const std::exception&) {
        Logger::getInstance().error("Failed to parse JSON number");
        return false;
    }
}

bool parseLiteral(const std::string& text, size_t& offset, JsonValue& outValue) {
    if (text.compare(offset, 4, "null") == 0) {
        offset += 4;
        outValue = JsonValue(nullptr);
        return true;
    }
    if (text.compare(offset, 4, "true") == 0) {
        offset += 4;
        outValue = JsonValue(true);
        return true;
    }
    if (text.compare(offset, 5, "false") == 0) {
        offset += 5;
        outValue = JsonValue(false);
        return true;
    }
    Logger::getInstance().error("Invalid JSON literal");
    return false;
}

bool parseValue(const std::string& text, size_t& offset, JsonValue& outValue) {
    skipWhitespace(text, offset);
    if (offset >= text.size()) {
        Logger::getInstance().error("Unexpected end of JSON input");
        return false;
    }

    switch (text[offset]) {
        case '{':
            return parseObject(text, offset, outValue);
        case '[':
            return parseArray(text, offset, outValue);
        case '"':
            {
                std::string str;
                if (!parseString(text, offset, str)) {
                    return false;
                }
                outValue = JsonValue(std::move(str));
                return true;
            }
        case 't':
        case 'f':
        case 'n':
            return parseLiteral(text, offset, outValue);
        default:
            return parseNumber(text, offset, outValue);
    }
}

} // namespace

bool Json::parse(const std::string& text, JsonValue& outValue) {
    size_t offset = 0;
    skipWhitespace(text, offset);
    if (!parseValue(text, offset, outValue)) {
        return false;
    }

    skipWhitespace(text, offset);
    if (offset != text.size()) {
        Logger::getInstance().error("Unexpected characters after JSON value");
        return false;
    }

    return true;
}

} // namespace kera
