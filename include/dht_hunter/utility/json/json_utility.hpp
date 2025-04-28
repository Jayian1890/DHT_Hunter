#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <memory>
#include <stdexcept>
#include <variant>
#include <optional>

namespace dht_hunter::utility::json {

/**
 * @brief A simple JSON value class
 */
class JsonValue {
public:
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Array,
        Object
    };

    // Forward declarations
    class JsonArray;
    class JsonObject;

    // Variant type for storing different JSON value types
    using ValueType = std::variant<
        std::nullptr_t,
        bool,
        double,
        std::string,
        std::shared_ptr<JsonArray>,
        std::shared_ptr<JsonObject>
    >;

    /**
     * @brief A JSON array
     */
    class JsonArray {
    public:
        JsonArray() = default;
        
        void push_back(const JsonValue& value) {
            m_values.push_back(value);
        }
        
        size_t size() const {
            return m_values.size();
        }
        
        const JsonValue& operator[](size_t index) const {
            return m_values.at(index);
        }
        
        JsonValue& operator[](size_t index) {
            return m_values.at(index);
        }
        
        auto begin() { return m_values.begin(); }
        auto end() { return m_values.end(); }
        auto begin() const { return m_values.begin(); }
        auto end() const { return m_values.end(); }
        
    private:
        std::vector<JsonValue> m_values;
    };

    /**
     * @brief A JSON object
     */
    class JsonObject {
    public:
        JsonObject() = default;
        
        void set(const std::string& key, const JsonValue& value) {
            m_values[key] = value;
        }
        
        bool has(const std::string& key) const {
            return m_values.find(key) != m_values.end();
        }
        
        const JsonValue& operator[](const std::string& key) const {
            auto it = m_values.find(key);
            if (it == m_values.end()) {
                throw std::out_of_range("Key not found: " + key);
            }
            return it->second;
        }
        
        JsonValue& operator[](const std::string& key) {
            return m_values[key];
        }
        
        size_t size() const {
            return m_values.size();
        }
        
        auto begin() { return m_values.begin(); }
        auto end() { return m_values.end(); }
        auto begin() const { return m_values.begin(); }
        auto end() const { return m_values.end(); }
        
    private:
        std::map<std::string, JsonValue> m_values;
    };

    // Constructors
    JsonValue() : m_value(nullptr) {}
    JsonValue(std::nullptr_t) : m_value(nullptr) {}
    JsonValue(bool value) : m_value(value) {}
    JsonValue(int value) : m_value(static_cast<double>(value)) {}
    JsonValue(double value) : m_value(value) {}
    JsonValue(const char* value) : m_value(std::string(value)) {}
    JsonValue(const std::string& value) : m_value(value) {}
    
    // Array constructor
    JsonValue(const JsonArray& array) : m_value(std::make_shared<JsonArray>(array)) {}
    
    // Object constructor
    JsonValue(const JsonObject& object) : m_value(std::make_shared<JsonObject>(object)) {}

    // Type checking
    Type getType() const {
        return static_cast<Type>(m_value.index());
    }
    
    bool isNull() const { return getType() == Type::Null; }
    bool isBoolean() const { return getType() == Type::Boolean; }
    bool isNumber() const { return getType() == Type::Number; }
    bool isString() const { return getType() == Type::String; }
    bool isArray() const { return getType() == Type::Array; }
    bool isObject() const { return getType() == Type::Object; }

    // Value getters
    bool getBool() const {
        if (!isBoolean()) throw std::runtime_error("Not a boolean value");
        return std::get<bool>(m_value);
    }
    
    double getNumber() const {
        if (!isNumber()) throw std::runtime_error("Not a number value");
        return std::get<double>(m_value);
    }
    
    int getInt() const {
        return static_cast<int>(getNumber());
    }
    
    const std::string& getString() const {
        if (!isString()) throw std::runtime_error("Not a string value");
        return std::get<std::string>(m_value);
    }
    
    const JsonArray& getArray() const {
        if (!isArray()) throw std::runtime_error("Not an array value");
        return *std::get<std::shared_ptr<JsonArray>>(m_value);
    }
    
    JsonArray& getArray() {
        if (!isArray()) throw std::runtime_error("Not an array value");
        return *std::get<std::shared_ptr<JsonArray>>(m_value);
    }
    
    const JsonObject& getObject() const {
        if (!isObject()) throw std::runtime_error("Not an object value");
        return *std::get<std::shared_ptr<JsonObject>>(m_value);
    }
    
    JsonObject& getObject() {
        if (!isObject()) throw std::runtime_error("Not an object value");
        return *std::get<std::shared_ptr<JsonObject>>(m_value);
    }

    // Convenience methods for creating arrays and objects
    static JsonValue createArray() {
        JsonValue value;
        value.m_value = std::make_shared<JsonArray>();
        return value;
    }
    
    static JsonValue createObject() {
        JsonValue value;
        value.m_value = std::make_shared<JsonObject>();
        return value;
    }

    // Serialization
    std::string toString(int indent = 0) const;

private:
    ValueType m_value;
};

/**
 * @brief JSON parser class
 */
class JsonParser {
public:
    /**
     * @brief Parse a JSON string
     * @param json The JSON string to parse
     * @return The parsed JSON value
     * @throws std::runtime_error if parsing fails
     */
    static JsonValue parse(const std::string& json);

private:
    static JsonValue parseValue(std::istringstream& stream);
    static JsonValue::JsonObject parseObject(std::istringstream& stream);
    static JsonValue::JsonArray parseArray(std::istringstream& stream);
    static std::string parseString(std::istringstream& stream);
    static JsonValue parseNumber(std::istringstream& stream);
    static JsonValue parseKeyword(std::istringstream& stream);
    static void skipWhitespace(std::istringstream& stream);
    static char peek(std::istringstream& stream);
    static char get(std::istringstream& stream);
    static bool isDigit(char c);
    static bool isHexDigit(char c);
};

/**
 * @brief Utility functions for working with JSON
 */
namespace JsonUtil {
    /**
     * @brief Parse a JSON string
     * @param json The JSON string to parse
     * @return The parsed JSON value
     */
    inline JsonValue parse(const std::string& json) {
        return JsonParser::parse(json);
    }
    
    /**
     * @brief Serialize a JSON value to a string
     * @param value The JSON value to serialize
     * @param indent The indentation level (0 for no pretty printing)
     * @return The serialized JSON string
     */
    inline std::string stringify(const JsonValue& value, int indent = 0) {
        return value.toString(indent);
    }
    
    /**
     * @brief Create a new JSON object
     * @return A new JSON object
     */
    inline JsonValue::JsonObject createObject() {
        return JsonValue::JsonObject();
    }
    
    /**
     * @brief Create a new JSON array
     * @return A new JSON array
     */
    inline JsonValue::JsonArray createArray() {
        return JsonValue::JsonArray();
    }
}

} // namespace dht_hunter::utility::json
