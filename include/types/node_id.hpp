#pragma once

#include "../common.hpp"

namespace dht_hunter::types {

/**
 * @brief Represents a NodeID in the DHT network
 */
class NodeID {
public:
    /**
     * @brief Default constructor
     */
    NodeID();

    /**
     * @brief Constructor from a byte array
     * @param bytes The byte array (must be 20 bytes)
     */
    explicit NodeID(const std::vector<uint8_t>& bytes);

    /**
     * @brief Constructor from a hex string
     * @param hexString The hex string (must be 40 characters)
     */
    explicit NodeID(const std::string& hexString);

    /**
     * @brief Gets the bytes of the NodeID
     * @return The bytes
     */
    const std::vector<uint8_t>& getBytes() const;

    /**
     * @brief Gets the hex string representation of the NodeID
     * @return The hex string
     */
    std::string getHexString() const;

    /**
     * @brief Checks if the NodeID is valid
     * @return True if the NodeID is valid, false otherwise
     */
    bool isValid() const;

    /**
     * @brief Equality operator
     * @param other The other NodeID
     * @return True if the NodeIDs are equal, false otherwise
     */
    bool operator==(const NodeID& other) const;

    /**
     * @brief Inequality operator
     * @param other The other NodeID
     * @return True if the NodeIDs are not equal, false otherwise
     */
    bool operator!=(const NodeID& other) const;

    /**
     * @brief Less than operator (for use in maps/sets)
     * @param other The other NodeID
     * @return True if this NodeID is less than the other, false otherwise
     */
    bool operator<(const NodeID& other) const;

private:
    std::vector<uint8_t> m_bytes;
};

/**
 * @brief Generates a random NodeID
 * @return The generated NodeID
 */
NodeID generateRandomNodeID();

} // namespace dht_hunter::types

// Hash function for NodeID
namespace std {
    template<>
    struct hash<dht_hunter::types::NodeID> {
        size_t operator()(const dht_hunter::types::NodeID& nodeId) const {
            const auto& bytes = nodeId.getBytes();
            size_t result = 0;
            for (size_t i = 0; i < bytes.size(); ++i) {
                result = (result * 31) + bytes[i];
            }
            return result;
        }
    };
}
