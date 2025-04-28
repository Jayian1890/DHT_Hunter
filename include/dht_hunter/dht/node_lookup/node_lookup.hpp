#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/network/message.hpp"
#include "dht_hunter/dht/network/response_message.hpp"
#include "dht_hunter/dht/network/error_message.hpp"
#include "dht_hunter/unified_event/adapters/logger_adapter.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <mutex>

namespace dht_hunter::dht {

// Forward declarations
class RoutingTable;
class TransactionManager;
class MessageSender;

/**
 * @brief A lookup state for node lookups
 */
struct NodeLookupState {
    NodeID targetID;
    std::vector<std::shared_ptr<Node>> nodes;
    std::unordered_set<std::string> queriedNodes;
    std::unordered_set<std::string> respondedNodes;
    std::unordered_set<std::string> activeQueries;
    size_t iteration;
    std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback;

    NodeLookupState(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback)
        : targetID(targetID), iteration(0), callback(callback) {}
};

/**
 * @brief Performs a node lookup (Singleton)
 */
class NodeLookup {
public:
    /**
     * @brief Gets the singleton instance of the node lookup
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param routingTable The routing table (only used if instance doesn't exist yet)
     * @param transactionManager The transaction manager (only used if instance doesn't exist yet)
     * @param messageSender The message sender (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<NodeLookup> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingTable> routingTable,
        std::shared_ptr<TransactionManager> transactionManager,
        std::shared_ptr<MessageSender> messageSender);

    /**
     * @brief Destructor
     */
    ~NodeLookup();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    NodeLookup(const NodeLookup&) = delete;
    NodeLookup& operator=(const NodeLookup&) = delete;
    NodeLookup(NodeLookup&&) = delete;
    NodeLookup& operator=(NodeLookup&&) = delete;

    /**
     * @brief Performs a node lookup
     * @param targetID The target ID
     * @param callback The callback to call with the result
     */
    void lookup(const NodeID& targetID, std::function<void(const std::vector<std::shared_ptr<Node>>&)> callback);

private:
    /**
     * @brief Sends find_node queries to the closest nodes
     * @param lookup The lookup state
     */
    void sendQueries(const std::string& lookupID);

    /**
     * @brief Handles a find_node response
     * @param lookupID The lookup ID
     * @param response The response
     * @param sender The sender
     */
    void handleResponse(const std::string& lookupID, std::shared_ptr<FindNodeResponse> response, const network::EndPoint& sender);

    /**
     * @brief Handles an error
     * @param lookupID The lookup ID
     * @param error The error
     * @param sender The sender
     */
    void handleError(const std::string& lookupID, std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles a timeout
     * @param lookupID The lookup ID
     * @param nodeID The node ID
     */
    void handleTimeout(const std::string& lookupID, const NodeID& nodeID);

    /**
     * @brief Checks if a lookup is complete
     * @param lookupID The lookup ID
     * @return True if the lookup is complete, false otherwise
     */
    bool isLookupComplete(const std::string& lookupID);

    /**
     * @brief Completes a lookup
     * @param lookupID The lookup ID
     */
    void completeLookup(const std::string& lookupID);

    /**
     * @brief Private constructor for singleton pattern
     */
    NodeLookup(const DHTConfig& config,
              const NodeID& nodeID,
              std::shared_ptr<RoutingTable> routingTable,
              std::shared_ptr<TransactionManager> transactionManager,
              std::shared_ptr<MessageSender> messageSender);

    // Static instance for singleton pattern
    static std::shared_ptr<NodeLookup> s_instance;
    static std::recursive_mutex s_instanceMutex;

    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::unordered_map<std::string, NodeLookupState> m_lookups;
    std::recursive_mutex m_mutex;
};

} // namespace dht_hunter::dht
