#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stdexcept>
#include <optional>

namespace bencode {

// Forward declaration
class BencodeValue;

// Type aliases
using BencodeValuePtr = std::shared_ptr<BencodeValue>;
using List = std::vector<BencodeValuePtr>;
using Dictionary = std::map<std::string, BencodeValuePtr>;

/**
 * @brief Class representing a bencode value
 */
class BencodeValue {
public:
    enum class Type {
        String,
        Integer,
        List,
        Dictionary
    };

private:
    Type type;
    std::string stringValue;
    int64_t integerValue;
    List listValue;
    Dictionary dictValue;

public:
    /**
     * @brief Default constructor (creates a string value)
     */
    BencodeValue() : type(Type::String), stringValue(""), integerValue(0) {}

    /**
     * @brief Constructor for string values
     * @param value The string value
     */
    BencodeValue(const std::string& value) : type(Type::String), stringValue(value), integerValue(0) {}

    /**
     * @brief Constructor for integer values
     * @param value The integer value
     */
    BencodeValue(int64_t value) : type(Type::Integer), stringValue(""), integerValue(value) {}

    /**
     * @brief Constructor for list values
     * @param value The list value
     */
    BencodeValue(const List& value) : type(Type::List), stringValue(""), integerValue(0), listValue(value) {}

    /**
     * @brief Constructor for dictionary values
     * @param value The dictionary value
     */
    BencodeValue(const Dictionary& value) : type(Type::Dictionary), stringValue(""), integerValue(0), dictValue(value) {}

    /**
     * @brief Check if the value is a string
     * @return True if the value is a string, false otherwise
     */
    bool isString() const {
        return type == Type::String;
    }

    /**
     * @brief Check if the value is an integer
     * @return True if the value is an integer, false otherwise
     */
    bool isInteger() const {
        return type == Type::Integer;
    }

    /**
     * @brief Check if the value is a list
     * @return True if the value is a list, false otherwise
     */
    bool isList() const {
        return type == Type::List;
    }

    /**
     * @brief Check if the value is a dictionary
     * @return True if the value is a dictionary, false otherwise
     */
    bool isDictionary() const {
        return type == Type::Dictionary;
    }

    /**
     * @brief Get the string value
     * @return The string value
     */
    const std::string& getString() const {
        if (!isString()) {
            throw std::runtime_error("Value is not a string");
        }
        return stringValue;
    }

    /**
     * @brief Get the integer value
     * @return The integer value
     */
    int64_t getInteger() const {
        if (!isInteger()) {
            throw std::runtime_error("Value is not an integer");
        }
        return integerValue;
    }

    /**
     * @brief Get the list value
     * @return The list value
     */
    const List& getList() const {
        if (!isList()) {
            throw std::runtime_error("Value is not a list");
        }
        return listValue;
    }

    /**
     * @brief Get the dictionary value
     * @return The dictionary value
     */
    const Dictionary& getDict() const {
        if (!isDictionary()) {
            throw std::runtime_error("Value is not a dictionary");
        }
        return dictValue;
    }

    /**
     * @brief Set the string value
     * @param value The string value
     */
    void setString(const std::string& value) {
        type = Type::String;
        stringValue = value;
    }

    /**
     * @brief Set the integer value
     * @param value The integer value
     */
    void setInteger(int64_t value) {
        type = Type::Integer;
        integerValue = value;
    }

    /**
     * @brief Set the list value
     * @param value The list value
     */
    void setList(const List& value) {
        type = Type::List;
        listValue = value;
    }

    /**
     * @brief Set the dictionary value
     * @param value The dictionary value
     */
    void setDict(const Dictionary& value) {
        type = Type::Dictionary;
        dictValue = value;
    }

    /**
     * @brief Get a string value from a dictionary
     * @param key The key
     * @return The string value, or std::nullopt if not found
     */
    std::optional<std::string> getString(const std::string& key) const {
        if (!isDictionary()) {
            return std::nullopt;
        }
        auto it = dictValue.find(key);
        if (it == dictValue.end() || !it->second->isString()) {
            return std::nullopt;
        }
        return it->second->getString();
    }

