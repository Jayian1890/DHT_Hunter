#pragma once

#include "dht_hunter/dht/types.hpp"
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
std::vector<std::shared_ptr<Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const NodeID& targetID);

/**
 * @brief Finds a node by endpoint
 * @param nodes The nodes to search
 * @param endpoint The endpoint to find
 * @return The node, or nullptr if not found
 */
std::shared_ptr<Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const EndPoint& endpoint);

/**
 * @brief Finds a node by ID
 * @param nodes The nodes to search
 * @param nodeID The node ID to find
 * @return The node, or nullptr if not found
 */
std::shared_ptr<Node> findNodeByID(
    const std::vector<std::shared_ptr<Node>>& nodes,
    const NodeID& nodeID);

/**
 * @brief Generates a lookup ID from a node ID
 * @param nodeID The node ID
 * @return The lookup ID
 */
std::string generateLookupID(const NodeID& nodeID);

} // namespace node_lookup_utils

} // namespace dht_hunter::dht
