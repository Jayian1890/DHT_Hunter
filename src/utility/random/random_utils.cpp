#include "dht_hunter/utility/random/random_utils.hpp"
#include "dht_hunter/utility/string/string_utils.hpp"
#include <sstream>
#include <iomanip>
#include <mutex>

namespace dht_hunter::utility::random {

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

} // namespace dht_hunter::utility::random
