#include "dht_hunter/dht/node_lookup/node_lookup.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_query_manager.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_response_handler.hpp"
#include "dht_hunter/dht/node_lookup/node_lookup_utils.hpp"
#include "dht_hunter/dht/transactions/transaction_manager.hpp"
#include "dht_hunter/dht/network/message_sender.hpp"
#include "dht_hunter/utility/thread/thread_utils.hpp"
#include <algorithm>

// Constants
constexpr int LOOKUP_MAX_ITERATIONS = 10;

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

    unified_event::logTrace("DHT.NodeLookup", "TRACE: getInstance() entry");
    try {
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Acquiring instance mutex");
        return utility::thread::withLock(s_instanceMutex, [&]() {
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Instance mutex acquired");
            if (!s_instance) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Creating new NodeLookup instance");
                s_instance = std::shared_ptr<NodeLookup>(new NodeLookup(
                    config, nodeID, routingTable, transactionManager, messageSender));
                unified_event::logTrace("DHT.NodeLookup", "TRACE: NodeLookup instance created");
            }
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Returning existing NodeLookup instance");
            return s_instance;
        }, "NodeLookup::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Lock timeout exception: " + std::string(e.what()));
        return nullptr;
    }
    unified_event::logTrace("DHT.NodeLookup", "TRACE: getInstance() exit");
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
    unified_event::logTrace("DHT.NodeLookup", "TRACE: Constructor entry");
    unified_event::logTrace("DHT.NodeLookup", "TRACE: Constructor exit");
}

NodeLookup::~NodeLookup() {
    unified_event::logTrace("DHT.NodeLookup", "TRACE: Destructor entry");
    // Clear the singleton instance
    try {
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Acquiring instance mutex");
        utility::thread::withLock(s_instanceMutex, [this]() {
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Instance mutex acquired");
            if (s_instance.get() == this) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Resetting instance pointer");
                s_instance.reset();
            }
        }, "NodeLookup::s_instanceMutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Lock timeout exception: " + std::string(e.what()));
    }
    unified_event::logTrace("DHT.NodeLookup", "TRACE: Destructor exit");
}

void NodeLookup::lookup(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback) {
    unified_event::logTrace("DHT.NodeLookup", "TRACE: lookup() entry for target ID: " + nodeIDToString(targetID));
    try {
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Acquiring mutex");
        utility::thread::withLock(m_mutex, [this, &targetID, &callback]() {
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Mutex acquired");
            // Generate a lookup ID
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Generating lookup ID");
            std::string lookupID = node_lookup_utils::generateLookupID(targetID);
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Generated lookup ID: " + lookupID);

            // Check if a lookup with this ID already exists
            if (m_lookups.find(lookupID) != m_lookups.end()) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Lookup with ID " + lookupID + " already exists, returning");
                return;
            }

            // Create a new lookup
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Creating new lookup state");
            NodeLookupState lookup(targetID, callback);

            // Get the closest nodes from the routing table
            if (m_routingTable) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Getting closest nodes from routing table");
                // Use a default value of 20 for max results
                lookup.nodes = m_routingTable->getClosestNodes(targetID, 20);
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Got " + std::to_string(lookup.nodes.size()) + " closest nodes from routing table");
            } else {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Routing table is null");
            }

            // Add the lookup
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Adding lookup to m_lookups");
            m_lookups.emplace(lookupID, lookup);

            // Send queries
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Sending queries for lookup ID: " + lookupID);
            sendQueries(lookupID);
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Queries sent for lookup ID: " + lookupID);
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Lock timeout exception: " + std::string(e.what()));
    }
    unified_event::logTrace("DHT.NodeLookup", "TRACE: lookup() exit");
}

