#include "dht_hunter/dht/node_lookup/components/node_lookup_manager.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

// Constants
constexpr int DEFAULT_MAX_RESULTS = 20;
constexpr int DEFAULT_ALPHA = 3;
constexpr int DEFAULT_MAX_ITERATIONS = 10;
constexpr int DEFAULT_MAX_QUERIES = 100;
constexpr int DEFAULT_K_BUCKET_SIZE = 8;

namespace dht_hunter::dht::node_lookup {

NodeLookupManager::NodeLookupManager(const DHTConfig& config,
                                   const NodeID& nodeID,
                                   std::shared_ptr<RoutingTable> routingTable,
                                   std::shared_ptr<TransactionManager> transactionManager,
                                   std::shared_ptr<MessageSender> messageSender)
    : BaseNodeLookupComponent("NodeLookupManager", config, nodeID, routingTable),
      m_transactionManager(transactionManager),
      m_messageSender(messageSender) {
}

bool NodeLookupManager::onInitialize() {
    if (!m_transactionManager) {
        unified_event::logError("DHT.NodeLookup." + m_name, "Transaction manager is null");
        return false;
    }

    if (!m_messageSender) {
        unified_event::logError("DHT.NodeLookup." + m_name, "Message sender is null");
        return false;
    }

    return true;
}

bool NodeLookupManager::onStart() {
    return true;
}

void NodeLookupManager::onStop() {
    // Clear all lookups
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lookups.clear();
}

void NodeLookupManager::lookup(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {
    try {
        utility::thread::withLock(m_mutex, [this, &targetID, &callback]() {
            // Generate a lookup ID
            std::string lookupID = generateLookupID(targetID);

            // Check if a lookup with this ID already exists
            if (m_lookups.find(lookupID) != m_lookups.end()) {
                return;
            }

            // Create a new lookup
            NodeLookupState lookup(targetID, callback);

            // Get the closest nodes from the routing table
            if (m_routingTable) {
                lookup.setNodes(m_routingTable->getClosestNodes(targetID, DEFAULT_MAX_RESULTS));
            }

            // Add the lookup
            m_lookups.emplace(lookupID, lookup);

            // Send queries
            sendQueries(lookupID);
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
    }
}

void NodeLookupManager::handleResponse(const std::string& lookupID, std::shared_ptr<FindNodeResponse> response, const EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &response, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Get the node ID from the response
            if (!response->getNodeID()) {
                return;
            }

            std::string nodeIDStr = nodeIDToString(response->getNodeID().value());

            // Mark the node as responded
            lookup.addRespondedNode(nodeIDStr);

            // Remove the node from active queries
            lookup.removeActiveQuery(nodeIDStr);

            // Add the node to the routing table
            if (m_routingTable) {
                auto node = std::make_shared<Node>(response->getNodeID().value(), sender);
                m_routingTable->addNode(node);
            }

            // Get the nodes from the response
            const auto& nodes = response->getNodes();

            // Add the nodes to the lookup
            for (const auto& node : nodes) {
                // Add the node to the lookup
                lookup.addNode(node);
            }

            // Check if the lookup is complete
            if (isLookupComplete(lookupID)) {
                completeLookup(lookupID);
            } else {
                // Send more queries
                sendQueries(lookupID);
            }
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
    }
}

void NodeLookupManager::handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &error, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Get the node ID from the error
            if (!error->getNodeID()) {
                return;
            }

            std::string nodeIDStr = nodeIDToString(error->getNodeID().value());

            // Remove the node from active queries
            lookup.removeActiveQuery(nodeIDStr);

            // Check if the lookup is complete
            if (isLookupComplete(lookupID)) {
                completeLookup(lookupID);
            } else {
                // Send more queries
                sendQueries(lookupID);
            }
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
    }
}

void NodeLookupManager::handleTimeout(const std::string& lookupID, const NodeID& nodeID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &nodeID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Get the node ID string
            std::string nodeIDStr = nodeIDToString(nodeID);

            // Remove the node from active queries
            lookup.removeActiveQuery(nodeIDStr);

            // Check if the lookup is complete
            if (isLookupComplete(lookupID)) {
                completeLookup(lookupID);
            } else {
                // Send more queries
                sendQueries(lookupID);
            }
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
    }
}

