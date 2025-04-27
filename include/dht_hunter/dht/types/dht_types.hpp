#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/dht/types/node_id.hpp"

namespace dht_hunter::dht {

/**
 * @brief Type alias for an info hash (20 bytes)
 */
using InfoHash = std::array<uint8_t, 20>;

/**
 * @brief Type alias for a transaction ID
 */
using TransactionID = std::string;

/**
 * @brief Calculates the distance between two node IDs
 * @param a The first node ID
 * @param b The second node ID
 * @return The distance between the two node IDs
 */
NodeID calculateDistance(const NodeID& a, const NodeID& b);

/**
 * @brief Converts a node ID to a string
 * @param nodeID The node ID
 * @return The string representation of the node ID
 */
std::string nodeIDToString(const NodeID& nodeID);

/**
 * @brief Converts an info hash to a string
 * @param infoHash The info hash
 * @return The string representation of the info hash
 */
std::string infoHashToString(const InfoHash& infoHash);

/**
 * @brief Generates a random node ID
 * @return A random node ID
 */
NodeID generateRandomNodeID();

/**
 * @brief Creates an empty info hash (all zeros)
 * @return An empty info hash
 */
InfoHash createEmptyInfoHash();

/**
 * @brief Checks if a node ID is valid
 * @param nodeID The node ID to check
 * @return True if the node ID is valid, false otherwise
 */
bool isValidNodeID(const NodeID& nodeID);

/**
 * @brief Checks if an info hash is valid
 * @param infoHash The info hash to check
 * @return True if the info hash is valid, false otherwise
 */
bool isValidInfoHash(const InfoHash& infoHash);

/**
 * @brief A node in the DHT network
 */
class Node {
public:
    /**
     * @brief Constructs a node
     * @param id The node ID
     * @param endpoint The endpoint of the node
     */
    Node(const NodeID& id, const network::EndPoint& endpoint);

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const NodeID& getID() const;

    /**
     * @brief Gets the endpoint of the node
     * @return The endpoint of the node
     */
    const network::EndPoint& getEndpoint() const;

    /**
     * @brief Sets the endpoint of the node
     * @param endpoint The endpoint of the node
     */
    void setEndpoint(const network::EndPoint& endpoint);

    /**
     * @brief Gets the last seen time
     * @return The last seen time
     */
    std::chrono::steady_clock::time_point getLastSeen() const;

    /**
     * @brief Updates the last seen time to now
     */
    void updateLastSeen();

private:
    NodeID m_id;
    network::EndPoint m_endpoint;
    std::chrono::steady_clock::time_point m_lastSeen;
};

} // namespace dht_hunter::dht

// Add hash support for NodeID and InfoHash
namespace dht_hunter::dht {
    // Common hash function for both NodeID and InfoHash
    template<typename T>
    size_t hashBytes(const T& bytes) {
        size_t result = 0;
        for (size_t i = 0; i < bytes.size(); ++i) {
            result = result * 31 + bytes[i];
        }
        return result;
    }
}

// Define a single hash specialization for std::array<uint8_t, 20>
// This will work for both NodeID and InfoHash since they're both aliases for the same type
namespace std {
    template<>
    struct hash<std::array<uint8_t, 20>> {
        size_t operator()(const std::array<uint8_t, 20>& bytes) const {
            return dht_hunter::dht::hashBytes(bytes);
        }
    };
}
