#pragma once

#include "../common.hpp"

namespace dht_hunter::types {

/**
 * @brief Represents an InfoHash (SHA-1 hash of a torrent's info dictionary)
 */
class InfoHash {
public:
    /**
     * @brief Default constructor
     */
    InfoHash();

    /**
     * @brief Constructor from a byte array
     * @param bytes The byte array (must be 20 bytes)
     */
    explicit InfoHash(const std::vector<uint8_t>& bytes);

    /**
     * @brief Constructor from a hex string
     * @param hexString The hex string (must be 40 characters)
     */
    explicit InfoHash(const std::string& hexString);

    /**
     * @brief Gets the bytes of the InfoHash
     * @return The bytes
     */
    const std::vector<uint8_t>& getBytes() const;

    /**
     * @brief Gets the hex string representation of the InfoHash
     * @return The hex string
     */
    std::string getHexString() const;

    /**
     * @brief Checks if the InfoHash is valid
     * @return True if the InfoHash is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Equality operator
     * @param other The other InfoHash
     * @return True if the InfoHashes are equal, false otherwise
     */
    bool operator==(const InfoHash& other) const;

    /**
     * @brief Inequality operator
     * @param other The other InfoHash
     * @return True if the InfoHashes are not equal, false otherwise
     */
    bool operator!=(const InfoHash& other) const;

    /**
     * @brief Less than operator (for use in maps/sets)
     * @param other The other InfoHash
     * @return True if this InfoHash is less than the other, false otherwise
     */
    bool operator<(const InfoHash& other) const;

private:
    std::vector<uint8_t> m_bytes;
};

} // namespace dht_hunter::types

// Hash function for InfoHash
namespace std {
    template<>
    struct hash<dht_hunter::types::InfoHash> {
        size_t operator()(const dht_hunter::types::InfoHash& infoHash) const {
            const auto& bytes = infoHash.getBytes();
            size_t result = 0;
            for (size_t i = 0; i < bytes.size(); ++i) {
                result = (result * 31) + bytes[i];
            }
            return result;
        }
    };
}
