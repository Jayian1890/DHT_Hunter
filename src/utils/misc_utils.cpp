#include "utils/misc_utils.hpp"
#include "utils/common_utils.hpp"
#include <sstream>
#include <iomanip>

namespace dht_hunter::utility {

//
// Random Utilities
//
namespace random {

// Thread-local random generator for better performance
thread_local std::mt19937 tl_generator(std::random_device{}());

std::vector<uint8_t> generateRandomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    std::uniform_int_distribution<> dist(0, 255);

    for (size_t i = 0; i < length; ++i) {
        bytes[i] = static_cast<uint8_t>(dist(tl_generator));
    }

    return bytes;
}

std::string generateRandomHexString(size_t length) {
    auto bytes = generateRandomBytes(length);
    return string::bytesToHex(bytes.data(), bytes.size());
}

std::string generateTransactionID(size_t length) {
    return generateRandomHexString(length);
}

std::string generateToken(size_t length) {
    return generateRandomHexString(length);
}

RandomGenerator::RandomGenerator() : m_generator(std::random_device{}()) {
}

int RandomGenerator::getRandomInt(int min, int max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::uniform_int_distribution<> dist(min, max);
    return dist(m_generator);
}

uint8_t RandomGenerator::getRandomByte() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::uniform_int_distribution<> dist(0, 255);
    return static_cast<uint8_t>(dist(m_generator));
}

std::vector<uint8_t> RandomGenerator::getRandomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);

    std::lock_guard<std::mutex> lock(m_mutex);
    std::uniform_int_distribution<> dist(0, 255);

    for (size_t i = 0; i < length; ++i) {
        bytes[i] = static_cast<uint8_t>(dist(m_generator));
    }

    return bytes;
}

} // namespace random

//
// JSON Utilities
//
namespace json {

// Forward declarations for JsonValue methods
std::string JsonValue::toString(bool pretty) const {
    std::ostringstream oss;
    
    if (isNull()) {
        oss << "null";
    } else if (isBool()) {
        oss << (getBool() ? "true" : "false");
    } else if (isNumber()) {
        oss << getNumber();
    } else if (isString()) {
        oss << "\"" << getString() << "\"";
    } else if (isObject()) {
        oss << "{";
        if (pretty) oss << "\n";
        
        const auto& obj = getObject();
        const auto& map = obj->getMap();
        bool first = true;
        
        for (const auto& pair : map) {
            if (!first) {
                oss << ",";
                if (pretty) oss << "\n";
            }
            first = false;
            
            if (pretty) oss << "  ";
            oss << "\"" << pair.first << "\":";
            if (pretty) oss << " ";
            oss << pair.second.toString(pretty);
        }
        
        if (pretty && !map.empty()) oss << "\n";
        oss << "}";
    } else if (isArray()) {
        oss << "[";
        if (pretty) oss << "\n";
        
        const auto& arr = getArray();
        const auto& vec = arr->getVector();
        bool first = true;
        
        for (const auto& val : vec) {
            if (!first) {
                oss << ",";
                if (pretty) oss << "\n";
            }
            first = false;
            
            if (pretty) oss << "  ";
            oss << val.toString(pretty);
        }
        
        if (pretty && !vec.empty()) oss << "\n";
        oss << "]";
    }
    
    return oss.str();
}

JsonValue JsonValue::parse(const std::string& json) {
    // Simple JSON parser implementation
    // This is a placeholder - in a real implementation, you would use a more robust parser
    // For now, we'll just return a null value
    return JsonValue(nullptr);
}

JsonValue::ObjectType JsonValue::createObject() {
    return std::make_shared<JsonObject>();
}

JsonValue::ArrayType JsonValue::createArray() {
    return std::make_shared<JsonArray>();
}

} // namespace json

} // namespace dht_hunter::utility
