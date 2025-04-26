#include "dht_hunter/network/connection/connection_pool_manager.hpp"

namespace dht_hunter::network {

ConnectionPoolManager::ConnectionPoolManager() {
}

ConnectionPoolManager::~ConnectionPoolManager() {
    clear();
}

std::shared_ptr<ConnectionPool> ConnectionPoolManager::getConnectionPool(SocketType type, AddressFamily family) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create a key for the connection pool
    int key = static_cast<int>(type) | (static_cast<int>(family) << 8);

    // Check if the connection pool exists
    auto it = m_connectionPools.find(key);
    if (it != m_connectionPools.end()) {
        return it->second;
    }

    // Create a new connection pool
    auto connectionPool = std::make_shared<ConnectionPool>();
    m_connectionPools[key] = connectionPool;

    return connectionPool;
}

void ConnectionPoolManager::update(std::chrono::steady_clock::time_point currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Update all connection pools
    for (auto& pair : m_connectionPools) {
        pair.second->update(currentTime);
    }
}

size_t ConnectionPoolManager::getTotalActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t total = 0;
    for (const auto& pair : m_connectionPools) {
        total += pair.second->getTotalActiveConnections();
    }

    return total;
}

void ConnectionPoolManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear all connection pools
    for (auto& pair : m_connectionPools) {
        pair.second->clear();
    }

    m_connectionPools.clear();
}

ConnectionPoolManager& ConnectionPoolManager::getInstance() {
    static ConnectionPoolManager instance;
    return instance;
}

} // namespace dht_hunter::network
