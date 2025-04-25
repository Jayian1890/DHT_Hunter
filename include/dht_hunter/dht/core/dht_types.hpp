#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>
#include "dht_hunter/network/network_address.hpp"

namespace dht_hunter::dht {

/**
 * @brief Type alias for a node ID (20 bytes)
 */
using NodeID = std::array<uint8_t, 20>;

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
 * @brief Converts a string to a node ID
 * @param str The string representation of the node ID
 * @return The node ID, or an empty optional if the string is invalid
 */
std::optional<NodeID> stringToNodeID(const std::string& str);

/**
 * @brief Converts an info hash to a string
 * @param infoHash The info hash
 * @return The string representation of the info hash
 */
std::string infoHashToString(const InfoHash& infoHash);

/**
 * @brief Converts a string to an info hash
 * @param str The string representation of the info hash
 * @return The info hash, or an empty optional if the string is invalid
 */
std::optional<InfoHash> stringToInfoHash(const std::string& str);

/**
 * @brief Generates a random node ID
 * @return A random node ID
 */
NodeID generateRandomNodeID();

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
     * @brief Gets the node ID (alias for getID for compatibility)
     * @return The node ID
     */
    const NodeID& getNodeID() const { return getID(); }

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

private:
    NodeID m_id;
    network::EndPoint m_endpoint;
};

// Custom hash function for NodeID and InfoHash
} // namespace dht_hunter::dht

// Add hash support for NodeID and InfoHash
namespace std {
    template<>
    struct hash<dht_hunter::dht::NodeID> {
        size_t operator()(const dht_hunter::dht::NodeID& id) const {
            size_t result = 0;
            for (size_t i = 0; i < id.size(); ++i) {
                result = result * 31 + id[i];
            }
            return result;
        }
    };
}
