#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/core/routing_table.hpp"
#include "dht_hunter/event/logger.hpp"
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace dht_hunter::dht {

/**
 * @brief Manages the routing table
 */
class RoutingManager {
public:
    /**
     * @brief Constructs a routing manager
     * @param config The DHT configuration
     * @param nodeID The node ID
     */
    RoutingManager(const DHTConfig& config, const NodeID& nodeID);

    /**
     * @brief Destructor
     */
    ~RoutingManager();

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
     * @brief Saves the routing table to a file
     * @param filePath The path to the file
     * @return True if the routing table was saved successfully, false otherwise
     */
    bool saveRoutingTable(const std::string& filePath) const;

    /**
     * @brief Loads the routing table from a file
     * @param filePath The path to the file
     * @return True if the routing table was loaded successfully, false otherwise
     */
    bool loadRoutingTable(const std::string& filePath);

private:
    /**
     * @brief Saves the routing table periodically
     */
    void saveRoutingTablePeriodically();

    DHTConfig m_config;
    NodeID m_nodeID;
    std::shared_ptr<RoutingTable> m_routingTable;
    std::atomic<bool> m_running;
    std::thread m_saveThread;
    mutable std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
