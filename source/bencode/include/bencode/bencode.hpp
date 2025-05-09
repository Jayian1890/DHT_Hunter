#pragma once

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <optional>
#include <stdexcept>
#include <memory>

namespace bencode {

/**
 * @class BencodeException
 * @brief Exception thrown when bencode encoding/decoding fails.
 */
class BencodeException : public std::runtime_error {
public:
    explicit BencodeException(const std::string& message) : std::runtime_error(message) {}
};

// Forward declaration
class BencodeValue;

/**
 * @brief Alias for a bencode list
 */
using BencodeList = std::vector<std::shared_ptr<BencodeValue>>;

/**
 * @class BencodeValue
 * @brief Represents a bencode value, which can be a string, integer, list, or dictionary.
 */
class BencodeValue {
public:
    using String = std::string;
    using Integer = int64_t;
    using List = BencodeList;
    using Dictionary = std::map<std::string, std::shared_ptr<BencodeValue>>;
    using Value = std::variant<String, Integer, List, Dictionary>;

    /**
     * @brief Default constructor. Creates a bencode value of type string with empty value.
     */
    BencodeValue();

    /**
     * @brief Constructs a bencode value from a string.
     * @param value The string value.
     */
    explicit BencodeValue(const String& value);

    /**
     * @brief Constructs a bencode value from an integer.
     * @param value The integer value.
     */
    explicit BencodeValue(Integer value);

    /**
     * @brief Constructs a bencode value from a list.
     * @param value The list value.
     */
    explicit BencodeValue(const List& value);

    /**
     * @brief Constructs a bencode value from a dictionary.
     * @param value The dictionary value.
     */
    explicit BencodeValue(const Dictionary& value);

    /**
     * @brief Gets the type of the bencode value.
     * @return The type index of the bencode value.
     */
    std::size_t getType() const;

    /**
     * @brief Checks if the bencode value is a string.
     * @return True if the bencode value is a string, false otherwise.
     */
    bool isString() const;

    /**
     * @brief Checks if the bencode value is an integer.
     * @return True if the bencode value is an integer, false otherwise.
     */
    bool isInteger() const;

    /**
     * @brief Checks if the bencode value is a list.
     * @return True if the bencode value is a list, false otherwise.
     */
    bool isList() const;

    /**
     * @brief Checks if the bencode value is a dictionary.
     * @return True if the bencode value is a dictionary, false otherwise.
     */
    bool isDictionary() const;

    /**
     * @brief Gets the string value.
     * @return The string value.
     * @throws BencodeException if the bencode value is not a string.
     */
    const String& getString() const;

    /**
     * @brief Gets a string value for a key in a dictionary.
     * @param key The key to look up.
     * @return The string value, or std::nullopt if the key does not exist or the value is not a string.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    std::optional<String> getString(const std::string& key) const;

    /**
     * @brief Gets the integer value.
     * @return The integer value.
     * @throws BencodeException if the bencode value is not an integer.
     */
    Integer getInteger() const;

    /**
     * @brief Gets the list value.
     * @return The list value.
     * @throws BencodeException if the bencode value is not a list.
     */
    const List& getList() const;

    /**
     * @brief Gets a value from a list at the specified index.
     * @param index The index.
     * @return The value at the specified index.
     * @throws BencodeException if the bencode value is not a list or the index is out of range.
     */
    std::shared_ptr<BencodeValue> at(size_t index) const;

    /**
     * @brief Gets the size of a list.
     * @return The size of the list.
     * @throws BencodeException if the bencode value is not a list.
     */
    size_t size() const;

