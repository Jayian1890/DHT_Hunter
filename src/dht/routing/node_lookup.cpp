#include "dht_hunter/dht/routing/node_lookup.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include <algorithm>
#include <random>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<NodeLookup> NodeLookup::s_instance = nullptr;
std::mutex NodeLookup::s_instanceMutex;

std::shared_ptr<NodeLookup> NodeLookup::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable,
    std::shared_ptr<TransactionManager> transactionManager,
    std::shared_ptr<MessageSender> messageSender) {

    std::lock_guard<std::mutex> lock(s_instanceMutex);

    if (!s_instance) {
        s_instance = std::shared_ptr<NodeLookup>(new NodeLookup(
            config, nodeID, routingTable, transactionManager, messageSender));
    }

    return s_instance;
}

NodeLookup::NodeLookup(const DHTConfig& config,
                     const NodeID& nodeID,
                     std::shared_ptr<RoutingTable> routingTable,
                     std::shared_ptr<TransactionManager> transactionManager,
                     std::shared_ptr<MessageSender> messageSender)
    : m_config(config),
      m_nodeID(nodeID),
      m_routingTable(routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender),
      m_logger(event::Logger::forComponent("DHT.NodeLookup")) {
}

NodeLookup::~NodeLookup() {
    // Clear the singleton instance
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (s_instance.get() == this) {
        s_instance.reset();
    }
}

void NodeLookup::lookup(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate a lookup ID
    std::string lookupID = nodeIDToString(targetID);

    // Check if a lookup with this ID already exists
    if (m_lookups.find(lookupID) != m_lookups.end()) {
        m_logger.warning("Lookup for {} already in progress", lookupID);
        return;
    }

    // Create a new lookup
    Lookup lookup(targetID, callback);

    // Get the closest nodes from the routing table
    if (m_routingTable) {
        lookup.nodes = m_routingTable->getClosestNodes(targetID, m_config.getMaxResults());
    }

    // Add the lookup
    m_lookups.emplace(lookupID, lookup);

    m_logger.debug("Starting lookup for {}", lookupID);

    // Send queries
    sendQueries(lookupID);
}

void NodeLookup::sendQueries(const std::string& lookupID) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        m_logger.error("Lookup {} not found", lookupID);
        return;
    }

    Lookup& lookup = it->second;

    // Check if we've reached the maximum number of iterations
    if (lookup.iteration >= LOOKUP_MAX_ITERATIONS) {
        m_logger.debug("Lookup {} reached maximum iterations", lookupID);
        completeLookup(lookupID);
        return;
    }

    // Increment the iteration counter
    lookup.iteration++;

    // Sort the nodes by distance to the target
    std::sort(lookup.nodes.begin(), lookup.nodes.end(),
        [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            NodeID distanceA = a->getID().distanceTo(lookup.targetID);
            NodeID distanceB = b->getID().distanceTo(lookup.targetID);
            return std::lexicographical_compare(
                distanceA.begin(), distanceA.end(),
                distanceB.begin(), distanceB.end());
        });

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
        if (isLookupComplete(lookupID)) {
            m_logger.debug("Lookup {} complete", lookupID);
            completeLookup(lookupID);
        }
        return;
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
                [this, lookupID, nodeIDStr](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                    auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                    if (findNodeResponse) {
                        handleResponse(lookupID, findNodeResponse, sender);
                    }
                },
                [this, lookupID, nodeIDStr](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                    handleError(lookupID, error, sender);
                },
                [this, lookupID, node]() {
                    handleTimeout(lookupID, node->getID());
                });

            if (!transactionID.empty()) {
                // Add the node to the queried nodes
                lookup.queriedNodes.insert(nodeIDStr);

                // Add the node to the active queries
                lookup.activeQueries.insert(nodeIDStr);

                // Send the query
                m_messageSender->sendQuery(query, node->getEndpoint());

                m_logger.debug("Sent find_node query to {} for lookup {}", node->getEndpoint().toString(), lookupID);

                queriesSent++;

                // Stop if we've sent enough queries
                if (queriesSent >= m_config.getAlpha()) {
                    break;
                }
            }
        }
    }
}

