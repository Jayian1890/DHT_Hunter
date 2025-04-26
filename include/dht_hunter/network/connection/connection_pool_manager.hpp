#pragma once

#include "dht_hunter/network/connection/connection_pool.hpp"
#include "dht_hunter/network/socket/socket_type.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>

namespace dht_hunter::network {

/**
 * @class ConnectionPoolManager
 * @brief Manages multiple connection pools.
 *
 * This class provides a manager for multiple connection pools,
 * with support for different socket types and address families.
 */
class ConnectionPoolManager {
public:
    /**
     * @brief Constructs a connection pool manager.
     */
    ConnectionPoolManager();

    /**
     * @brief Destructor.
     */
    ~ConnectionPoolManager();

    /**
     * @brief Gets a connection pool for a socket type and address family.
     * @param type The socket type.
     * @param family The address family.
     * @return The connection pool.
     */
    std::shared_ptr<ConnectionPool> getConnectionPool(SocketType type, AddressFamily family = AddressFamily::IPv4);

    /**
     * @brief Updates all connection pools.
     * @param currentTime The current time.
     */
    void update(std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now());

    /**
     * @brief Gets the total number of active connections.
     * @return The total number of active connections.
     */
    size_t getTotalActiveConnections() const;

    /**
     * @brief Clears all connection pools.
     */
    void clear();

    /**
     * @brief Gets the singleton instance of the connection pool manager.
     * @return The singleton instance.
     */
    static ConnectionPoolManager& getInstance();

private:
    std::unordered_map<int, std::shared_ptr<ConnectionPool>> m_connectionPools;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
