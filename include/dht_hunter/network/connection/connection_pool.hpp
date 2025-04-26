#pragma once

#include "dht_hunter/network/address/endpoint.hpp"
#include "dht_hunter/network/socket/socket.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace dht_hunter::network {

/**
 * @class ConnectionPool
 * @brief Manages a pool of connections.
 *
 * This class provides a pool of connections to remote endpoints,
 * with support for connection reuse, timeouts, and limits.
 */
class ConnectionPool {
public:
    /**
     * @brief Callback for connection establishment.
     * @param success True if the connection was established, false otherwise.
     * @param socket The socket for the connection.
     * @param endpoint The endpoint of the connection.
     */
    using ConnectionCallback = std::function<void(bool success, std::shared_ptr<Socket> socket, const EndPoint& endpoint)>;

    /**
     * @brief Constructs a connection pool.
     * @param maxConnectionsPerEndpoint The maximum number of connections allowed per endpoint.
     * @param connectionTimeout The timeout for connections.
     * @param idleTimeout The timeout for idle connections.
     */
    ConnectionPool(size_t maxConnectionsPerEndpoint = 10,
                   std::chrono::seconds connectionTimeout = std::chrono::seconds(30),
                   std::chrono::seconds idleTimeout = std::chrono::seconds(300));

    /**
     * @brief Destructor.
     */
    ~ConnectionPool();

    /**
     * @brief Gets a connection to an endpoint.
     * @param endpoint The endpoint to connect to.
     * @param callback The connection callback.
     */
    void getConnection(const EndPoint& endpoint, ConnectionCallback callback);

    /**
     * @brief Releases a connection back to the pool.
     * @param socket The socket to release.
     * @param endpoint The endpoint of the connection.
     */
    void releaseConnection(std::shared_ptr<Socket> socket, const EndPoint& endpoint);

    /**
     * @brief Closes a connection.
     * @param socket The socket to close.
     * @param endpoint The endpoint of the connection.
     */
    void closeConnection(std::shared_ptr<Socket> socket, const EndPoint& endpoint);

    /**
     * @brief Updates the connection pool.
     * @param currentTime The current time.
     */
    void update(std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now());

    /**
     * @brief Gets the maximum number of connections allowed per endpoint.
     * @return The maximum number of connections allowed per endpoint.
     */
    size_t getMaxConnectionsPerEndpoint() const;

    /**
     * @brief Sets the maximum number of connections allowed per endpoint.
     * @param maxConnectionsPerEndpoint The maximum number of connections allowed per endpoint.
     */
    void setMaxConnectionsPerEndpoint(size_t maxConnectionsPerEndpoint);

    /**
     * @brief Gets the connection timeout.
     * @return The connection timeout.
     */
    std::chrono::seconds getConnectionTimeout() const;

    /**
     * @brief Sets the connection timeout.
     * @param connectionTimeout The connection timeout.
     */
    void setConnectionTimeout(std::chrono::seconds connectionTimeout);

    /**
     * @brief Gets the idle timeout.
     * @return The idle timeout.
     */
    std::chrono::seconds getIdleTimeout() const;

    /**
     * @brief Sets the idle timeout.
     * @param idleTimeout The idle timeout.
     */
    void setIdleTimeout(std::chrono::seconds idleTimeout);

    /**
     * @brief Gets the number of active connections to an endpoint.
     * @param endpoint The endpoint to check.
     * @return The number of active connections.
     */
    size_t getActiveConnections(const EndPoint& endpoint) const;

    /**
     * @brief Gets the total number of active connections.
     * @return The total number of active connections.
     */
    size_t getTotalActiveConnections() const;

    /**
     * @brief Clears all connections.
     */
    void clear();

private:
    /**
     * @brief Creates a new connection.
     * @param endpoint The endpoint to connect to.
     * @param callback The connection callback.
     */
    void createConnection(const EndPoint& endpoint, ConnectionCallback callback);

    /**
     * @brief Removes expired connections.
     * @param currentTime The current time.
     */
    void removeExpiredConnections(std::chrono::steady_clock::time_point currentTime);

    struct Connection {
        std::shared_ptr<Socket> socket;
        std::chrono::steady_clock::time_point lastUsedTime;
        bool inUse;
    };

    size_t m_maxConnectionsPerEndpoint;
    std::chrono::seconds m_connectionTimeout;
    std::chrono::seconds m_idleTimeout;
    std::unordered_map<EndPoint, std::vector<Connection>> m_connections;
    std::unordered_map<EndPoint, std::vector<ConnectionCallback>> m_pendingCallbacks;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