void NodeLookup::handleResponse(const std::string& lookupID, std::shared_ptr<FindNodeResponse> response, const network::EndPoint& /*sender*/) {
    // Variables to store data we'll need after releasing the lock
    std::vector<std::shared_ptr<Node>> nodesToAdd;
    bool shouldSendQueries = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Find the lookup
        auto it = m_lookups.find(lookupID);
        if (it == m_lookups.end()) {
            m_logger.error("Lookup {} not found", lookupID);
            return;
        }

        Lookup& lookup = it->second;

        // Get the node ID from the response
        if (!response->getNodeID()) {
            m_logger.error("Response has no node ID");
            return;
        }

        std::string nodeIDStr = nodeIDToString(response->getNodeID().value());

        // Remove the node from the active queries
        lookup.activeQueries.erase(nodeIDStr);

        // Add the node to the responded nodes
        lookup.respondedNodes.insert(nodeIDStr);

        // Process the nodes from the response
        for (const auto& node : response->getNodes()) {
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

                // Store the node for adding to the routing table after releasing the lock
                nodesToAdd.push_back(node);
            }
        }

        // Set flag to send more queries after releasing the lock
        shouldSendQueries = true;
    } // Release the lock

    // Add nodes to the routing table outside the lock
    if (m_routingTable) {
        for (const auto& node : nodesToAdd) {
            m_routingTable->addNode(node);
        }
    }

    // Send more queries if needed
    if (shouldSendQueries) {
        sendQueries(lookupID);
    }
}

void NodeLookup::handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> /*error*/, const network::EndPoint& sender) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        m_logger.error("Lookup {} not found", lookupID);
        return;
    }

    Lookup& lookup = it->second;

    // Find the node
    auto nodeIt = std::find_if(lookup.nodes.begin(), lookup.nodes.end(),
        [&sender](const std::shared_ptr<Node>& node) {
            return node->getEndpoint() == sender;
        });

    if (nodeIt == lookup.nodes.end()) {
        m_logger.error("Node not found for error");
        return;
    }

    std::string nodeIDStr = nodeIDToString((*nodeIt)->getID());

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    sendQueries(lookupID);
}

void NodeLookup::handleTimeout(const std::string& lookupID, const NodeID& nodeID) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        m_logger.error("Lookup {} not found", lookupID);
        return;
    }

    Lookup& lookup = it->second;

    std::string nodeIDStr = nodeIDToString(nodeID);

    // Remove the node from the active queries
    lookup.activeQueries.erase(nodeIDStr);

    // Send more queries
    sendQueries(lookupID);
}

bool NodeLookup::isLookupComplete(const std::string& lookupID) {
    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        m_logger.error("Lookup {} not found", lookupID);
        return true;
    }

    const Lookup& lookup = it->second;

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
    std::vector<std::shared_ptr<Node>> sortedNodes = lookup.nodes;
    std::sort(sortedNodes.begin(), sortedNodes.end(),
        [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            NodeID distanceA = a->getID().distanceTo(lookup.targetID);
            NodeID distanceB = b->getID().distanceTo(lookup.targetID);
            return std::lexicographical_compare(
                distanceA.begin(), distanceA.end(),
                distanceB.begin(), distanceB.end());
        });

    // Check if we've queried the k closest nodes
    size_t k = std::min(m_config.getKBucketSize(), sortedNodes.size());
    for (size_t i = 0; i < k; ++i) {
        std::string nodeIDStr = nodeIDToString(sortedNodes[i]->getID());
        if (lookup.queriedNodes.find(nodeIDStr) == lookup.queriedNodes.end()) {
            return false;
        }
    }

    return true;
}

void NodeLookup::completeLookup(const std::string& lookupID) {
    // Find the lookup
    auto it = m_lookups.find(lookupID);
    if (it == m_lookups.end()) {
        m_logger.error("Lookup {} not found", lookupID);
        return;
    }

    Lookup& lookup = it->second;

    // Sort the nodes by distance to the target
    std::sort(lookup.nodes.begin(), lookup.nodes.end(),
        [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
            NodeID distanceA = a->getID().distanceTo(lookup.targetID);
            NodeID distanceB = b->getID().distanceTo(lookup.targetID);
            return std::lexicographical_compare(
                distanceA.begin(), distanceA.end(),
                distanceB.begin(), distanceB.end());
        });

    // Limit the number of nodes
    if (lookup.nodes.size() > m_config.getMaxResults()) {
        lookup.nodes.resize(m_config.getMaxResults());
    }

    // Call the callback
    if (lookup.callback) {
        lookup.callback(lookup.nodes);
    }

    // Remove the lookup
    m_lookups.erase(it);

    m_logger.debug("Completed lookup for {}", lookupID);
}

} // namespace dht_hunter::dht
