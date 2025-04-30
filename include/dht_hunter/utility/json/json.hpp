#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <optional>

namespace dht_hunter::utility::json {

/**
 * @brief A simple JSON value class
 */
class JsonValue {
public:
    // Forward declaration of JsonObject and JsonArray
    class JsonObject;
    class JsonArray;

    // JSON value types
    using NullType = std::nullptr_t;
    using BoolType = bool;
    using NumberType = double;
    using StringType = std::string;
    using ObjectType = std::shared_ptr<JsonObject>;
    using ArrayType = std::shared_ptr<JsonArray>;

    // Variant to hold any JSON value type
    using ValueType = std::variant<NullType, BoolType, NumberType, StringType, ObjectType, ArrayType>;

    /**
     * @brief Default constructor (creates a null value)
     */
    JsonValue() : m_value(nullptr) {}

    /**
     * @brief Constructor for null
     */
    JsonValue(std::nullptr_t) : m_value(nullptr) {}

    /**
     * @brief Constructor for boolean
     */
    JsonValue(bool value) : m_value(value) {}

    /**
     * @brief Constructor for integer
     */
    JsonValue(int value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for unsigned integer
     */
    JsonValue(unsigned int value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for long
     */
    JsonValue(long value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for unsigned long
     */
    JsonValue(unsigned long value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for long long
     */
    JsonValue(long long value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for unsigned long long
     */
    JsonValue(unsigned long long value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for float
     */
    JsonValue(float value) : m_value(static_cast<NumberType>(value)) {}

    /**
     * @brief Constructor for double
     */
    JsonValue(double value) : m_value(value) {}

    /**
     * @brief Constructor for string
     */
    JsonValue(const char* value) : m_value(std::string(value)) {}

    /**
     * @brief Constructor for string
     */
    JsonValue(const std::string& value) : m_value(value) {}

    /**
     * @brief Constructor for object
     */
    JsonValue(const ObjectType& value) : m_value(value) {}

    /**
     * @brief Constructor for array
     */
    JsonValue(const ArrayType& value) : m_value(value) {}

    /**
     * @brief Get the type of the value
     */
    enum class Type {
        Null,
        Boolean,
        Number,
        String,
        Object,
        Array
    };

    /**
     * @brief Get the type of the value
     * @return The type of the value
     */
    Type getType() const {
        if (std::holds_alternative<NullType>(m_value)) return Type::Null;
        if (std::holds_alternative<BoolType>(m_value)) return Type::Boolean;
        if (std::holds_alternative<NumberType>(m_value)) return Type::Number;
        if (std::holds_alternative<StringType>(m_value)) return Type::String;
        if (std::holds_alternative<ObjectType>(m_value)) return Type::Object;
        if (std::holds_alternative<ArrayType>(m_value)) return Type::Array;
        return Type::Null; // Should never happen
    }

    /**
     * @brief Check if the value is null
     * @return True if the value is null, false otherwise
     */
    bool isNull() const {
        return getType() == Type::Null;
    }

    /**
     * @brief Check if the value is a boolean
     * @return True if the value is a boolean, false otherwise
     */
    bool isBoolean() const {
        return getType() == Type::Boolean;
    }

    /**
     * @brief Check if the value is a number
     * @return True if the value is a number, false otherwise
     */
    bool isNumber() const {
        return getType() == Type::Number;
    }

    /**
     * @brief Check if the value is a string
     * @return True if the value is a string, false otherwise
     */
    bool isString() const {
        return getType() == Type::String;
    }

    /**
     * @brief Check if the value is an object
     * @return True if the value is an object, false otherwise
     */
    bool isObject() const {
        return getType() == Type::Object;
    }

    /**
     * @brief Check if the value is an array
     * @return True if the value is an array, false otherwise
     */
    bool isArray() const {
        return getType() == Type::Array;
    }

    /**
     * @brief Get the value as a boolean
     * @return The value as a boolean
     * @throws std::runtime_error if the value is not a boolean
     */
    bool getBoolean() const {
        if (!isBoolean()) {
            throw std::runtime_error("JSON value is not a boolean");
        }
        return std::get<BoolType>(m_value);
    }

    /**
     * @brief Get the value as a number
     * @return The value as a number
     * @throws std::runtime_error if the value is not a number
     */
    NumberType getNumber() const {
        if (!isNumber()) {
            throw std::runtime_error("JSON value is not a number");
        }
        return std::get<NumberType>(m_value);
    }

    /**
     * @brief Get the value as an integer
     * @return The value as an integer
     * @throws std::runtime_error if the value is not a number
     */
    int getInt() const {
        return static_cast<int>(getNumber());
    }

    /**
     * @brief Get the value as an unsigned integer
     * @return The value as an unsigned integer
     * @throws std::runtime_error if the value is not a number
     */
    unsigned int getUInt() const {
        return static_cast<unsigned int>(getNumber());
    }

    /**
     * @brief Get the value as a long
     * @return The value as a long
     * @throws std::runtime_error if the value is not a number
     */
    long getLong() const {
        return static_cast<long>(getNumber());
    }

    /**
     * @brief Get the value as an unsigned long
     * @return The value as an unsigned long
     * @throws std::runtime_error if the value is not a number
     */
    unsigned long getULong() const {
        return static_cast<unsigned long>(getNumber());
    }

    /**
     * @brief Get the value as a long long
     * @return The value as a long long
     * @throws std::runtime_error if the value is not a number
     */
    long long getLongLong() const {
        return static_cast<long long>(getNumber());
    }

    /**
     * @brief Get the value as an unsigned long long
     * @return The value as an unsigned long long
     * @throws std::runtime_error if the value is not a number
     */
    unsigned long long getULongLong() const {
        return static_cast<unsigned long long>(getNumber());
    }

    /**
     * @brief Get the value as a string
     * @return The value as a string
     * @throws std::runtime_error if the value is not a string
     */
    const std::string& getString() const {
        if (!isString()) {
            throw std::runtime_error("JSON value is not a string");
        }
        return std::get<StringType>(m_value);
    }

    /**
     * @brief Get the value as an object
     * @return The value as an object
     * @throws std::runtime_error if the value is not an object
     */
    const ObjectType& getObject() const {
        if (!isObject()) {
            throw std::runtime_error("JSON value is not an object");
        }
        return std::get<ObjectType>(m_value);
    }

    /**
     * @brief Get the value as an array
     * @return The value as an array
     * @throws std::runtime_error if the value is not an array
     */
    const ArrayType& getArray() const {
        if (!isArray()) {
            throw std::runtime_error("JSON value is not an array");
        }
        return std::get<ArrayType>(m_value);
    }

    /**
     * @brief Convert the value to a string representation
     * @param pretty Whether to format the output with indentation
     * @return The string representation of the value
     */
    std::string toString(bool pretty = false) const;

    /**
     * @brief Parse a JSON string
     * @param json The JSON string to parse
     * @return The parsed JSON value
     * @throws std::runtime_error if the JSON string is invalid
     */
    static JsonValue parse(const std::string& json);

    /**
     * @brief Create a new JSON object
     * @return A new JSON object
     */
    static ObjectType createObject();

    /**
     * @brief Create a new JSON array
     * @return A new JSON array
     */
    static ArrayType createArray();

    /**
     * @brief JSON object class
     */
    class JsonObject {
    public:
        /**
         * @brief Default constructor
         */
        JsonObject() = default;

        /**
         * @brief Set a value in the object
         * @param key The key
         * @param value The value
         */
        void set(const std::string& key, const JsonValue& value) {
            m_values[key] = value;
        }

        /**
         * @brief Get a value from the object
         * @param key The key
         * @return The value, or null if the key doesn't exist
         */
        JsonValue get(const std::string& key) const {
            auto it = m_values.find(key);
            if (it == m_values.end()) {
                return JsonValue(nullptr);
            }
            return it->second;
        }

        /**
         * @brief Check if the object has a key
         * @param key The key
         * @return True if the object has the key, false otherwise
         */
        bool has(const std::string& key) const {
            return m_values.find(key) != m_values.end();
        }

        /**
         * @brief Remove a key from the object
         * @param key The key
         */
        void remove(const std::string& key) {
            m_values.erase(key);
        }

        /**
         * @brief Get the number of key-value pairs in the object
         * @return The number of key-value pairs
         */
        size_t size() const {
            return m_values.size();
        }

        /**
         * @brief Get all keys in the object
         * @return A vector of all keys
         */
        std::vector<std::string> keys() const {
            std::vector<std::string> result;
            for (const auto& pair : m_values) {
                result.push_back(pair.first);
            }
            return result;
        }

        /**
         * @brief Get the underlying map
         * @return The underlying map
         */
        const std::map<std::string, JsonValue>& getMap() const {
            return m_values;
        }

    private:
        std::map<std::string, JsonValue> m_values;
    };

    /**
     * @brief JSON array class
     */
    class JsonArray {
    public:
        /**
         * @brief Default constructor
         */
        JsonArray() = default;

        /**
         * @brief Add a value to the array
         * @param value The value
         */
        void add(const JsonValue& value) {
            m_values.push_back(value);
        }

        /**
         * @brief Get a value from the array
         * @param index The index
         * @return The value
         * @throws std::out_of_range if the index is out of range
         */
        JsonValue get(size_t index) const {
            if (index >= m_values.size()) {
                throw std::out_of_range("JSON array index out of range");
            }
            return m_values[index];
        }

        /**
         * @brief Get the number of values in the array
         * @return The number of values
         */
        size_t size() const {
            return m_values.size();
        }

        /**
         * @brief Get the underlying vector
         * @return The underlying vector
         */
        const std::vector<JsonValue>& getVector() const {
            return m_values;
        }

    private:
        std::vector<JsonValue> m_values;
    };

private:
    ValueType m_value;
};

/**
 * @brief JSON utility functions
 */
class Json {
public:
    /**
     * @brief Parse a JSON string
     * @param json The JSON string to parse
     * @return The parsed JSON value
     * @throws std::runtime_error if the JSON string is invalid
     */
    static JsonValue parse(const std::string& json) {
        return JsonValue::parse(json);
    }

    /**
     * @brief Convert a JSON value to a string
     * @param value The JSON value
     * @param pretty Whether to format the output with indentation
     * @return The string representation of the value
     */
    static std::string stringify(const JsonValue& value, bool pretty = false) {
        return value.toString(pretty);
    }

    /**
     * @brief Create a new JSON object
     * @return A new JSON object
     */
    static JsonValue::ObjectType createObject() {
        return JsonValue::createObject();
    }

    /**
     * @brief Create a new JSON array
     * @return A new JSON array
     */
    static JsonValue::ArrayType createArray() {
        return JsonValue::createArray();
    }
};

} // namespace dht_hunter::utility::json
