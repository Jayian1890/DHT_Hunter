#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_query_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_response_handler.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_utils.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/dht/core/dht_constants.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include <algorithm>

namespace dht_hunter::dht {

// Initialize static members
std::shared_ptr<NodeLookup> NodeLookup::s_instance = nullptr;
std::recursive_mutex NodeLookup::s_instanceMutex;

std::shared_ptr<NodeLookup> NodeLookup::getInstance(
    const DHTConfig& config,
    const NodeID& nodeID,
    std::shared_ptr<RoutingTable> routingTable,
    std::shared_ptr<TransactionManager> transactionManager,
    std::shared_ptr<MessageSender> messageSender) {

    try {
        return utility::thread::withLock(s_instanceMutex, [&]() {
            if (!s_instance) {
                s_instance = std::shared_ptr<NodeLookup>(new NodeLookup(
                    config, nodeID, routingTable, transactionManager, messageSender));
            }
            return s_instance;
        }, "NodeLookup::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
        return nullptr;
    }
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
      m_messageSender(messageSender) {
}

NodeLookup::~NodeLookup() {
    // Clear the singleton instance
    try {
        utility::thread::withLock(s_instanceMutex, [this]() {
            if (s_instance.get() == this) {
                s_instance.reset();
            }
        }, "NodeLookup::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::lookup(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {
    try {
        utility::thread::withLock(m_mutex, [this, &targetID, &callback]() {
            // Generate a lookup ID
            std::string lookupID = node_lookup_utils::generateLookupID(targetID);

            // Check if a lookup with this ID already exists
            if (m_lookups.find(lookupID) != m_lookups.end()) {
                return;
            }

            // Create a new lookup
            NodeLookupState lookup(targetID, callback);

            // Get the closest nodes from the routing table
            if (m_routingTable) {
                lookup.nodes = m_routingTable->getClosestNodes(targetID, m_config.getMaxResults());
            }

            // Add the lookup
            m_lookups.emplace(lookupID, lookup);

            // Send queries
            sendQueries(lookupID);
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::sendQueries(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Create query manager and response handler
            NodeLookupQueryManager queryManager(m_config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);
            NodeLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable);

            // Check if we've reached the maximum number of iterations
            if (lookup.iteration >= LOOKUP_MAX_ITERATIONS) {
                completeLookup(lookupID);
                return;
            }

            // Check if the lookup is complete
            if (queryManager.isLookupComplete(lookup, m_config.getKBucketSize())) {
                completeLookup(lookupID);
                return;
            }

            // Send queries using the query manager
            bool queriesSent = queryManager.sendQueries(
                lookupID,
                lookup,
                [this, lookupID](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                    auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                    if (findNodeResponse) {
                        handleResponse(lookupID, findNodeResponse, sender);
                    }
                },
                [this, lookupID](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                    handleError(lookupID, error, sender);
                },
                [this, lookupID](const NodeID& nodeID) {
                    handleTimeout(lookupID, nodeID);
                });

            // If no queries were sent, check if the lookup is complete
            if (!queriesSent) {
                if (queryManager.isLookupComplete(lookup, m_config.getKBucketSize())) {
                    completeLookup(lookupID);
                }
            }
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::handleResponse(const std::string& lookupID, std::shared_ptr<FindNodeResponse> response, const network::EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &response, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Create response handler
            NodeLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable);

            // Use the response handler to process the response
            bool shouldSendQueries = responseHandler.handleResponse(lookupID, lookup, response, sender);

            // Send more queries if needed
            if (shouldSendQueries) {
                sendQueries(lookupID);
            }
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &error, &sender]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Create response handler
            NodeLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable);

            // Use the response handler to process the error
            bool shouldSendQueries = responseHandler.handleError(lookup, error, sender);

            // Send more queries if needed
            if (shouldSendQueries) {
                sendQueries(lookupID);
            }
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

void NodeLookup::handleTimeout(const std::string& lookupID, const NodeID& nodeID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID, &nodeID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Create response handler
            NodeLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable);

            // Use the response handler to process the timeout
            bool shouldSendQueries = responseHandler.handleTimeout(lookup, nodeID);

            // Send more queries if needed
            if (shouldSendQueries) {
                sendQueries(lookupID);
            }
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

bool NodeLookup::isLookupComplete(const std::string& lookupID) {
    try {
        return utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return true;
            }

            const NodeLookupState& lookup = it->second;

            // Create query manager
            NodeLookupQueryManager queryManager(m_config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);

            // Use the query manager to check if the lookup is complete
            return queryManager.isLookupComplete(lookup, m_config.getKBucketSize());
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
        return true; // Assume complete on error
    }
}

void NodeLookup::completeLookup(const std::string& lookupID) {
    try {
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            // Find the lookup
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                return;
            }

            NodeLookupState& lookup = it->second;

            // Create response handler
            NodeLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable);

            // Use the response handler to complete the lookup
            responseHandler.completeLookup(lookup);

            // Remove the lookup
            m_lookups.erase(it);
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
    }
}

} // namespace dht_hunter::dht
