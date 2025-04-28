#pragma once

#include "dht_hunter/types/node_id.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/types/dht_types.hpp"
#include <vector>
#include <memory>
#include <string>

namespace dht_hunter::utility::node {

/**
 * @brief Sorts nodes by distance to a target
 * @param nodes The nodes to sort
 * @param targetID The target ID
 * @return The sorted nodes
 */
std::vector<std::shared_ptr<types::Node>> sortNodesByDistance(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& targetID);

/**
 * @brief Finds a node by endpoint
 * @param nodes The nodes to search
 * @param endpoint The endpoint to find
 * @return The node, or nullptr if not found
 */
std::shared_ptr<types::Node> findNodeByEndpoint(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::EndPoint& endpoint);

/**
 * @brief Finds a node by ID
 * @param nodes The nodes to search
 * @param nodeID The node ID to find
 * @return The node, or nullptr if not found
 */
std::shared_ptr<types::Node> findNodeByID(
    const std::vector<std::shared_ptr<types::Node>>& nodes, 
    const types::NodeID& nodeID);

/**
 * @brief Generates a lookup ID from a node ID
 * @param nodeID The node ID
 * @return The lookup ID
 */
std::string generateNodeLookupID(const types::NodeID& nodeID);

/**
 * @brief Generates a lookup ID from an info hash
 * @param infoHash The info hash
 * @return The lookup ID
 */
std::string generatePeerLookupID(const types::InfoHash& infoHash);

} // namespace dht_hunter::utility::node
