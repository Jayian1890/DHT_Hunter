#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/types.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/dht/routing/components/node_verifier_component.hpp"
#include "dht_hunter/dht/routing/components/bucket_refresh_component.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace dht_hunter::dht {

/**
 * @brief Manages the routing table (Singleton)
 */
class RoutingManager {
public:
    /**
     * @brief Gets the singleton instance of the routing manager
     * @param config The DHT configuration (only used if instance doesn't exist yet)
     * @param nodeID The node ID (only used if instance doesn't exist yet)
     * @param routingTable The routing table (only used if instance doesn't exist yet)
     * @param transactionManager The transaction manager (only used if instance doesn't exist yet)
     * @param messageSender The message sender (only used if instance doesn't exist yet)
     * @return The singleton instance
     */
    static std::shared_ptr<RoutingManager> getInstance(
        const DHTConfig& config,
        const NodeID& nodeID,
        std::shared_ptr<RoutingTable> routingTable,
        std::shared_ptr<TransactionManager> transactionManager,
        std::shared_ptr<MessageSender> messageSender);

    /**
     * @brief Destructor
     */
    ~RoutingManager();

    /**
     * @brief Delete copy constructor and assignment operator
     */
    RoutingManager(const RoutingManager&) = delete;
    RoutingManager& operator=(const RoutingManager&) = delete;
    RoutingManager(RoutingManager&&) = delete;
    RoutingManager& operator=(RoutingManager&&) = delete;

    /**
     * @brief Starts the routing manager
     * @return True if the routing manager was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the routing manager
     */
    void stop();

    /**
     * @brief Checks if the routing manager is running
     * @return True if the routing manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Gets the routing table
     * @return The routing table
     */
    std::shared_ptr<RoutingTable> getRoutingTable() const;

    /**
     * @brief Adds a node to the routing table
     * @param node The node to add
     * @return True if the node was added, false otherwise
     */
    bool addNode(std::shared_ptr<Node> node);

    /**
     * @brief Removes a node from the routing table
     * @param nodeID The ID of the node to remove
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const NodeID& nodeID);

    /**
     * @brief Gets a node from the routing table
     * @param nodeID The ID of the node to get
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> getNode(const NodeID& nodeID) const;

    /**
     * @brief Gets the closest nodes to a target ID
     * @param targetID The target ID
     * @param k The maximum number of nodes to return
     * @return The closest nodes
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes(const NodeID& targetID, size_t k) const;

    /**
     * @brief Gets all nodes in the routing table
     * @return All nodes in the routing table
     */
    std::vector<std::shared_ptr<Node>> getAllNodes() const;

    /**
     * @brief Gets the number of nodes in the routing table
     * @return The number of nodes in the routing table
     */
    size_t getNodeCount() const;

    /**
     * @brief Refreshes all buckets in the routing table
     * This is useful after system sleep/wake events to ensure the routing table is up-to-date
     */
    void refreshAllBuckets();

    // Routing table saving and loading methods have been removed
    // These operations are now handled by the PersistenceManager

private:
    // Routing table periodic saving has been removed
    // This operation is now handled by the PersistenceManager

    /**
     * @brief Private constructor for singleton pattern
     */
    RoutingManager(const DHTConfig& config,
                  const NodeID& nodeID,
                  std::shared_ptr<RoutingTable> routingTable,
                  std::shared_ptr<TransactionManager> transactionManager,
                  std::shared_ptr<MessageSender> messageSender);

    // Static instance for singleton pattern
    static std::shared_ptr<RoutingManager> s_instance;
    static std::mutex s_instanceMutex;

    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::shared_ptr<routing::NodeVerifierComponent> m_nodeVerifier;
    std::shared_ptr<routing::BucketRefreshComponent> m_bucketRefresher;
    std::shared_ptr<TransactionManager> m_transactionManager;
    std::shared_ptr<MessageSender> m_messageSender;
    std::atomic<bool> m_running;
    mutable std::mutex m_mutex;

    // Event bus
    std::shared_ptr<unified_event::EventBus> m_eventBus;    // Logger removed
};

} // namespace dht_hunter::dht
