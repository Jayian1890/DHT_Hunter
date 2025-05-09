#include "dht_hunter/dht/node_lookup/node_lookup_response_handler.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_utils.hpp"
#include <algorithm>

// Constants
constexpr int DEFAULT_MAX_RESULTS = 20;

namespace dht_hunter::dht {

NodeLookupResponseHandler::NodeLookupResponseHandler(const DHTConfig& config,
                                                   const NodeID& nodeID,
                                                   std::shared_ptr<RoutingTable> routingTable)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable) {
}

bool NodeLookupResponseHandler::handleResponse(
    const std::string& /*lookupID*/,
    NodeLookupState& lookup,
    std::shared_ptr<FindNodeResponse> response,
    const network::EndPoint& /*sender*/) {

    // Variables to store data we'll need after processing
    std::vector<std::shared_ptr<Node>> nodesToAdd;
    bool shouldSendQueries = false;

    // Get the node ID from the response
    if (!response->getNodeID()) {
        return false;
    }

    std::string nodeIDStr = nodeIDToString(response->getNodeID().value());

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Add the node to the responded nodes
    lookup.respondedNodes.insert(nodeIDStr);

    // Process the nodes from the response
    const auto& nodes = response->getNodes();
    if (!nodes.empty()) {
        for (const auto& node : nodes) {
            // Skip our own node
            if (node->getID() == m_nodeID) {
                continue;
            }

            // Check if the node is already in the list
            auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
                [&node](const std::shared_ptr<Node>& existingNode) {
                    return existingNode->getID() == node->getID();
                });

            if (nodeIt == lookup.nodes.end()) {
                // Add the node to the lookup's node list
                lookup.nodes.push_back(node);

                // Store the node for adding to the routing table
                nodesToAdd.push_back(node);
            }
        }
    }

    // Set flag to send more queries
    shouldSendQueries = true;

    // Add the nodes to the routing table
    if (m_routingTable && !nodesToAdd.empty()) {
        for (const auto& node : nodesToAdd) {
            m_routingTable->addNode(node);
        }
    }

    return shouldSendQueries;
}

bool NodeLookupResponseHandler::handleError(
    NodeLookupState& lookup,
    std::shared_ptr<ErrorMessage> /*error*/,
    const network::EndPoint& sender) {

    // Find the node
    auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
        [&sender](const std::shared_ptr<Node>& node) {
            return node->getEndpoint() == sender;
        });

    if (nodeIt == lookup.nodes.end()) {
        return false;
    }

    std::string nodeIDStr = nodeIDToString((*nodeIt)->getID());

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    return true;
}

bool NodeLookupResponseHandler::handleTimeout(
    NodeLookupState& lookup,
    const NodeID& nodeID) {

    std::string nodeIDStr = nodeIDToString(nodeID);

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    return true;
}

void NodeLookupResponseHandler::completeLookup(NodeLookupState& lookup) const {
    // Call the callback
    if (lookup.callback) {
        // Sort the nodes by distance to the target
        std::vector<std::shared_ptr<Node>> sortedNodes =
            node_lookup_utils::sortNodesByDistance(lookup.nodes, lookup.targetID);

        // Limit the number of nodes to return
        if (sortedNodes.size() > DEFAULT_MAX_RESULTS) {
            sortedNodes.resize(DEFAULT_MAX_RESULTS);
        }

        lookup.callback(sortedNodes);
    }
}

} // namespace dht_hunter::dht
