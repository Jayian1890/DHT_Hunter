#include "dht_hunter/utility/json/json.hpp"
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cmath>

namespace dht_hunter::utility::json {

// Create a new JSON object
JsonValue::ObjectType JsonValue::createObject() {
    return std::make_shared<JsonObject>();
}

// Create a new JSON array
JsonValue::ArrayType JsonValue::createArray() {
    return std::make_shared<JsonArray>();
}

// Convert the value to a string representation
std::string JsonValue::toString(bool pretty) const {
    std::ostringstream ss;

    switch (getType()) {
        case Type::Null:
            ss << "null";
            break;
        case Type::Boolean:
            ss << (getBoolean() ? "true" : "false");
            break;
        case Type::Number: {
            auto value = getNumber();
            // Check if the number is an integer
            if (std::floor(value) == value) {
                ss << static_cast<long long>(value);
            } else {
                ss << std::fixed << std::setprecision(10) << value;
                // Remove trailing zeros
                std::string str = ss.str();
                size_t pos = str.find_last_not_of('0');
                if (pos != std::string::npos && str[pos] == '.') {
                    pos--;
                }
                return str.substr(0, pos + 1);
            }
            break;
        }
        case Type::String:
            ss << '"';
            for (char c : getString()) {
                switch (c) {
                    case '"': ss << "\\\""; break;
                    case '\\': ss << "\\\\"; break;
                    case '\b': ss << "\\b"; break;
                    case '\f': ss << "\\f"; break;
                    case '\n': ss << "\\n"; break;
                    case '\r': ss << "\\r"; break;
                    case '\t': ss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 32) {
                            ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                        } else {
                            ss << c;
                        }
                }
            }
            ss << '"';
            break;
        case Type::Object: {
            const auto& obj = getObject();
            ss << '{';

            bool first = true;
            for (const auto& [key, value] : obj->getMap()) {
                if (!first) {
                    ss << ',';
                }
                if (pretty) {
                    ss << '\n' << std::string(4, ' ');
                }
                ss << '"' << key << "\":";
                if (pretty) {
                    ss << ' ';
                }
                ss << value.toString(pretty);
                first = false;
            }

            if (pretty && !obj->getMap().empty()) {
                ss << '\n';
            }
            ss << '}';
            break;
        }
        case Type::Array: {
            const auto& arr = getArray();
            ss << '[';

            bool first = true;
            for (const auto& value : arr->getVector()) {
                if (!first) {
                    ss << ',';
                }
                if (pretty) {
                    ss << '\n' << std::string(4, ' ');
                }
                ss << value.toString(pretty);
                first = false;
            }

            if (pretty && !arr->getVector().empty()) {
                ss << '\n';
            }
            ss << ']';
            break;
        }
    }

    return ss.str();
}

// Helper function to skip whitespace in JSON parsing
static void skipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(json[pos])) {
        pos++;
    }
}

// Helper function to parse a JSON string
static std::string parseString(const std::string& json, size_t& pos) {
    if (json[pos] != '"') {
        throw std::runtime_error("Expected '\"' at position " + std::to_string(pos));
    }

    pos++; // Skip opening quote
    std::string result;

    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\') {
            pos++; // Skip backslash
            if (pos >= json.size()) {
                throw std::runtime_error("Unexpected end of string");
            }

            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    if (pos + 4 >= json.size()) {
                        throw std::runtime_error("Unexpected end of unicode escape sequence");
                    }

                    std::string hex = json.substr(pos + 1, 4);
                    pos += 4; // Skip the 4 hex digits

                    // Convert hex to int
                    int codePoint = std::stoi(hex, nullptr, 16);

                    // Handle UTF-16 surrogate pairs
                    if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
                        // High surrogate, need to read the low surrogate
                        if (pos + 6 >= json.size() || json[pos + 1] != '\\' || json[pos + 2] != 'u') {
                            throw std::runtime_error("Expected low surrogate after high surrogate");
                        }

                        pos += 3; // Skip \u
                        std::string lowHex = json.substr(pos, 4);
                        pos += 3; // Skip the 4 hex digits (the loop will increment pos again)

                        int lowCodePoint = std::stoi(lowHex, nullptr, 16);
                        if (lowCodePoint < 0xDC00 || lowCodePoint > 0xDFFF) {
                            throw std::runtime_error("Invalid low surrogate");
                        }

                        // Combine the surrogate pair
                        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (lowCodePoint - 0xDC00);
                    }

                    // Convert code point to UTF-8
                    if (codePoint <= 0x7F) {
                        result += static_cast<char>(codePoint);
                    } else if (codePoint <= 0x7FF) {
                        result += static_cast<char>(0xC0 | (codePoint >> 6));
                        result += static_cast<char>(0x80 | (codePoint & 0x3F));
                    } else if (codePoint <= 0xFFFF) {
                        result += static_cast<char>(0xE0 | (codePoint >> 12));
                        result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codePoint & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (codePoint >> 18));
                        result += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (codePoint & 0x3F));
                    }
                    break;
                }
                default:
                    throw std::runtime_error("Invalid escape sequence: \\" + std::string(1, json[pos]));
            }
        } else {
            result += json[pos];
        }

        pos++;
    }

    if (pos >= json.size() || json[pos] != '"') {
        throw std::runtime_error("Unterminated string");
    }

    pos++; // Skip closing quote
    return result;
}

