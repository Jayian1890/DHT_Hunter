#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
#include <functional>

namespace dht_hunter::dht {

// Forward declarations
class DHTRoutingManager;
class DHTPeerStorage;
class DHTTransactionManager;

/**
 * @brief Manages persistence operations for DHT components
 */
class DHTPersistenceManager {
public:
    /**
     * @brief Constructs a persistence manager
     * @param config The DHT node configuration
     * @param routingManager The routing manager to use for saving/loading the routing table
     * @param peerStorage The peer storage to use for saving/loading the peer cache
     * @param transactionManager The transaction manager to use for saving/loading transactions
     */
    DHTPersistenceManager(const DHTNodeConfig& config,
                         std::shared_ptr<DHTRoutingManager> routingManager,
                         std::shared_ptr<DHTPeerStorage> peerStorage,
                         std::shared_ptr<DHTTransactionManager> transactionManager);

    /**
     * @brief Destructor
     */
    ~DHTPersistenceManager();

    /**
     * @brief Starts the persistence manager
     * @return True if the persistence manager was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the persistence manager
     */
    void stop();

    /**
     * @brief Checks if the persistence manager is running
     * @return True if the persistence manager is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Saves all data
     * @return True if all data was saved successfully, false otherwise
     */
    bool saveAll();

    /**
     * @brief Loads all data
     * @return True if all data was loaded successfully, false otherwise
     */
    bool loadAll();

    /**
     * @brief Saves the routing table
     * @param filePath The path to the file
     * @return True if the routing table was saved successfully, false otherwise
     */
    bool saveRoutingTable(const std::string& filePath);

    /**
     * @brief Loads the routing table
     * @param filePath The path to the file
     * @return True if the routing table was loaded successfully, false otherwise
     */
    bool loadRoutingTable(const std::string& filePath);

    /**
     * @brief Saves the peer cache
     * @param filePath The path to the file
     * @return True if the peer cache was saved successfully, false otherwise
     */
    bool savePeerCache(const std::string& filePath);

    /**
     * @brief Loads the peer cache
     * @param filePath The path to the file
     * @return True if the peer cache was loaded successfully, false otherwise
     */
    bool loadPeerCache(const std::string& filePath);

    /**
     * @brief Saves the transactions
     * @param filePath The path to the file
     * @return True if the transactions were saved successfully, false otherwise
     */
    bool saveTransactions(const std::string& filePath);

    /**
     * @brief Loads the transactions
     * @param filePath The path to the file
     * @return True if the transactions were loaded successfully, false otherwise
     */
    bool loadTransactions(const std::string& filePath);

    /**
     * @brief Saves the node ID
     * @param filePath The path to the file
     * @param nodeID The node ID to save
     * @return True if the node ID was saved successfully, false otherwise
     */
    bool saveNodeID(const std::string& filePath, const std::array<uint8_t, 20>& nodeID);

    /**
     * @brief Loads the node ID
     * @param filePath The path to the file
     * @param nodeID The node ID to load into
     * @return True if the node ID was loaded successfully, false otherwise
     */
    bool loadNodeID(const std::string& filePath, std::array<uint8_t, 20>& nodeID);

private:
    /**
     * @brief Saves data periodically
     */
    void savePeriodically();

    DHTNodeConfig m_config;
    std::shared_ptr<DHTRoutingManager> m_routingManager;
    std::shared_ptr<DHTPeerStorage> m_peerStorage;
    std::shared_ptr<DHTTransactionManager> m_transactionManager;
    std::atomic<bool> m_running{false};
    std::thread m_saveThread;
    util::CheckedMutex m_saveMutex;
};

} // namespace dht_hunter::dht