    /**
     * @brief Gets the dictionary value.
     * @return The dictionary value.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    const Dictionary& getDict() const;

    /**
     * @brief Gets a dictionary value for a key.
     * @param key The key to look up.
     * @return The dictionary value for the key, or std::nullopt if the key does not exist or the value is not a dictionary.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    std::optional<Dictionary> getDict(const std::string& key) const;

    /**
     * @brief Gets a value from a dictionary by key.
     * @param key The key to look up.
     * @return The value associated with the key, or nullptr if the key is not found.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    std::shared_ptr<BencodeValue> get(const std::string& key) const;

    /**
     * @brief Gets an integer value from a dictionary by key.
     * @param key The key to look up.
     * @return The integer value associated with the key, or std::nullopt if the key is not found or the value is not an integer.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    std::optional<Integer> getInteger(const std::string& key) const;

    /**
     * @brief Gets a list value from a dictionary by key.
     * @param key The key to look up.
     * @return The list value associated with the key, or std::nullopt if the key is not found or the value is not a list.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    std::optional<List> getList(const std::string& key) const;

    /**
     * @brief Gets a dictionary value from a dictionary by key.
     * @param key The key to look up.
     * @return The dictionary value associated with the key, or std::nullopt if the key is not found or the value is not a dictionary.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    std::optional<Dictionary> getDictionary(const std::string& key) const;

    /**
     * @brief Sets a value in a dictionary.
     * @param key The key to set.
     * @param value The value to set.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    void set(const std::string& key, std::shared_ptr<BencodeValue> value);

    /**
     * @brief Sets a string value in a dictionary.
     * @param key The key to set.
     * @param value The string value to set.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    void setString(const std::string& key, const String& value);

    /**
     * @brief Sets an integer value in a dictionary.
     * @param key The key to set.
     * @param value The integer value to set.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    void setInteger(const std::string& key, Integer value);

    /**
     * @brief Sets a list value in a dictionary.
     * @param key The key to set.
     * @param value The list value to set.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    void setList(const std::string& key, const List& value);

    /**
     * @brief Sets the value to a list.
     * @param value The list value to set.
     */
    void setList(const List& value);

    /**
     * @brief Sets a dictionary value in a dictionary.
     * @param key The key to set.
     * @param value The dictionary value to set.
     * @throws BencodeException if the bencode value is not a dictionary.
     */
    void setDict(const std::string& key, const Dictionary& value);

    /**
     * @brief Sets the value to a dictionary.
     * @param value The dictionary value to set.
     */
    void setDict(const Dictionary& value);

    /**
     * @brief Adds a value to a list.
     * @param value The value to add.
     * @throws BencodeException if the bencode value is not a list.
     */
    void add(std::shared_ptr<BencodeValue> value);

    /**
     * @brief Adds a string value to a list.
     * @param value The string value to add.
     * @throws BencodeException if the bencode value is not a list.
     */
    void addString(const String& value);

    /**
     * @brief Adds an integer value to a list.
     * @param value The integer value to add.
     * @throws BencodeException if the bencode value is not a list.
     */
    void addInteger(Integer value);

    /**
     * @brief Adds a list value to a list.
     * @param value The list value to add.
     * @throws BencodeException if the bencode value is not a list.
     */
    void addList(const List& value);

    /**
     * @brief Adds a dictionary value to a list.
     * @param value The dictionary value to add.
     * @throws BencodeException if the bencode value is not a list.
     */
    void addDictionary(const Dictionary& value);

    /**
     * @brief Gets the underlying value.
     * @return The underlying value.
     */
    const Value& getValue() const;

    /**
     * @brief Gets the underlying value.
     * @return The underlying value.
     */
    Value& getValue();

    /**
     * @brief Sets the value to a string
     * @param str The string
     */
    void setString(const String& str);

    /**
     * @brief Sets the value to an integer
     * @param value The integer
     */
    void setInteger(Integer value);

    /**
     * @brief Checks if a dictionary contains a key
     * @param key The key to check
     * @return True if the dictionary contains the key, false otherwise
     * @throws BencodeException if the bencode value is not a dictionary
     */
    bool contains(const std::string& key) const;

    /**
     * @brief Assignment operator for string
     * @param str The string to assign
     * @return Reference to this object
     */
    BencodeValue& operator=(const std::string& str);

    /**
     * @brief Assignment operator for integer
     * @param value The integer to assign
     * @return Reference to this object
     */
    BencodeValue& operator=(int64_t value);

    /**
     * @brief Assignment operator for list
     * @param list The list to assign
     * @return Reference to this object
     */
    BencodeValue& operator=(const BencodeList& list);

