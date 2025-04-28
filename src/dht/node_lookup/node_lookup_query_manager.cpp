#include "dht_hunter/dht/node_lookup/node_lookup_query_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_utils.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include <algorithm>

namespace dht_hunter::dht {

NodeLookupQueryManager::NodeLookupQueryManager(const DHTConfig& config, 
                                             const NodeID& nodeID,
                                             std::shared_ptr<RoutingTable> routingTable,
                                             std::shared_ptr<TransactionManager> transactionManager,
                                             std::shared_ptr<MessageSender> messageSender)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender) {
}

bool NodeLookupQueryManager::sendQueries(
    const std::string& lookupID, 
    NodeLookupState& lookup,
    std::function<void(std::shared_ptr<ResponseMessage>, const network::EndPoint&)> responseCallback,
    std::function<void(std::shared_ptr<ErrorMessage>, const network::EndPoint&)> errorCallback,
    std::function<void(const NodeID&)> timeoutCallback) {
    
    // Check if we've reached the maximum number of iterations
    if (lookup.iteration >= LOOKUP_MAX_ITERATIONS) {
        return false;
    }

    // Increment the iteration counter
    lookup.iteration++;

    // Count the number of queries we need to send
    size_t queriesToSend = 0;
    for (const auto& node : lookup.nodes) {
        // Skip nodes we've already queried
        if (lookup.queriedNodes.find(nodeIDToString(node->getID())) != lookup.queriedNodes.end()) {
            continue;
        }

        // Skip nodes that are already being queried
        if (lookup.activeQueries.find(nodeIDToString(node->getID())) != lookup.activeQueries.end()) {
            continue;
        }

        queriesToSend++;

        // Stop if we've reached the alpha value
        if (queriesToSend >= m_config.getAlpha()) {
            break;
        }
    }

    // If there are no queries to send, check if the lookup is complete
    if (queriesToSend == 0) {
        return false;
    }

    // Send queries
    size_t queriesSent = 0;
    for (const auto& node : lookup.nodes) {
        // Skip nodes we've already queried
        std::string nodeIDStr = nodeIDToString(node->getID());
        if (lookup.queriedNodes.find(nodeIDStr) != lookup.queriedNodes.end()) {
            continue;
        }

        // Skip nodes that are already being queried
        if (lookup.activeQueries.find(nodeIDStr) != lookup.activeQueries.end()) {
            continue;
        }

        // Create a find_node query
        auto query = std::make_shared<FindNodeQuery>("", m_nodeID, lookup.targetID);

        // Send the query
        if (m_transactionManager && m_messageSender) {
            std::string transactionID = m_transactionManager->createTransaction(
                query,
                node->getEndpoint(),
                responseCallback,
                errorCallback,
                [timeoutCallback, node]() {
                    timeoutCallback(node->getID());
                });

            if (!transactionID.empty()) {
                // Add the node to the queried nodes
                lookup.queriedNodes.insert(nodeIDStr);

                // Add the node to the active queries
                lookup.activeQueries.insert(nodeIDStr);

                // Send the query
                m_messageSender->sendQuery(query, node->getEndpoint());

                queriesSent++;

                // Stop if we've sent enough queries
                if (queriesSent >= m_config.getAlpha()) {
                    break;
                }
            }
        }
    }

    return queriesSent > 0;
}

bool NodeLookupQueryManager::isLookupComplete(const NodeLookupState& lookup, size_t kBucketSize) const {
    // If there are active queries, the lookup is not complete
    if (!lookup.activeQueries.empty()) {
        return false;
    }

    // If we haven't queried any nodes, the lookup is not complete
    if (lookup.queriedNodes.empty()) {
        return false;
    }

    // If we've queried all nodes, the lookup is complete
    if (lookup.queriedNodes.size() >= lookup.nodes.size()) {
        return true;
    }

    // Sort the nodes by distance to the target
    std::vector<std::shared_ptr<Node>> sortedNodes = 
        node_lookup_utils::sortNodesByDistance(lookup.nodes, lookup.targetID);

    // Check if we've queried the k closest nodes
    size_t k = std::min(kBucketSize, sortedNodes.size());
    for (size_t i = 0; i < k; ++i) {
        std::string nodeIDStr = nodeIDToString(sortedNodes[i]->getID());
        if (lookup.queriedNodes.find(nodeIDStr) == lookup.queriedNodes.end()) {
            return false;
        }
    }

    return true;
}

} // namespace dht_hunter::dht
