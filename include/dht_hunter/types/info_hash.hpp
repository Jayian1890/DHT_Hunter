#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace dht_hunter::types {

/**
 * @brief Type alias for an info hash (20 bytes)
 */
using InfoHash = std::array<uint8_t, 20>;

/**
 * @brief Converts an info hash to a string
 * @param infoHash The info hash
 * @return The string representation of the info hash
 */
std::string infoHashToString(const InfoHash& infoHash);

/**
 * @brief Converts a string to an info hash
 * @param str The string representation of the info hash
 * @param infoHash The info hash to populate
 * @return True if the conversion was successful, false otherwise
 */
bool infoHashFromString(const std::string& str, InfoHash& infoHash);

/**
 * @brief Creates an empty info hash (all zeros)
 * @return An empty info hash
 */
InfoHash createEmptyInfoHash();

/**
 * @brief Checks if an info hash is valid
 * @param infoHash The info hash to check
 * @return True if the info hash is valid, false otherwise
 */
bool isValidInfoHash(const InfoHash& infoHash);

} // namespace dht_hunter::types

// Add hash support for InfoHash
namespace std {
    template<>
    struct hash<dht_hunter::types::InfoHash> {
        size_t operator()(const dht_hunter::types::InfoHash& infoHash) const {
            size_t result = 0;
            for (size_t i = 0; i < infoHash.size(); ++i) {
                result = result * 31 + infoHash[i];
            }
            return result;
        }
    };
}
