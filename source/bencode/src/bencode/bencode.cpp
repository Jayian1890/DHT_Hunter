#include "bencode/bencode.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace bencode {
// BencodeValue implementation
BencodeValue::BencodeValue() : m_value(String()) {}
BencodeValue::BencodeValue(const String& value) : m_value(value) {}
BencodeValue::BencodeValue(Integer value) : m_value(value) {}
BencodeValue::BencodeValue(const List& value) : m_value(value) {}
BencodeValue::BencodeValue(const Dictionary& value) : m_value(value) {}

std::size_t BencodeValue::getType() const {
    return m_value.index();
}

bool BencodeValue::isString() const {
    return std::holds_alternative<String>(m_value);
}

bool BencodeValue::isInteger() const {
    return std::holds_alternative<Integer>(m_value);
}

bool BencodeValue::isList() const {
    return std::holds_alternative<List>(m_value);
}

bool BencodeValue::isDictionary() const {
    return std::holds_alternative<Dictionary>(m_value);
}

const BencodeValue::String& BencodeValue::getString() const {
    if (!isString()) {
        throw BencodeException("Value is not a string");
    }
    return std::get<String>(m_value);
}

BencodeValue::Integer BencodeValue::getInteger() const {
    if (!isInteger()) {
        throw BencodeException("Value is not an integer");
    }
    return std::get<Integer>(m_value);
}

const BencodeValue::List& BencodeValue::getList() const {
    if (!isList()) {
        throw BencodeException("Value is not a list");
    }
    return std::get<List>(m_value);
}

std::shared_ptr<BencodeValue> BencodeValue::at(size_t index) const {
    if (!isList()) {
        throw BencodeException("Value is not a list");
    }
    const auto& list = std::get<List>(m_value);
    if (index >= list.size()) {
        throw BencodeException("Index out of range");
    }
    return list[index];
}

size_t BencodeValue::size() const {
    if (!isList()) {
        throw BencodeException("Value is not a list");
    }
    const auto& list = std::get<List>(m_value);
    return list.size();
}

const BencodeValue::Dictionary& BencodeValue::getDict() const {
    if (!isDictionary()) {
        throw BencodeException("Value is not a dictionary");
    }
    return std::get<Dictionary>(m_value);
}

std::optional<BencodeValue::Dictionary> BencodeValue::getDict(const std::string& key) const {
    return getDictionary(key);
}

bool BencodeValue::contains(const std::string& key) const {
    if (!isDictionary()) {
        throw BencodeException("Value is not a dictionary");
    }
    const auto& dict = std::get<Dictionary>(m_value);
    return dict.find(key) != dict.end();
}

std::shared_ptr<BencodeValue> BencodeValue::get(const std::string& key) const {
    if (!isDictionary()) {
        throw BencodeException("Value is not a dictionary");
    }
    const auto& dict = std::get<Dictionary>(m_value);
    auto it = dict.find(key);
    if (it != dict.end()) {
        return it->second;
    }
    return nullptr;
}

std::optional<BencodeValue::String> BencodeValue::getString(const std::string& key) const {
    auto value = get(key);
    if (value && value->isString()) {
        return value->getString();
    }
    return std::nullopt;
}

std::optional<BencodeValue::Integer> BencodeValue::getInteger(const std::string& key) const {
    auto value = get(key);
    if (value && value->isInteger()) {
        return value->getInteger();
    }
    return std::nullopt;
}

std::optional<BencodeValue::List> BencodeValue::getList(const std::string& key) const {
    auto value = get(key);
    if (value && value->isList()) {
        return value->getList();
    }
    return std::nullopt;
}

std::optional<BencodeValue::Dictionary> BencodeValue::getDictionary(const std::string& key) const {
    auto value = get(key);
    if (value && value->isDictionary()) {
        return value->getDict();
    }
    return std::nullopt;
}

void BencodeValue::set(const std::string& key, std::shared_ptr<BencodeValue> value) {
    if (!isDictionary()) {
        throw BencodeException("Value is not a dictionary");
    }
    auto& dict = std::get<Dictionary>(m_value);
    dict[key] = value;
}