    /**
     * @brief Assignment operator for dictionary
     * @param dict The dictionary to assign
     * @return Reference to this object
     */
    BencodeValue& operator=(const Dictionary& dict);

    /**
     * @brief Encodes the bencode value to a string.
     * @return The encoded string.
     */
    std::string encode() const;

    /**
     * @brief Decodes a string to a bencode value.
     * @param data The string to decode.
     * @param size The size of the string.
     * @return The decoded bencode value.
     */
    static std::shared_ptr<BencodeValue> decode(const char* data, size_t size);

private:
    Value m_value;
};

/**
 * @class BencodeEncoder
 * @brief Encodes bencode values to strings.
 */
class BencodeEncoder {
public:
    /**
     * @brief Encodes a bencode value to a string.
     * @param value The bencode value to encode.
     * @return The encoded string.
     */
    static std::string encode(const BencodeValue& value);

    /**
     * @brief Encodes a shared_ptr<BencodeValue> to a string.
     * @param value The shared_ptr<BencodeValue> to encode.
     * @return The encoded string.
     */
    static std::string encode(const std::shared_ptr<BencodeValue>& value);

    /**
     * @brief Encodes a string to a bencode string.
     * @param value The string to encode.
     * @return The encoded string.
     */
    static std::string encodeString(const std::string& value);

    /**
     * @brief Encodes an integer to a bencode integer.
     * @param value The integer to encode.
     * @return The encoded string.
     */
    static std::string encodeInteger(int64_t value);

    /**
     * @brief Encodes a list to a bencode list.
     * @param value The list to encode.
     * @return The encoded string.
     */
    static std::string encodeList(const BencodeValue::List& value);

    /**
     * @brief Encodes a dictionary to a bencode dictionary.
     * @param value The dictionary to encode.
     * @return The encoded string.
     */
    static std::string encodeDictionary(const BencodeValue::Dictionary& value);
};

/**
 * @class BencodeDecoder
 * @brief Decodes strings to bencode values.
 */
class BencodeDecoder {
public:
    /**
     * @brief Decodes a string to a bencode value.
     * @param data The string to decode.
     * @return The decoded bencode value.
     * @throws BencodeException if the string is not a valid bencode value.
     */
    static std::shared_ptr<BencodeValue> decode(const std::string& data);

    /**
     * @brief Decodes a string to a bencode value.
     * @param data The string to decode.
     * @param pos The position to start decoding from. Will be updated to the position after the decoded value.
     * @return The decoded bencode value.
     * @throws BencodeException if the string is not a valid bencode value.
     */
    static std::shared_ptr<BencodeValue> decode(const std::string& data, size_t& pos);

private:
    /**
     * @brief Decodes a bencode string.
     * @param data The string to decode.
     * @param pos The position to start decoding from. Will be updated to the position after the decoded value.
     * @return The decoded string.
     * @throws BencodeException if the string is not a valid bencode string.
     */
    static std::string decodeString(const std::string& data, size_t& pos);

    /**
     * @brief Decodes a bencode integer.
     * @param data The string to decode.
     * @param pos The position to start decoding from. Will be updated to the position after the decoded value.
     * @return The decoded integer.
     * @throws BencodeException if the string is not a valid bencode integer.
     */
    static int64_t decodeInteger(const std::string& data, size_t& pos);

    /**
     * @brief Decodes a bencode list.
     * @param data The string to decode.
     * @param pos The position to start decoding from. Will be updated to the position after the decoded value.
     * @return The decoded list.
     * @throws BencodeException if the string is not a valid bencode list.
     */
    static BencodeValue::List decodeList(const std::string& data, size_t& pos);

    /**
     * @brief Decodes a bencode dictionary.
     * @param data The string to decode.
     * @param pos The position to start decoding from. Will be updated to the position after the decoded value.
     * @return The decoded dictionary.
     * @throws BencodeException if the string is not a valid bencode dictionary.
     */
    static BencodeValue::Dictionary decodeDictionary(const std::string& data, size_t& pos);
};

} // namespace bencode
