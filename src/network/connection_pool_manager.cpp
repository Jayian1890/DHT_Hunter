#include "dht_hunter/network/connection_pool_manager.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.ConnectionPoolManager");
}

ConnectionPoolManager::ConnectionPoolManager()
    : m_running(false),
      m_maintenanceInterval(60) {
    
    // Create the default connection pool
    m_pools["default"] = std::make_shared<ConnectionPool>();
    
    logger->debug("Created ConnectionPoolManager");
}

ConnectionPoolManager::~ConnectionPoolManager() {
    stopMaintenance();
    
    // Close all connection pools
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& [name, pool] : m_pools) {
        pool->closeAll();
    }
    
    m_pools.clear();
    
    logger->debug("Destroyed ConnectionPoolManager");
}

ConnectionPoolManager& ConnectionPoolManager::getInstance() {
    static ConnectionPoolManager instance;
    return instance;
}

std::shared_ptr<ConnectionPool> ConnectionPoolManager::getDefaultPool() {
    return getPool("default");
}

std::shared_ptr<ConnectionPool> ConnectionPoolManager::getPool(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_pools.find(name);
    if (it != m_pools.end()) {
        return it->second;
    }
    
    // If the pool doesn't exist, create it with default configuration
    auto pool = std::make_shared<ConnectionPool>();
    m_pools[name] = pool;
    
    logger->debug("Created new connection pool: {}", name);
    return pool;
}

std::shared_ptr<ConnectionPool> ConnectionPoolManager::createPool(const std::string& name, const ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // If the pool already exists, update its configuration
    auto it = m_pools.find(name);
    if (it != m_pools.end()) {
        it->second->setConfig(config);
        logger->debug("Updated configuration for existing connection pool: {}", name);
        return it->second;
    }
    
    // Create a new pool with the specified configuration
    auto pool = std::make_shared<ConnectionPool>(config);
    m_pools[name] = pool;
    
    logger->debug("Created new connection pool: {} with custom configuration", name);
    return pool;
}

void ConnectionPoolManager::removePool(const std::string& name) {
    // Don't allow removing the default pool
    if (name == "default") {
        logger->warning("Cannot remove the default connection pool");
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_pools.find(name);
    if (it != m_pools.end()) {
        // Close all connections in the pool
        it->second->closeAll();
        
        // Remove the pool
        m_pools.erase(it);
        
        logger->debug("Removed connection pool: {}", name);
    }
}

void ConnectionPoolManager::startMaintenance(std::chrono::seconds interval) {
    if (m_running) {
        logger->warning("Maintenance thread already running");
        return;
    }
    
    m_maintenanceInterval = interval;
    m_running = true;
    
    // Start the maintenance thread
    m_maintenanceThread = std::thread(&ConnectionPoolManager::maintenanceThread, this);
    
    logger->debug("Started maintenance thread with interval: {} seconds", interval.count());
}

void ConnectionPoolManager::stopMaintenance() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Wait for the maintenance thread to finish
    if (m_maintenanceThread.joinable()) {
        m_maintenanceThread.join();
    }
    
    logger->debug("Stopped maintenance thread");
}

void ConnectionPoolManager::maintenance() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& [name, pool] : m_pools) {
        pool->maintenance();
    }
    
    logger->debug("Performed maintenance on all connection pools");
}

void ConnectionPoolManager::maintenanceThread() {
    logger->debug("Maintenance thread started");
    
    while (m_running) {
        // Sleep for the maintenance interval
        std::this_thread::sleep_for(m_maintenanceInterval);
        
        // Perform maintenance
        maintenance();
    }
    
    logger->debug("Maintenance thread stopped");
}

} // namespace dht_hunter::network