void BencodeValue::setString(const std::string& key, const String& value) {
    set(key, std::make_shared<BencodeValue>(value));
}

void BencodeValue::setInteger(const std::string& key, Integer value) {
    set(key, std::make_shared<BencodeValue>(value));
}

void BencodeValue::setList(const std::string& key, const List& value) {
    set(key, std::make_shared<BencodeValue>(value));
}

void BencodeValue::setList(const List& value) {
    m_value = value;
}

void BencodeValue::setDict(const std::string& key, const Dictionary& value) {
    set(key, std::make_shared<BencodeValue>(value));
}

void BencodeValue::setDict(const Dictionary& value) {
    m_value = value;
}

void BencodeValue::add(std::shared_ptr<BencodeValue> value) {
    if (!isList()) {
        throw BencodeException("Value is not a list");
    }
    auto& list = std::get<List>(m_value);
    list.push_back(value);
}

void BencodeValue::addString(const String& value) {
    add(std::make_shared<BencodeValue>(value));
}

void BencodeValue::addInteger(Integer value) {
    add(std::make_shared<BencodeValue>(value));
}

void BencodeValue::addList(const List& value) {
    add(std::make_shared<BencodeValue>(value));
}

void BencodeValue::addDictionary(const Dictionary& value) {
    add(std::make_shared<BencodeValue>(value));
}

const BencodeValue::Value& BencodeValue::getValue() const {
    return m_value;
}

BencodeValue::Value& BencodeValue::getValue() {
    return m_value;
}

void BencodeValue::setString(const String& str) {
    m_value = str;
}

void BencodeValue::setInteger(Integer value) {
    m_value = value;
}

BencodeValue& BencodeValue::operator=(const std::string& str) {
    setString(str);
    return *this;
}

BencodeValue& BencodeValue::operator=(int64_t value) {
    setInteger(value);
    return *this;
}

BencodeValue& BencodeValue::operator=(const BencodeList& list) {
    setList(list);
    return *this;
}

BencodeValue& BencodeValue::operator=(const Dictionary& dict) {
    setDict(dict);
    return *this;
}

// BencodeEncoder implementation
std::string BencodeEncoder::encode(const std::shared_ptr<BencodeValue>& value) {
    if (!value) {
        throw BencodeException("Null BencodeValue");
    }
    return encode(*value);
}

std::string BencodeEncoder::encode(const BencodeValue& value) {
    if (value.isString()) {
        return encodeString(value.getString());
    } else if (value.isInteger()) {
        return encodeInteger(value.getInteger());
    } else if (value.isList()) {
        return encodeList(value.getList());
    } else if (value.isDictionary()) {
        return encodeDictionary(value.getDict());
    }
    throw BencodeException("Unknown value type");
}

std::string BencodeEncoder::encodeString(const std::string& value) {
    return std::to_string(value.size()) + ":" + value;
}

std::string BencodeEncoder::encodeInteger(int64_t value) {
    return "i" + std::to_string(value) + "e";
}

std::string BencodeEncoder::encodeList(const BencodeValue::List& value) {
    std::string result = "l";
    for (const auto& item : value) {
        result += encode(*item);
    }
    result += "e";
    return result;
}

std::string BencodeEncoder::encodeDictionary(const BencodeValue::Dictionary& value) {
    std::string result = "d";
    // Sort keys for canonical encoding
    std::vector<std::string> keys;
    keys.reserve(value.size());
    for (const auto& [key, _] : value) {
        keys.push_back(key);
    }
    std::sort(keys.begin(), keys.end());
    for (const auto& key : keys) {
        result += encodeString(key);
        result += encode(*value.at(key));
    }
    result += "e";
    return result;
}

// BencodeDecoder implementation
std::shared_ptr<BencodeValue> BencodeDecoder::decode(const std::string& data) {
    size_t pos = 0;
    auto result = decode(data, pos);
    if (pos != data.size()) {
        std::stringstream ss;
        ss << "Decoded only " << pos << " of " << data.size() << " bytes";
        std::cerr << "[WARNING] [Bencode.Parser] " << ss.str() << std::endl;
    }
    return result;
}