// Helper function to parse a JSON number
static double parseNumber(const std::string& json, size_t& pos) {
    size_t start = pos;

    // Handle sign
    if (json[pos] == '-') {
        pos++;
    }

    // Handle integer part
    if (json[pos] == '0') {
        pos++;
    } else if (std::isdigit(json[pos])) {
        pos++;
        while (pos < json.size() && std::isdigit(json[pos])) {
            pos++;
        }
    } else {
        throw std::runtime_error("Expected digit at position " + std::to_string(pos));
    }

    // Handle decimal part
    if (pos < json.size() && json[pos] == '.') {
        pos++;
        if (pos >= json.size() || !std::isdigit(json[pos])) {
            throw std::runtime_error("Expected digit after decimal point");
        }
        while (pos < json.size() && std::isdigit(json[pos])) {
            pos++;
        }
    }

    // Handle exponent
    if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
        pos++;
        if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) {
            pos++;
        }
        if (pos >= json.size() || !std::isdigit(json[pos])) {
            throw std::runtime_error("Expected digit in exponent");
        }
        while (pos < json.size() && std::isdigit(json[pos])) {
            pos++;
        }
    }

    // Convert the substring to a double
    return std::stod(json.substr(start, pos - start));
}

// Forward declaration for mutual recursion
static JsonValue parseValue(const std::string& json, size_t& pos);

// Helper function to parse a JSON object
static JsonValue::ObjectType parseObject(const std::string& json, size_t& pos) {
    if (json[pos] != '{') {
        throw std::runtime_error("Expected '{' at position " + std::to_string(pos));
    }

    pos++; // Skip opening brace
    skipWhitespace(json, pos);

    auto obj = JsonValue::createObject();

    if (pos < json.size() && json[pos] == '}') {
        pos++; // Skip closing brace for empty object
        return obj;
    }

    while (pos < json.size()) {
        skipWhitespace(json, pos);

        // Parse key
        std::string key = parseString(json, pos);

        skipWhitespace(json, pos);

        if (pos >= json.size() || json[pos] != ':') {
            throw std::runtime_error("Expected ':' after key in object");
        }

        pos++; // Skip colon
        skipWhitespace(json, pos);

        // Parse value
        JsonValue value = parseValue(json, pos);

        // Add key-value pair to object
        obj->set(key, value);

        skipWhitespace(json, pos);

        if (pos >= json.size()) {
            throw std::runtime_error("Unterminated object");
        }

        if (json[pos] == '}') {
            pos++; // Skip closing brace
            return obj;
        }

        if (json[pos] != ',') {
            throw std::runtime_error("Expected ',' or '}' in object");
        }

        pos++; // Skip comma
    }

    throw std::runtime_error("Unterminated object");
}

// Helper function to parse a JSON array
static JsonValue::ArrayType parseArray(const std::string& json, size_t& pos) {
    if (json[pos] != '[') {
        throw std::runtime_error("Expected '[' at position " + std::to_string(pos));
    }

    pos++; // Skip opening bracket
    skipWhitespace(json, pos);

    auto arr = JsonValue::createArray();

    if (pos < json.size() && json[pos] == ']') {
        pos++; // Skip closing bracket for empty array
        return arr;
    }

    while (pos < json.size()) {
        // Parse value
        JsonValue value = parseValue(json, pos);

        // Add value to array
        arr->add(value);

        skipWhitespace(json, pos);

        if (pos >= json.size()) {
            throw std::runtime_error("Unterminated array");
        }

        if (json[pos] == ']') {
            pos++; // Skip closing bracket
            return arr;
        }

        if (json[pos] != ',') {
            throw std::runtime_error("Expected ',' or ']' in array");
        }

        pos++; // Skip comma
        skipWhitespace(json, pos);
    }

    throw std::runtime_error("Unterminated array");
}

// Helper function to parse a JSON value
static JsonValue parseValue(const std::string& json, size_t& pos) {
    skipWhitespace(json, pos);

    if (pos >= json.size()) {
        throw std::runtime_error("Unexpected end of input");
    }

    switch (json[pos]) {
        case 'n': // null
            if (pos + 3 < json.size() && json.substr(pos, 4) == "null") {
                pos += 4;
                return JsonValue(nullptr);
            }
            throw std::runtime_error("Invalid token at position " + std::to_string(pos));

        case 't': // true
            if (pos + 3 < json.size() && json.substr(pos, 4) == "true") {
                pos += 4;
                return JsonValue(true);
            }
            throw std::runtime_error("Invalid token at position " + std::to_string(pos));

        case 'f': // false
            if (pos + 4 < json.size() && json.substr(pos, 5) == "false") {
                pos += 5;
                return JsonValue(false);
            }
            throw std::runtime_error("Invalid token at position " + std::to_string(pos));

        case '"': // string
            return JsonValue(parseString(json, pos));

        case '{': // object
            return JsonValue(parseObject(json, pos));

        case '[': // array
            return JsonValue(parseArray(json, pos));

        case '-': // number
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return JsonValue(parseNumber(json, pos));

        default:
            throw std::runtime_error("Invalid token at position " + std::to_string(pos));
    }
}

// Parse a JSON string
JsonValue JsonValue::parse(const std::string& json) {
    size_t pos = 0;
    JsonValue result = parseValue(json, pos);

    skipWhitespace(json, pos);
    if (pos < json.size()) {
        throw std::runtime_error("Unexpected data after JSON value");
    }

    return result;
}

} // namespace dht_hunter::utility::json
