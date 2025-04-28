#include "dht_hunter/utility/json/json_utility.hpp"
#include <iomanip>
#include <sstream>

namespace dht_hunter::utility::json {

// JsonValue::toString implementation
std::string JsonValue::toString(int indent) const {
    std::ostringstream ss;

    switch (getType()) {
        case Type::Null:
            ss << "null";
            break;

        case Type::Boolean:
            ss << (getBool() ? "true" : "false");
            break;

        case Type::Number:
            ss << std::fixed << std::setprecision(10) << getNumber();
            // Remove trailing zeros
            {
                std::string str = ss.str();
                if (str.find('.') != std::string::npos) {
                    str.erase(str.find_last_not_of('0') + 1, std::string::npos);
                    if (str.back() == '.') {
                        str.pop_back();
                    }
                }
                return str;
            }

        case Type::String:
            ss << '"';
            for (char c : getString()) {
                switch (c) {
                    case '\"': ss << "\\\""; break;
                    case '\\': ss << "\\\\"; break;
                    case '/': ss << "\\/"; break;
                    case '\b': ss << "\\b"; break;
                    case '\f': ss << "\\f"; break;
                    case '\n': ss << "\\n"; break;
                    case '\r': ss << "\\r"; break;
                    case '\t': ss << "\\t"; break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                        } else {
                            ss << c;
                        }
                }
            }
            ss << '"';
            break;

        case Type::Array: {
            const auto& array = getArray();
            if (array.size() == 0) {
                ss << "[]";
                break;
            }

            ss << "[";
            if (indent > 0) {
                ss << "\n";
            }

            for (size_t i = 0; i < array.size(); ++i) {
                if (indent > 0) {
                    ss << std::string(static_cast<size_t>(indent + 2), ' ');
                }

                ss << array[i].toString(indent > 0 ? indent + 2 : 0);

                if (i < array.size() - 1) {
                    ss << ",";
                }

                if (indent > 0) {
                    ss << "\n";
                }
            }

            if (indent > 0) {
                ss << std::string(static_cast<size_t>(indent), ' ');
            }
            ss << "]";
            break;
        }

        case Type::Object: {
            const auto& object = getObject();
            if (object.size() == 0) {
                ss << "{}";
                break;
            }

            ss << "{";
            if (indent > 0) {
                ss << "\n";
            }

            size_t i = 0;
            for (const auto& [key, value] : object) {
                if (indent > 0) {
                    ss << std::string(static_cast<size_t>(indent + 2), ' ');
                }

                ss << "\"" << key << "\":";
                if (indent > 0) {
                    ss << " ";
                }

                ss << value.toString(indent > 0 ? indent + 2 : 0);

                if (i < object.size() - 1) {
                    ss << ",";
                }

                if (indent > 0) {
                    ss << "\n";
                }

                ++i;
            }

            if (indent > 0) {
                ss << std::string(static_cast<size_t>(indent), ' ');
            }
            ss << "}";
            break;
        }
    }

    return ss.str();
}

// JsonParser implementation
JsonValue JsonParser::parse(const std::string& json) {
    std::istringstream stream(json);
    skipWhitespace(stream);

    JsonValue value = parseValue(stream);

    skipWhitespace(stream);
    if (!stream.eof()) {
        throw std::runtime_error("Unexpected data after JSON value");
    }

    return value;
}

JsonValue JsonParser::parseValue(std::istringstream& stream) {
    skipWhitespace(stream);

    char c = peek(stream);

    if (c == '{') {
        return JsonValue(parseObject(stream));
    } else if (c == '[') {
        return JsonValue(parseArray(stream));
    } else if (c == '"') {
        return JsonValue(parseString(stream));
    } else if (c == '-' || isDigit(c)) {
        return parseNumber(stream);
    } else if (c == 't' || c == 'f' || c == 'n') {
        return parseKeyword(stream);
    } else {
        throw std::runtime_error(std::string("Unexpected character: ") + c);
    }
}

JsonValue::JsonObject JsonParser::parseObject(std::istringstream& stream) {
    JsonValue::JsonObject object;

    // Consume the opening brace
    get(stream);

    skipWhitespace(stream);

    // Check for empty object
    if (peek(stream) == '}') {
        get(stream);
        return object;
    }

    while (true) {
        skipWhitespace(stream);

        // Parse the key
        if (peek(stream) != '"') {
            throw std::runtime_error("Expected string key in object");
        }

        std::string key = parseString(stream);

        skipWhitespace(stream);

        // Consume the colon
        if (get(stream) != ':') {
            throw std::runtime_error("Expected ':' after key in object");
        }

        skipWhitespace(stream);

        // Parse the value
        JsonValue value = parseValue(stream);

        // Add the key-value pair to the object
        object.set(key, value);

        skipWhitespace(stream);

        // Check for end of object or comma
        char c = get(stream);
        if (c == '}') {
            break;
        } else if (c != ',') {
            throw std::runtime_error("Expected ',' or '}' after value in object");
        }
    }

    return object;
}