void NodeLookup::sendQueries(const std::string& lookupID) {
    unified_event::logTrace("DHT.NodeLookup", "TRACE: sendQueries() entry for lookup ID: " + lookupID);
    try {
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Acquiring mutex");
        utility::thread::withLock(m_mutex, [this, &lookupID]() {
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Mutex acquired");
            // Find the lookup
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Finding lookup with ID: " + lookupID);
            auto it = m_lookups.find(lookupID);
            if (it == m_lookups.end()) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Lookup with ID " + lookupID + " not found, returning");
                return;
            }
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Found lookup with ID: " + lookupID);

            NodeLookupState& lookup = it->second;
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Current iteration: " + std::to_string(lookup.iteration));

            // Create query manager and response handler
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Creating query manager and response handler");
            NodeLookupQueryManager queryManager(m_config, m_nodeID, m_routingTable, m_transactionManager, m_messageSender);
            NodeLookupResponseHandler responseHandler(m_config, m_nodeID, m_routingTable);
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Created query manager and response handler");

            // Check if we've reached the maximum number of iterations
            if (lookup.iteration >= LOOKUP_MAX_ITERATIONS) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Reached maximum iterations (" + std::to_string(LOOKUP_MAX_ITERATIONS) + "), completing lookup");
                completeLookup(lookupID);
                return;
            }

            // Check if the lookup is complete
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Checking if lookup is complete");
            if (queryManager.isLookupComplete(lookup, m_config.getKBucketSize())) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: Lookup is complete, completing lookup");
                completeLookup(lookupID);
                return;
            }
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Lookup is not complete, continuing");

            // Send queries using the query manager
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Sending queries using query manager");
            bool queriesSent = queryManager.sendQueries(
                lookupID,
                lookup,
                [this, lookupID](std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender) {
                    unified_event::logTrace("DHT.NodeLookup", "TRACE: Response callback called for lookup ID: " + lookupID);
                    auto findNodeResponse = std::dynamic_pointer_cast<FindNodeResponse>(response);
                    if (findNodeResponse) {
                        unified_event::logTrace("DHT.NodeLookup", "TRACE: Response is a FindNodeResponse, handling response");
                        handleResponse(lookupID, findNodeResponse, sender);
                    } else {
                        unified_event::logTrace("DHT.NodeLookup", "TRACE: Response is not a FindNodeResponse, ignoring");
                    }
                },
                [this, lookupID](std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender) {
                    unified_event::logTrace("DHT.NodeLookup", "TRACE: Error callback called for lookup ID: " + lookupID);
                    handleError(lookupID, error, sender);
                },
                [this, lookupID](const NodeID& nodeID) {
                    unified_event::logTrace("DHT.NodeLookup", "TRACE: Timeout callback called for lookup ID: " + lookupID + ", node ID: " + nodeIDToString(nodeID));
                    handleTimeout(lookupID, nodeID);
                });
            unified_event::logTrace("DHT.NodeLookup", "TRACE: Queries sent: " + std::string(queriesSent ? "true" : "false"));

            // If no queries were sent, check if the lookup is complete
            if (!queriesSent) {
                unified_event::logTrace("DHT.NodeLookup", "TRACE: No queries were sent, checking if lookup is complete");
                if (queryManager.isLookupComplete(lookup, m_config.getKBucketSize())) {
                    unified_event::logTrace("DHT.NodeLookup", "TRACE: Lookup is complete, completing lookup");
                    completeLookup(lookupID);
                } else {
                    unified_event::logTrace("DHT.NodeLookup", "TRACE: Lookup is not complete, but no queries were sent");
                }
            }
            unified_event::logTrace("DHT.NodeLookup", "TRACE: sendQueries() mutex section completed");
        }, "NodeLookup::m_mutex");
    } catch (const utility::thread::LockTimeoutException& e) {
        unified_event::logError("DHT.NodeLookup", e.what());
        unified_event::logTrace("DHT.NodeLookup", "TRACE: Lock timeout exception: " + std::string(e.what()));
    }
    unified_event::logTrace("DHT.NodeLookup", "TRACE: sendQueries() exit");
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
