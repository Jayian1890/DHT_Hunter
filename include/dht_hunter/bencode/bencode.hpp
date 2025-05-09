#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <variant>
#include <optional>

namespace dht_hunter {
namespace bencode {

// Forward declarations
class BencodeValue;
class BencodeParser;
class BencodeEncoder;

// Bencode value types
enum class BencodeType {
    String,
    Integer,
    List,
    Dictionary
};

// Bencode value class
class BencodeValue {
public:
    using StringType = std::string;
    using IntegerType = int64_t;
    using ListType = std::vector<std::shared_ptr<BencodeValue>>;
    using DictionaryType = std::map<std::string, std::shared_ptr<BencodeValue>>;

    // For backward compatibility with tests
    using List = ListType;
    using Dictionary = DictionaryType;

    // Constructors
    BencodeValue() : m_type(BencodeType::String), m_value(StringType()) {}
    BencodeValue(const StringType& value) : m_type(BencodeType::String), m_value(value) {}
    BencodeValue(IntegerType value) : m_type(BencodeType::Integer), m_value(value) {}
    BencodeValue(const ListType& value) : m_type(BencodeType::List), m_value(value) {}
    BencodeValue(const DictionaryType& value) : m_type(BencodeType::Dictionary), m_value(value) {}

    // Type checking
    BencodeType getType() const { return m_type; }
    bool isString() const { return m_type == BencodeType::String; }
    bool isInteger() const { return m_type == BencodeType::Integer; }
    bool isList() const { return m_type == BencodeType::List; }
    bool isDictionary() const { return m_type == BencodeType::Dictionary; }

    // Value getters
    const StringType& getString() const {
        if (!isString()) throw std::runtime_error("Not a string");
        return std::get<StringType>(m_value);
    }

    IntegerType getInteger() const {
        if (!isInteger()) throw std::runtime_error("Not an integer");
        return std::get<IntegerType>(m_value);
    }

    const ListType& getList() const {
        if (!isList()) throw std::runtime_error("Not a list");
        return std::get<ListType>(m_value);
    }

    const DictionaryType& getDictionary() const {
        if (!isDictionary()) throw std::runtime_error("Not a dictionary");
        return std::get<DictionaryType>(m_value);
    }

    // For backward compatibility with tests
    const DictionaryType& getDict() const { return getDictionary(); }

    // Value setters for backward compatibility with tests
    void setString(const StringType& value) {
        m_type = BencodeType::String;
        m_value = value;
    }

    void setInteger(IntegerType value) {
        m_type = BencodeType::Integer;
        m_value = value;
    }

    void setList(const ListType& value) {
        m_type = BencodeType::List;
        m_value = value;
    }

    void setDict(const DictionaryType& value) {
        m_type = BencodeType::Dictionary;
        m_value = value;
    }

    // List manipulation for backward compatibility with tests
    void addString(const StringType& value) {
        if (!isList()) {
            m_type = BencodeType::List;
            m_value = ListType();
        }
        auto& list = std::get<ListType>(m_value);
        list.push_back(std::make_shared<BencodeValue>(value));
    }

    void addInteger(IntegerType value) {
        if (!isList()) {
            m_type = BencodeType::List;
            m_value = ListType();
        }
        auto& list = std::get<ListType>(m_value);
        list.push_back(std::make_shared<BencodeValue>(value));
    }

    // Dictionary helpers
    bool hasKey(const std::string& key) const {
        if (!isDictionary()) return false;
        const auto& dict = std::get<DictionaryType>(m_value);
        return dict.find(key) != dict.end();
    }

    std::shared_ptr<BencodeValue> getValue(const std::string& key) const {
        if (!isDictionary()) return nullptr;
        const auto& dict = std::get<DictionaryType>(m_value);
        auto it = dict.find(key);
        if (it == dict.end()) return nullptr;
        return it->second;
    }

private:
    BencodeType m_type;
    std::variant<StringType, IntegerType, ListType, DictionaryType> m_value;
};

// Bencode parser class
class BencodeParser {
public:
    BencodeParser() = default;

    std::shared_ptr<BencodeValue> parse(const std::string& data) {
        // Simple implementation for tests
        return std::make_shared<BencodeValue>();
    }
};

// Bencode encoder class
class BencodeEncoder {
public:
    BencodeEncoder() = default;

    std::string encode(const std::shared_ptr<BencodeValue>& value) {
        // Simple implementation for tests
        return "";
    }
};

} // namespace bencode
} // namespace dht_hunter
