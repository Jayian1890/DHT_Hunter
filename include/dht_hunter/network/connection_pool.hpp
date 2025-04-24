#pragma once

#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/network/rate_limiter.hpp"

#include <unordered_map>
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>
#include <atomic>

namespace dht_hunter::network {

/**
 * @brief Configuration for the connection pool
 */
struct ConnectionPoolConfig {
    size_t maxConnections = 100;                                ///< Maximum number of connections in the pool
    size_t maxConnectionsPerHost = 8;                           ///< Maximum number of connections per host
    std::chrono::seconds connectionTimeout{30};                 ///< Connection timeout
    std::chrono::seconds connectionIdleTimeout{60};             ///< Idle connection timeout
    std::chrono::seconds connectionMaxLifetime{300};            ///< Maximum lifetime of a connection
    bool reuseConnections = true;                               ///< Whether to reuse connections
    bool validateConnectionsBeforeReuse = true;                 ///< Whether to validate connections before reuse
    std::shared_ptr<ConnectionThrottler> connectionThrottler;   ///< Connection throttler
};

/**
 * @brief Represents a pooled connection
 */
class PooledConnection {
public:
    /**
     * @brief Constructs a pooled connection
     * @param socket The socket
     * @param endpoint The endpoint
     */
    PooledConnection(std::shared_ptr<Socket> socket, const EndPoint& endpoint);
    
    /**
     * @brief Destructor
     */
    ~PooledConnection();
    
    /**
     * @brief Gets the socket
     * @return The socket
     */
    std::shared_ptr<Socket> getSocket() const;
    
    /**
     * @brief Gets the endpoint
     * @return The endpoint
     */
    EndPoint getEndpoint() const;
    
    /**
     * @brief Gets the creation time
     * @return The creation time
     */
    std::chrono::steady_clock::time_point getCreationTime() const;
    
    /**
     * @brief Gets the last used time
     * @return The last used time
     */
    std::chrono::steady_clock::time_point getLastUsedTime() const;
    
    /**
     * @brief Updates the last used time
     */
    void updateLastUsedTime();
    
    /**
     * @brief Checks if the connection is valid
     * @return True if the connection is valid, false otherwise
     */
    bool isValid() const;
    
    /**
     * @brief Validates the connection
     * @return True if the connection is valid, false otherwise
     */
    bool validate();
    
    /**
     * @brief Closes the connection
     */
    void close();
    
private:
    std::shared_ptr<Socket> m_socket;                           ///< The socket
    EndPoint m_endpoint;                                        ///< The endpoint
    std::chrono::steady_clock::time_point m_creationTime;       ///< The creation time
    std::chrono::steady_clock::time_point m_lastUsedTime;       ///< The last used time
    std::atomic<bool> m_isValid;                                ///< Whether the connection is valid
};

/**
 * @brief Manages a pool of connections for reuse
 */
class ConnectionPool {
public:
    /**
     * @brief Constructs a connection pool
     * @param config The configuration
     */
    explicit ConnectionPool(const ConnectionPoolConfig& config = ConnectionPoolConfig());
    
    /**
     * @brief Destructor
     */
    ~ConnectionPool();
    
    /**
     * @brief Gets a connection from the pool
     * @param endpoint The endpoint to connect to
     * @param timeout The connection timeout
     * @return A shared pointer to a pooled connection, or nullptr if no connection could be established
     */
    std::shared_ptr<PooledConnection> getConnection(const EndPoint& endpoint, 
                                                   std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    
    /**
     * @brief Returns a connection to the pool
     * @param connection The connection to return
     */
    void returnConnection(std::shared_ptr<PooledConnection> connection);
    
    /**
     * @brief Closes all connections
     */
    void closeAll();
    
    /**
     * @brief Gets the number of active connections
     * @return The number of active connections
     */
    size_t getActiveConnectionCount() const;
    
    /**
     * @brief Gets the number of idle connections
     * @return The number of idle connections
     */
    size_t getIdleConnectionCount() const;
    
    /**
     * @brief Gets the number of connections for a specific host
     * @param host The host
     * @return The number of connections
     */
    size_t getConnectionCountForHost(const NetworkAddress& host) const;
    
    /**
     * @brief Sets the configuration
     * @param config The configuration
     */
    void setConfig(const ConnectionPoolConfig& config);
    
    /**
     * @brief Gets the configuration
     * @return The configuration
     */
    ConnectionPoolConfig getConfig() const;
    
    /**
     * @brief Performs maintenance on the connection pool
     * 
     * This method should be called periodically to clean up idle and expired connections.
     */
    void maintenance();
    
private:
    /**
     * @brief Creates a new connection
     * @param endpoint The endpoint to connect to
     * @param timeout The connection timeout
     * @return A shared pointer to a pooled connection, or nullptr if no connection could be established
     */
    std::shared_ptr<PooledConnection> createConnection(const EndPoint& endpoint, std::chrono::milliseconds timeout);
    
    /**
     * @brief Gets an idle connection for the specified endpoint
     * @param endpoint The endpoint
     * @return A shared pointer to a pooled connection, or nullptr if no idle connection is available
     */
    std::shared_ptr<PooledConnection> getIdleConnection(const EndPoint& endpoint);
    
    /**
     * @brief Removes expired connections
     */
    void removeExpiredConnections();
    
    ConnectionPoolConfig m_config;                                                  ///< The configuration
    
    std::unordered_map<NetworkAddress, std::vector<std::shared_ptr<PooledConnection>>> m_activeConnections;  ///< Active connections by host
    std::unordered_map<NetworkAddress, std::queue<std::shared_ptr<PooledConnection>>> m_idleConnections;     ///< Idle connections by host
    
    mutable std::mutex m_mutex;                                                     ///< Mutex for thread safety
    std::atomic<size_t> m_totalConnections;                                         ///< Total number of connections
};

} // namespace dht_hunter::network
