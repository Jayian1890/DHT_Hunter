#pragma once

#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/info_hash.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include <chrono>
#include <memory>

namespace dht_hunter::types {

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
 * @brief A node in the DHT network
 */
class Node {
public:
    /**
     * @brief Constructs a node
     * @param id The node ID
     * @param endpoint The endpoint of the node
     */
    Node(const NodeID& id, const EndPoint& endpoint);

    /**
     * @brief Gets the node ID
     * @return The node ID
     */
    const NodeID& getID() const;

    /**
     * @brief Gets the endpoint of the node
     * @return The endpoint of the node
     */
    const EndPoint& getEndpoint() const;

    /**
     * @brief Sets the endpoint of the node
     * @param endpoint The endpoint of the node
     */
    void setEndpoint(const EndPoint& endpoint);

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
    EndPoint m_endpoint;
    std::chrono::steady_clock::time_point m_lastSeen;
};

} // namespace dht_hunter::types
