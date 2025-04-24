#pragma once

#include "dht_hunter/network/connection_pool.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <atomic>

namespace dht_hunter::network {

/**
 * @brief A global connection pool manager
 * 
 * This class provides access to global connection pools for different purposes.
 * It manages the lifecycle of connection pools and provides a maintenance thread
 * to clean up idle and expired connections.
 */
class ConnectionPoolManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static ConnectionPoolManager& getInstance();
    
    /**
     * @brief Gets the default connection pool
     * @return A shared pointer to the default connection pool
     */
    std::shared_ptr<ConnectionPool> getDefaultPool();
    
    /**
     * @brief Gets a named connection pool
     * @param name The name of the pool
     * @return A shared pointer to the named connection pool
     */
    std::shared_ptr<ConnectionPool> getPool(const std::string& name);
    
    /**
     * @brief Creates a named connection pool with the specified configuration
     * @param name The name of the pool
     * @param config The configuration
     * @return A shared pointer to the created connection pool
     */
    std::shared_ptr<ConnectionPool> createPool(const std::string& name, const ConnectionPoolConfig& config);
    
    /**
     * @brief Removes a named connection pool
     * @param name The name of the pool
     */
    void removePool(const std::string& name);
    
    /**
     * @brief Starts the maintenance thread
     * @param interval The maintenance interval
     */
    void startMaintenance(std::chrono::seconds interval = std::chrono::seconds(60));
    
    /**
     * @brief Stops the maintenance thread
     */
    void stopMaintenance();
    
    /**
     * @brief Performs maintenance on all connection pools
     */
    void maintenance();
    
    /**
     * @brief Destructor
     */
    ~ConnectionPoolManager();
    
private:
    /**
     * @brief Constructs a connection pool manager
     */
    ConnectionPoolManager();
    
    /**
     * @brief Copy constructor (deleted)
     */
    ConnectionPoolManager(const ConnectionPoolManager&) = delete;
    
    /**
     * @brief Assignment operator (deleted)
     */
    ConnectionPoolManager& operator=(const ConnectionPoolManager&) = delete;
    
    /**
     * @brief Maintenance thread function
     */
    void maintenanceThread();
    
    std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> m_pools;  ///< Connection pools by name
    mutable std::mutex m_mutex;                                               ///< Mutex for thread safety
    
    std::thread m_maintenanceThread;                                          ///< Maintenance thread
    std::atomic<bool> m_running;                                              ///< Whether the maintenance thread is running
    std::chrono::seconds m_maintenanceInterval;                               ///< Maintenance interval
};

} // namespace dht_hunter::network
