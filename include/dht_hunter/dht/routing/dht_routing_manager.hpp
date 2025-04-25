#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/dht/core/dht_routing_table.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>

namespace dht_hunter::dht {

/**
 * @brief Manages the DHT routing table
 */
class DHTRoutingManager {
public:
    /**
     * @brief Constructs a routing manager
     * @param config The DHT node configuration
     * @param nodeID The node ID
     */
    DHTRoutingManager(const DHTNodeConfig& config, const NodeID& nodeID);

    /**
     * @brief Destructor
     */
    ~DHTRoutingManager();

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
     * @brief Adds a node to the routing table
     * @param nodeID The node ID
     * @param endpoint The node's endpoint
     * @return True if the node was added, false otherwise
     */
    bool addNode(const NodeID& nodeID, const network::EndPoint& endpoint);

    /**
     * @brief Removes a node from the routing table
     * @param nodeID The node ID
     * @return True if the node was removed, false otherwise
     */
    bool removeNode(const NodeID& nodeID);

    /**
     * @brief Finds a node in the routing table
     * @param nodeID The node ID
     * @return The node, or nullptr if not found
     */
    std::shared_ptr<Node> findNode(const NodeID& nodeID) const;

    /**
     * @brief Gets the closest nodes to a target ID
     * @param targetID The target ID
     * @param count The maximum number of nodes to return
     * @return The closest nodes
     */
    std::vector<std::shared_ptr<Node>> getClosestNodes(const NodeID& targetID, size_t count) const;

    /**
     * @brief Gets the number of nodes in the routing table
     * @return The number of nodes
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

    DHTNodeConfig m_config;
    NodeID m_nodeID;
    RoutingTable m_routingTable;
    std::string m_routingTablePath;
    std::atomic<bool> m_running{false};
    std::thread m_saveThread;
    mutable util::CheckedMutex m_routingTableMutex;
};

} // namespace dht_hunter::dht