JsonValue::JsonArray JsonParser::parseArray(std::istringstream& stream) {
    JsonValue::JsonArray array;

    // Consume the opening bracket
    get(stream);

    skipWhitespace(stream);

    // Check for empty array
    if (peek(stream) == ']') {
        get(stream);
        return array;
    }

    while (true) {
        skipWhitespace(stream);

        // Parse the value
        JsonValue value = parseValue(stream);

        // Add the value to the array
        array.push_back(value);

        skipWhitespace(stream);

        // Check for end of array or comma
        char c = get(stream);
        if (c == ']') {
            break;
        } else if (c != ',') {
            throw std::runtime_error("Expected ',' or ']' after value in array");
        }
    }

    return array;
}

std::string JsonParser::parseString(std::istringstream& stream) {
    std::string result;

    // Consume the opening quote
    get(stream);

    while (true) {
        char c = get(stream);

        if (c == '"') {
            break;
        } else if (c == '\\') {
            char escape = get(stream);

            switch (escape) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    // Parse 4-digit hex code
                    std::string hex;
                    for (int i = 0; i < 4; ++i) {
                        char h = get(stream);
                        if (!isHexDigit(h)) {
                            throw std::runtime_error("Invalid hex digit in Unicode escape");
                        }
                        hex += h;
                    }

                    // Convert hex to int
                    int code = std::stoi(hex, nullptr, 16);

                    // Handle UTF-16 surrogate pairs
                    if (code >= 0xD800 && code <= 0xDBFF) {
                        // High surrogate, expect a low surrogate
                        if (get(stream) != '\\' || get(stream) != 'u') {
                            throw std::runtime_error("Expected low surrogate after high surrogate");
                        }

                        std::string lowHex;
                        for (int i = 0; i < 4; ++i) {
                            char h = get(stream);
                            if (!isHexDigit(h)) {
                                throw std::runtime_error("Invalid hex digit in Unicode escape");
                            }
                            lowHex += h;
                        }

                        int lowCode = std::stoi(lowHex, nullptr, 16);
                        if (lowCode < 0xDC00 || lowCode > 0xDFFF) {
                            throw std::runtime_error("Invalid low surrogate");
                        }

                        // Combine the surrogate pair
                        code = 0x10000 + ((code - 0xD800) << 10) + (lowCode - 0xDC00);
                    }

                    // Convert to UTF-8
                    if (code < 0x80) {
                        result += static_cast<char>(code);
                    } else if (code < 0x800) {
                        result += static_cast<char>(0xC0 | (code >> 6));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    } else if (code < 0x10000) {
                        result += static_cast<char>(0xE0 | (code >> 12));
                        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        result += static_cast<char>(0xF0 | (code >> 18));
                        result += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                        result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        result += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    throw std::runtime_error(std::string("Invalid escape sequence: \\") + escape);
            }
        } else if (static_cast<unsigned char>(c) < 0x20) {
            throw std::runtime_error("Control character in string");
        } else {
            result += c;
        }
    }

    return result;
}

JsonValue JsonParser::parseNumber(std::istringstream& stream) {
    std::string number;

    // Parse the sign
    if (peek(stream) == '-') {
        number += get(stream);
    }

    // Parse the integer part
    if (peek(stream) == '0') {
        number += get(stream);
    } else if (isDigit(peek(stream))) {
        while (isDigit(peek(stream))) {
            number += get(stream);
        }
    } else {
        throw std::runtime_error("Expected digit");
    }

    // Parse the fractional part
    if (peek(stream) == '.') {
        number += get(stream);

        if (!isDigit(peek(stream))) {
            throw std::runtime_error("Expected digit after decimal point");
        }

        while (isDigit(peek(stream))) {
            number += get(stream);
        }
    }

    // Parse the exponent
    if (peek(stream) == 'e' || peek(stream) == 'E') {
        number += get(stream);

        if (peek(stream) == '+' || peek(stream) == '-') {
            number += get(stream);
        }

        if (!isDigit(peek(stream))) {
            throw std::runtime_error("Expected digit in exponent");
        }

        while (isDigit(peek(stream))) {
            number += get(stream);
        }
    }

    return JsonValue(std::stod(number));
}

JsonValue JsonParser::parseKeyword(std::istringstream& stream) {
    std::string keyword;

    while (std::isalpha(peek(stream))) {
        keyword += get(stream);
    }

    if (keyword == "true") {
        return JsonValue(true);
    } else if (keyword == "false") {
        return JsonValue(false);
    } else if (keyword == "null") {
        return JsonValue(nullptr);
    } else {
        throw std::runtime_error("Unknown keyword: " + keyword);
    }
}

void JsonParser::skipWhitespace(std::istringstream& stream) {
    while (std::isspace(peek(stream))) {
        get(stream);
    }
}

char JsonParser::peek(std::istringstream& stream) {
    int c = stream.peek();
    if (c == EOF) {
        throw std::runtime_error("Unexpected end of input");
    }
    return static_cast<char>(c);
}

char JsonParser::get(std::istringstream& stream) {
    int c = stream.get();
    if (c == EOF) {
        throw std::runtime_error("Unexpected end of input");
    }
    return static_cast<char>(c);
}

bool JsonParser::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool JsonParser::isHexDigit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

} // namespace dht_hunter::utility::json