    /**
     * @brief Get an integer value from a dictionary
     * @param key The key
     * @return The integer value, or std::nullopt if not found
     */
    std::optional<int64_t> getInteger(const std::string& key) const {
        if (!isDictionary()) {
            return std::nullopt;
        }
        auto it = dictValue.find(key);
        if (it == dictValue.end() || !it->second->isInteger()) {
            return std::nullopt;
        }
        return it->second->getInteger();
    }

    /**
     * @brief Get a list value from a dictionary
     * @param key The key
     * @return The list value, or std::nullopt if not found
     */
    std::optional<List> getList(const std::string& key) const {
        if (!isDictionary()) {
            return std::nullopt;
        }
        auto it = dictValue.find(key);
        if (it == dictValue.end() || !it->second->isList()) {
            return std::nullopt;
        }
        return it->second->getList();
    }

    /**
     * @brief Get a dictionary value from a dictionary
     * @param key The key
     * @return The dictionary value, or std::nullopt if not found
     */
    std::optional<Dictionary> getDictionary(const std::string& key) const {
        if (!isDictionary()) {
            return std::nullopt;
        }
        auto it = dictValue.find(key);
        if (it == dictValue.end() || !it->second->isDictionary()) {
            return std::nullopt;
        }
        return it->second->getDict();
    }
};

} // namespace bencode

/**
 * @brief Test the BencodeValue constructors and type checking
 * @return True if all tests pass, false otherwise
 */
bool testBencodeValueConstructors() {
    std::cout << "Testing BencodeValue constructors..." << std::endl;

    // Test default constructor (creates a string value)
    bencode::BencodeValue defaultValue;
    if (!defaultValue.isString()) {
        std::cerr << "Default constructor did not create a string value" << std::endl;
        return false;
    }

    // Test string constructor
    bencode::BencodeValue stringValue("test");
    if (!stringValue.isString()) {
        std::cerr << "String constructor did not create a string value" << std::endl;
        return false;
    }
    if (stringValue.getString() != "test") {
        std::cerr << "String constructor created incorrect value" << std::endl;
        std::cerr << "Expected: 'test', Got: '" << stringValue.getString() << "'" << std::endl;
        return false;
    }

    // Test integer constructor
    bencode::BencodeValue intValue(42);
    if (!intValue.isInteger()) {
        std::cerr << "Integer constructor did not create an integer value" << std::endl;
        return false;
    }
    if (intValue.getInteger() != 42) {
        std::cerr << "Integer constructor created incorrect value" << std::endl;
        std::cerr << "Expected: 42, Got: " << intValue.getInteger() << std::endl;
        return false;
    }

    // Test list constructor
    bencode::List list;
    list.push_back(std::make_shared<bencode::BencodeValue>("item1"));
    list.push_back(std::make_shared<bencode::BencodeValue>(123));
    bencode::BencodeValue listValue(list);
    if (!listValue.isList()) {
        std::cerr << "List constructor did not create a list value" << std::endl;
        return false;
    }
    if (listValue.getList().size() != 2) {
        std::cerr << "List constructor created incorrect list size" << std::endl;
        std::cerr << "Expected: 2, Got: " << listValue.getList().size() << std::endl;
        return false;
    }

    // Test dictionary constructor
    bencode::Dictionary dict;
    dict["key1"] = std::make_shared<bencode::BencodeValue>("value1");
    dict["key2"] = std::make_shared<bencode::BencodeValue>(456);
    bencode::BencodeValue dictValue(dict);
    if (!dictValue.isDictionary()) {
        std::cerr << "Dictionary constructor did not create a dictionary value" << std::endl;
        return false;
    }
    if (dictValue.getDict().size() != 2) {
        std::cerr << "Dictionary constructor created incorrect dictionary size" << std::endl;
        std::cerr << "Expected: 2, Got: " << dictValue.getDict().size() << std::endl;
        return false;
    }

    std::cout << "BencodeValue constructors tests passed!" << std::endl;
    return true;
}

/**
 * @brief Main function
 * @return 0 if all tests pass, 1 otherwise
 */
int main() {
    // Run the tests
    bool allTestsPassed = true;

    allTestsPassed &= testBencodeValueConstructors();

    if (allTestsPassed) {
        std::cout << "All Bencode tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some Bencode tests failed!" << std::endl;
        return 1;
    }
}