std::shared_ptr<BencodeValue> BencodeDecoder::decode(const std::string& data, size_t& pos) {
    if (pos >= data.size()) {
        throw BencodeException("Unexpected end of data");
    }
    char c = data[pos];
    if (c == 'i') {
        // Integer
        return std::make_shared<BencodeValue>(decodeInteger(data, pos));
    } else if (c == 'l') {
        // List
        return std::make_shared<BencodeValue>(decodeList(data, pos));
    } else if (c == 'd') {
        // Dictionary
        return std::make_shared<BencodeValue>(decodeDictionary(data, pos));
    } else if (c >= '0' && c <= '9') {
        // String
        return std::make_shared<BencodeValue>(decodeString(data, pos));
    }
    throw BencodeException("Invalid bencode data at position " + std::to_string(pos));
}

std::string BencodeDecoder::decodeString(const std::string& data, size_t& pos) {
    size_t lengthStart = pos;
    size_t lengthEnd = data.find(':', pos);
    if (lengthEnd == std::string::npos) {
        throw BencodeException("Invalid string format: no colon found");
    }
    std::string lengthStr = data.substr(lengthStart, lengthEnd - lengthStart);
    size_t length;
    try {
        length = std::stoul(lengthStr);
    } catch (const std::exception& e) {
        throw BencodeException("Invalid string length: " + lengthStr);
    }
    pos = lengthEnd + 1;
    if (pos + length > data.size()) {
        throw BencodeException("String length exceeds data size");
    }
    std::string result = data.substr(pos, length);
    pos += length;
    return result;
}

int64_t BencodeDecoder::decodeInteger(const std::string& data, size_t& pos) {
    if (data[pos] != 'i') {
        throw BencodeException("Invalid integer format: does not start with 'i'");
    }
    size_t valueStart = pos + 1;
    size_t valueEnd = data.find('e', valueStart);
    if (valueEnd == std::string::npos) {
        throw BencodeException("Invalid integer format: no end marker 'e' found");
    }
    std::string valueStr = data.substr(valueStart, valueEnd - valueStart);
    int64_t value;
    try {
        value = std::stoll(valueStr);
    } catch (const std::exception& e) {
        throw BencodeException("Invalid integer value: " + valueStr);
    }
    pos = valueEnd + 1;
    return value;
}

BencodeValue::List BencodeDecoder::decodeList(const std::string& data, size_t& pos) {
    if (data[pos] != 'l') {
        throw BencodeException("Invalid list format: does not start with 'l'");
    }
    pos++;
    BencodeValue::List result;
    while (pos < data.size() && data[pos] != 'e') {
        result.push_back(decode(data, pos));
    }
    if (pos >= data.size()) {
        throw BencodeException("Invalid list format: no end marker 'e' found");
    }
    pos++; // Skip 'e'
    return result;
}

BencodeValue::Dictionary BencodeDecoder::decodeDictionary(const std::string& data, size_t& pos) {
    if (data[pos] != 'd') {
        throw BencodeException("Invalid dictionary format: does not start with 'd'");
    }
    pos++;
    BencodeValue::Dictionary result;
    while (pos < data.size() && data[pos] != 'e') {
        // Decode key (must be a string)
        if (pos >= data.size() || data[pos] < '0' || data[pos] > '9') {
            throw BencodeException("Invalid dictionary key: not a string");
        }
        std::string key = decodeString(data, pos);
        // Decode value
        auto value = decode(data, pos);
        result[key] = value;
    }
    if (pos >= data.size()) {
        throw BencodeException("Invalid dictionary format: no end marker 'e' found");
    }
    pos++; // Skip 'e'
    return result;
}

// BencodeValue encode and decode methods
std::string BencodeValue::encode() const {
    return BencodeEncoder::encode(*this);
}

std::shared_ptr<BencodeValue> BencodeValue::decode(const char* data, size_t size) {
    return BencodeDecoder::decode(std::string(data, size));
}

} // namespace bencode
