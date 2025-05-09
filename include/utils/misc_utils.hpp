#pragma once

/**
 * @file misc_utils.hpp
 * @brief Miscellaneous utility functions and types for the BitScrape project
 *
 * This file consolidates miscellaneous functionality from various utility files:
 * - random_utils.hpp (Random number generation)
 * - json.hpp (JSON parsing and serialization)
 * - configuration_manager.hpp (Configuration management)
 */

// Standard C++ libraries
#include <string>
#include <vector>
#include <cstdint>
#include <random>
#include <mutex>
#include <map>
#include <variant>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <optional>
#include <any>
#include <filesystem>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <unordered_map>

namespace dht_hunter::utility {

//
// Random Utilities
//
namespace random {

/**
 * @brief Generates a random byte array
 * @param length The length of the array
 * @return The random byte array
 */
std::vector<uint8_t> generateRandomBytes(size_t length);

/**
 * @brief Generates a random hex string
 * @param length The length of the string in bytes (output will be 2*length characters)
 * @return The random hex string
 */
std::string generateRandomHexString(size_t length);

/**
 * @brief Generates a random transaction ID for DHT messages
 * @param length The length of the ID in bytes (default: 4)
 * @return The random transaction ID
 */
std::string generateTransactionID(size_t length = 4);

/**
 * @brief Generates a random token for DHT security
 * @param length The length of the token in bytes (default: 20)
 * @return The random token
 */
std::string generateToken(size_t length = 20);

/**
 * @brief Thread-safe random number generator
 */
class RandomGenerator {
public:
    /**
     * @brief Constructor
     */
    RandomGenerator();
    
    /**
     * @brief Generates a random integer in the range [min, max]
     * @param min The minimum value
     * @param max The maximum value
     * @return The random integer
     */
    int getRandomInt(int min, int max);
    
    /**
     * @brief Generates a random byte
     * @return The random byte
     */
    uint8_t getRandomByte();
    
    /**
     * @brief Generates a random byte array
     * @param length The length of the array
     * @return The random byte array
     */
    std::vector<uint8_t> getRandomBytes(size_t length);
    
private:
    std::mt19937 m_generator;
    std::mutex m_mutex;
};

} // namespace random

//
// JSON Utilities
//
namespace json {

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
     * @brief Constructor for number
     */
    JsonValue(double value) : m_value(value) {}

    /**
     * @brief Constructor for integer
     */
    JsonValue(int value) : m_value(static_cast<double>(value)) {}

    /**
     * @brief Constructor for string
     */
    JsonValue(const std::string& value) : m_value(value) {}

    /**
     * @brief Constructor for string literal
     */
    JsonValue(const char* value) : m_value(std::string(value)) {}

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
     * @return The index of the type in the variant
     */
    size_t getType() const {
        return m_value.index();
    }

    /**
     * @brief Check if the value is null
     * @return True if the value is null, false otherwise
     */
    bool isNull() const {
        return std::holds_alternative<NullType>(m_value);
    }

    /**
     * @brief Check if the value is a boolean
     * @return True if the value is a boolean, false otherwise
     */
    bool isBool() const {
        return std::holds_alternative<BoolType>(m_value);
    }

    /**
     * @brief Check if the value is a number
     * @return True if the value is a number, false otherwise
     */
    bool isNumber() const {
        return std::holds_alternative<NumberType>(m_value);
    }

    /**
     * @brief Check if the value is a string
     * @return True if the value is a string, false otherwise
     */
    bool isString() const {
        return std::holds_alternative<StringType>(m_value);
    }

    /**
     * @brief Check if the value is an object
     * @return True if the value is an object, false otherwise
     */
    bool isObject() const {
        return std::holds_alternative<ObjectType>(m_value);
    }

    /**
     * @brief Check if the value is an array
     * @return True if the value is an array, false otherwise
     */
    bool isArray() const {
        return std::holds_alternative<ArrayType>(m_value);
    }

    /**
     * @brief Get the value as a boolean
     * @return The boolean value
     * @throws std::bad_variant_access if the value is not a boolean
     */
    bool getBool() const {
        return std::get<BoolType>(m_value);
    }

    /**
     * @brief Get the value as a number
     * @return The number value
     * @throws std::bad_variant_access if the value is not a number
     */
    double getNumber() const {
        return std::get<NumberType>(m_value);
    }

    /**
     * @brief Get the value as a string
     * @return The string value
     * @throws std::bad_variant_access if the value is not a string
     */
    const std::string& getString() const {
        return std::get<StringType>(m_value);
    }

    /**
     * @brief Get the value as an object
     * @return The object value
     * @throws std::bad_variant_access if the value is not an object
     */
    const ObjectType& getObject() const {
        return std::get<ObjectType>(m_value);
    }

    /**
     * @brief Get the value as an array
     * @return The array value
     * @throws std::bad_variant_access if the value is not an array
     */
    const ArrayType& getArray() const {
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
         * @return True if the key was removed, false if it didn't exist
         */
        bool remove(const std::string& key) {
            return m_values.erase(key) > 0;
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
         * @brief Get the number of key-value pairs in the object
         * @return The number of key-value pairs
         */
        size_t size() const {
            return m_values.size();
        }

        /**
         * @brief Check if the object is empty
         * @return True if the object is empty, false otherwise
         */
        bool empty() const {
            return m_values.empty();
        }

        /**
         * @brief Clear the object
         */
        void clear() {
            m_values.clear();
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
         * @return The value, or null if the index is out of bounds
         */
        JsonValue get(size_t index) const {
            if (index >= m_values.size()) {
                return JsonValue(nullptr);
            }
            return m_values[index];
        }

        /**
         * @brief Set a value in the array
         * @param index The index
         * @param value The value
         * @return True if the value was set, false if the index is out of bounds
         */
        bool set(size_t index, const JsonValue& value) {
            if (index >= m_values.size()) {
                return false;
            }
            m_values[index] = value;
            return true;
        }

        /**
         * @brief Remove a value from the array
         * @param index The index
         * @return True if the value was removed, false if the index is out of bounds
         */
        bool remove(size_t index) {
            if (index >= m_values.size()) {
                return false;
            }
            m_values.erase(m_values.begin() + index);
            return true;
        }

        /**
         * @brief Get the number of values in the array
         * @return The number of values
         */
        size_t size() const {
            return m_values.size();
        }

        /**
         * @brief Check if the array is empty
         * @return True if the array is empty, false otherwise
         */
        bool empty() const {
            return m_values.empty();
        }

        /**
         * @brief Clear the array
         */
        void clear() {
            m_values.clear();
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

} // namespace json

} // namespace dht_hunter::utility
