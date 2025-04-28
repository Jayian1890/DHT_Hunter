#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace dht_hunter::utility::hash {

/**
 * @brief Computes the SHA-1 hash of the input data
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @param output Output buffer for the hash (must be at least 20 bytes)
 */
void sha1(const uint8_t* data, size_t length, uint8_t* output);

/**
 * @brief Computes the SHA-1 hash of the input data
 * @param data The input data
 * @return The SHA-1 hash as a 20-byte array
 */
std::array<uint8_t, 20> sha1(const std::vector<uint8_t>& data);

/**
 * @brief Computes the SHA-1 hash of the input string
 * @param data The input string
 * @return The SHA-1 hash as a 20-byte array
 */
std::array<uint8_t, 20> sha1(const std::string& data);

} // namespace dht_hunter::utility::hash
