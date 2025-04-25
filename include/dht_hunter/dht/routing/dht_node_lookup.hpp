#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/network/dht_message.hpp"
#include "dht_hunter/dht/network/dht_response_message.hpp"
#include "dht_hunter/dht/network/dht_error_message.hpp"
#include "dht_hunter/dht/transactions/dht_transaction_types.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <atomic>

namespace dht_hunter::dht {

// Forward declarations
class DHTRoutingManager;
class DHTTransactionManager;

/**
 * @brief Callback for node lookup completion
 * @param nodes The closest nodes found
 */
using NodeLookupCallback = std::function<void(const std::vector<std::shared_ptr<Node>>&)>;

/**
 * @brief Represents a node lookup operation
 */
class NodeLookup {
public:
    /**
     * @brief Constructs a node lookup
     * @param id The lookup ID
     * @param targetID The target ID to look up
     * @param callback The callback to call when the lookup is complete
     */
    NodeLookup(const std::string& id, const NodeID& targetID, NodeLookupCallback callback);

    /**
     * @brief Gets the lookup ID
     * @return The lookup ID
     */
    const std::string& getID() const;

    /**
     * @brief Gets the target ID
     * @return The target ID
     */
    const NodeID& getTargetID() const;

    /**
     * @brief Gets the closest nodes found so far
     * @return The closest nodes
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes() const;

    /**
     * @brief Adds a node to the lookup
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

    /**
     * @brief Marks a node as queried
     * @param nodeID The node ID
     */
    void markNodeAsQueried(const NodeID& nodeID);

    /**
     * @brief Marks a node as responded
     * @param nodeID The node ID
     */
    void markNodeAsResponded(const NodeID& nodeID);

    /**
     * @brief Marks a node as failed
     * @param nodeID The node ID
     */
    void markNodeAsFailed(const NodeID& nodeID);

    /**
     * @brief Gets the next nodes to query
     * @param count The maximum number of nodes to return
     * @return The next nodes to query
     */
    std::vector<std::shared_ptr<Node>> getNextNodesToQuery(size_t count);

    /**
     * @brief Checks if the lookup is complete
     * @return True if the lookup is complete, false otherwise
     */
    bool isComplete() const;

    /**
     * @brief Completes the lookup
     */
    void complete();

private:
    std::string m_id;
    NodeID m_targetID;
    NodeLookupCallback m_callback;
    std::vector<std::shared_ptr<Node>> m_closestNodes;
    std::unordered_set<NodeID> m_queriedNodes;
    std::unordered_set<NodeID> m_respondedNodes;
    std::unordered_set<NodeID> m_failedNodes;
    mutable util::CheckedMutex m_mutex;
    std::atomic<bool> m_complete{false};
};

/**
 * @brief Manages node lookups
 */
class DHTNodeLookup {
public:
    /**
     * @brief Constructs a node lookup manager
     * @param config The DHT node configuration
     * @param nodeID The node ID
     * @param routingManager The routing manager to use for finding nodes
     * @param transactionManager The transaction manager to use for sending queries
     */
    DHTNodeLookup(const DHTNodeConfig& config,
                 const NodeID& nodeID,
                 std::shared_ptr<DHTRoutingManager> routingManager,
                 std::shared_ptr<DHTTransactionManager> transactionManager);

    /**
     * @brief Destructor
     */
    ~DHTNodeLookup() = default;

    /**
     * @brief Starts a node lookup
     * @param targetID The target ID to look up
     * @param callback The callback to call when the lookup is complete
     * @return The lookup ID, or an empty string if the lookup could not be started
     */
    std::string startLookup(const NodeID& targetID, NodeLookupCallback callback);

    /**
     * @brief Handles a find_node response
     * @param response The response message
     * @param sender The sender's endpoint
     * @return True if the response was handled, false otherwise
     */
    bool handleFindNodeResponse(std::shared_ptr<ResponseMessage> response, const network::EndPoint& sender);

    /**
     * @brief Handles a find_node error
     * @param error The error message
     * @param sender The sender's endpoint
     * @return True if the error was handled, false otherwise
     */
    bool handleFindNodeError(std::shared_ptr<ErrorMessage> error, const network::EndPoint& sender);

    /**
     * @brief Handles a find_node timeout
     * @param transactionID The transaction ID
     * @return True if the timeout was handled, false otherwise
     */
    bool handleFindNodeTimeout(const std::string& transactionID);

private:
    /**
     * @brief Continues a node lookup
     * @param lookup The lookup to continue
     */
    void continueLookup(std::shared_ptr<NodeLookup> lookup);

    /**
     * @brief Generates a random lookup ID
     * @return The lookup ID
     */
    std::string generateLookupID() const;

    /**
     * @brief Finds a lookup by transaction ID
     * @param transactionID The transaction ID
     * @return The lookup, or nullptr if not found
     */
    std::shared_ptr<NodeLookup> findLookupByTransactionID(const std::string& transactionID);

    DHTNodeConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<DHTRoutingManager> m_routingManager;
    std::shared_ptr<DHTTransactionManager> m_transactionManager;
    std::unordered_map<std::string, std::shared_ptr<NodeLookup>> m_lookups;
    std::unordered_map<std::string, std::string> m_transactionToLookup;
    mutable util::CheckedMutex m_lookupsMutex;
};

} // namespace dht_hunter::dht
