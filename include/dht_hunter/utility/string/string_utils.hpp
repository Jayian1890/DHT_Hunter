#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace dht_hunter::utility::string {

/**
 * @brief Converts a byte array to a hex string
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @return The hex string representation
 */
std::string bytesToHex(const uint8_t* data, size_t length);

/**
 * @brief Converts a hex string to a byte vector
 * @param hex The hex string
 * @return The byte vector
 */
std::vector<uint8_t> hexToBytes(const std::string& hex);

/**
 * @brief Formats a byte array as a hex string with optional separator
 * @param data Pointer to the input data
 * @param length Length of the input data
 * @param separator Optional separator between bytes (e.g., ":")
 * @return The formatted hex string
 */
std::string formatHex(const uint8_t* data, size_t length, const std::string& separator = "");

/**
 * @brief Truncates a string to a maximum length and adds an ellipsis if truncated
 * @param str The input string
 * @param maxLength The maximum length
 * @return The truncated string
 */
std::string truncateString(const std::string& str, size_t maxLength);

} // namespace dht_hunter::utility::string