void NodeLookupManager::sendQueries(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Increment the iteration
            lookup.incrementIteration();

            // Sort the nodes by distance to the target
            std::vector<std::shared_ptr<Node>> nodes = lookup.getNodes();
            std::sort(nodes.begin(), nodes.end(), [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
                return calculateDistance(a->getID(), lookup.getTargetID()) < calculateDistance(b->getID(), lookup.getTargetID());
            });

            // Send queries to the closest nodes that haven't been queried yet
            size_t queriesSent = 0;
            for (const auto& node : nodes) {
                // Skip nodes that have already been queried
                std::string nodeIDStr = nodeIDToString(node->getID());
                if (lookup.hasBeenQueried(nodeIDStr)) {
                    continue;
                }

                // Create a find_node query
                auto query = std::make_shared<FindNodeQuery>("", m_nodeID, lookup.getTargetID());

                // Create callbacks
                auto responseCallback = [this, lookupID](std::shared_ptr<ResponseMessage> response, const EndPoint& sender) {
                    auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                    if (findNodeResponse) {
                        handleResponse(lookupID, findNodeResponse, sender);
                    }
                };

                auto errorCallback = [this, lookupID](std::shared_ptr<ErrorMessage> error, const EndPoint& sender) {
                    handleError(lookupID, error, sender);
                };

                auto timeoutCallback = [this, lookupID, node]() {
                    handleTimeout(lookupID, node->getID());
                };

                // Send the query
                if (m_transactionManager && m_messageSender) {
                    std::string transactionID = m_transactionManager->createTransaction(
                        query,
                        node->getEndpoint(),
                        responseCallback,
                        errorCallback,
                        timeoutCallback);

                    if (!transactionID.empty()) {
                        // Add the node to the queried nodes
                        lookup.addQueriedNode(nodeIDStr);

                        // Add the node to the active queries
                        lookup.addActiveQuery(nodeIDStr);

                        // Send the query
                        m_messageSender->sendQuery(query, node->getEndpoint());

                        queriesSent++;

                        // Stop if we've sent enough queries
                        if (queriesSent >= DEFAULT_ALPHA) {
                            break;
                        }
                    }
                }
            }
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
    }
}

bool NodeLookupManager::isLookupComplete(const std::string& lookupID) {
    try {
        return utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return true;
            }

            const NodeLookupState& lookup = it->second;

            // If there are no active queries, the lookup is complete
            if (lookup.getActiveQueries().empty()) {
                return true;
            }

            // If we've reached the maximum number of iterations, the lookup is complete
            if (lookup.getIteration() >= DEFAULT_MAX_ITERATIONS) {
                return true;
            }

            // If we've queried enough nodes, the lookup is complete
            if (lookup.getQueriedNodes().size() >= DEFAULT_MAX_QUERIES) {
                return true;
            }

            return false;
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
        return true; // Assume complete on error
    }
}

void NodeLookupManager::completeLookup(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Sort the nodes by distance to the target
            std::vector<std::shared_ptr<Node>> nodes = lookup.getNodes();
            std::sort(nodes.begin(), nodes.end(), [&lookup](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
                return calculateDistance(a->getID(), lookup.getTargetID()) < calculateDistance(b->getID(), lookup.getTargetID());
            });

            // Limit the number of nodes
            if (nodes.size() > DEFAULT_K_BUCKET_SIZE) {
                nodes.resize(DEFAULT_K_BUCKET_SIZE);
            }

            // Call the callback
            if (lookup.getCallback()) {
                lookup.getCallback()(nodes);
            }

            // Remove the lookup
            m_lookups.erase(lookupID);
        }, "NodeLookupManager::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup." + m_name, e.what());
    }
}

std::string NodeLookupManager::generateLookupID(const NodeID& targetID) const {
    // Create a string stream
    std::ostringstream ss;

    // Add the target ID
    ss << "nl_" << std::hex << std::setfill('0');
    for (const auto& byte : targetID) {
        ss << std::setw(2) << static_cast<int>(byte);
    }

    // Return the lookup ID
    return ss.str();
}

} // namespace dht_hunter::dht::node_lookup
