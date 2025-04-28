#pragma once

#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/utility/node/node_utils.hpp"
#include <string>
#include <vector>
#include <memory>

namespace dht_hunter::dht {

/**
 * @brief Utility functions for node lookups
 */
namespace node_lookup_utils {

/**
 * @brief Sorts nodes by distance to a target
 * @param nodes The nodes to sort
 * @param targetID The target ID
 * @return The sorted nodes
 */
inline std::vector<std::shared_ptr<Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const NodeID& targetID) {
    return utility::node::sortNodesByDistance(nodes, targetID);
}

/**
 * @brief Finds a node by endpoint
 * @param nodes The nodes to search
 * @param endpoint The endpoint to find
 * @return The node, or nullptr if not found
 */
inline std::shared_ptr<Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const EndPoint& endpoint) {
    return utility::node::findNodeByEndpoint(nodes, endpoint);
}

/**
 * @brief Finds a node by ID
 * @param nodes The nodes to search
 * @param nodeID The node ID to find
 * @return The node, or nullptr if not found
 */
inline std::shared_ptr<Node> findNodeByID(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const NodeID& nodeID) {
    return utility::node::findNodeByID(nodes, nodeID);
}

/**
 * @brief Generates a lookup ID from a node ID
 * @param nodeID The node ID
 * @return The lookup ID
 */
inline std::string generateLookupID(const NodeID& nodeID) {
    return utility::node::generateNodeLookupID(nodeID);
}

} // namespace node_lookup_utils

} // namespace dht_hunter::dht
