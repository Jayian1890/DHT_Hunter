#pragma once

#include "dht_hunter/network/address/endpoint.hpp"
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dht_hunter::network {

/**
 * @class ConnectionThrottler
 * @brief Throttles connections to prevent flooding.
 *
 * This class provides a mechanism for throttling connections to specific endpoints,
 * to prevent flooding or denial-of-service attacks.
 */
class ConnectionThrottler {
public:
    /**
     * @brief Constructs a connection throttler.
     * @param maxConnectionsPerEndpoint The maximum number of connections allowed per endpoint.
     * @param connectionTimeout The timeout for connections.
     */
    ConnectionThrottler(size_t maxConnectionsPerEndpoint = 10, std::chrono::seconds connectionTimeout = std::chrono::seconds(60));

    /**
     * @brief Destructor.
     */
    ~ConnectionThrottler();

    /**
     * @brief Tries to acquire permission for a connection to an endpoint.
     * @param endpoint The endpoint to connect to.
     * @return True if permission was granted, false otherwise.
     */
    bool tryAcquire(const EndPoint& endpoint);

    /**
     * @brief Releases a connection to an endpoint.
     * @param endpoint The endpoint to release.
     */
    void release(const EndPoint& endpoint);

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
     * @brief Removes expired connections.
     */
    void removeExpiredConnections();

    struct Connection {
        std::chrono::steady_clock::time_point timestamp;
    };

    size_t m_maxConnectionsPerEndpoint;
    std::chrono::seconds m_connectionTimeout;
    std::unordered_map<EndPoint, std::vector<Connection>> m_connections;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
