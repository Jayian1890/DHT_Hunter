#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <random>
#include <mutex>

namespace dht_hunter::utility::random {

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

} // namespace dht_hunter::utility::random
