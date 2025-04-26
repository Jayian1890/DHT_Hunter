#include "dht_hunter/network/connection/connection_pool.hpp"
#include "dht_hunter/network/socket/socket.hpp"
#include "dht_hunter/network/socket/socket_type.hpp"
#include <algorithm>

namespace dht_hunter::network {

ConnectionPool::ConnectionPool(size_t maxConnectionsPerEndpoint, std::chrono::seconds connectionTimeout, std::chrono::seconds idleTimeout)
    : m_maxConnectionsPerEndpoint(maxConnectionsPerEndpoint), m_connectionTimeout(connectionTimeout), m_idleTimeout(idleTimeout) {
}

ConnectionPool::~ConnectionPool() {
    clear();
}

void ConnectionPool::getConnection(const EndPoint& endpoint, ConnectionCallback callback) {
    if (!endpoint.isValid() || !callback) {
        if (callback) {
            callback(false, nullptr, endpoint);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if we have an idle connection
    auto& connections = m_connections[endpoint];
    for (auto& connection : connections) {
        if (!connection.inUse) {
            // Found an idle connection
            connection.inUse = true;
            connection.lastUsedTime = std::chrono::steady_clock::now();
            callback(true, connection.socket, endpoint);
            return;
        }
    }

    // Check if we've reached the maximum number of connections for this endpoint
    if (connections.size() >= m_maxConnectionsPerEndpoint) {
        // Add the callback to the pending list
        m_pendingCallbacks[endpoint].push_back(callback);
        return;
    }

    // Create a new connection
    createConnection(endpoint, callback);
}

void ConnectionPool::releaseConnection(std::shared_ptr<Socket> socket, const EndPoint& endpoint) {
    if (!socket || !endpoint.isValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the connection
    auto& connections = m_connections[endpoint];
    for (auto& connection : connections) {
        if (connection.socket == socket) {
            // Mark the connection as idle
            connection.inUse = false;
            connection.lastUsedTime = std::chrono::steady_clock::now();

            // Check if there are pending callbacks
            auto it = m_pendingCallbacks.find(endpoint);
            if (it != m_pendingCallbacks.end() && !it->second.empty()) {
                // Get the next pending callback
                auto callback = it->second.front();
                it->second.erase(it->second.begin());

                // Mark the connection as in use
                connection.inUse = true;
                connection.lastUsedTime = std::chrono::steady_clock::now();

                // Call the callback
                callback(true, connection.socket, endpoint);

                // Remove the pending callbacks entry if it's empty
                if (it->second.empty()) {
                    m_pendingCallbacks.erase(it);
                }
            }

            return;
        }
    }
}

void ConnectionPool::closeConnection(std::shared_ptr<Socket> socket, const EndPoint& endpoint) {
    if (!socket || !endpoint.isValid()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the connection
    auto& connections = m_connections[endpoint];
    for (auto it = connections.begin(); it != connections.end(); ++it) {
        if (it->socket == socket) {
            // Close the socket
            it->socket->close();

            // Remove the connection
            connections.erase(it);

            // Remove the connections entry if it's empty
            if (connections.empty()) {
                m_connections.erase(endpoint);
            }

            return;
        }
    }
}

void ConnectionPool::update(std::chrono::steady_clock::time_point currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove expired connections
    removeExpiredConnections(currentTime);
}

size_t ConnectionPool::getMaxConnectionsPerEndpoint() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxConnectionsPerEndpoint;
}

void ConnectionPool::setMaxConnectionsPerEndpoint(size_t maxConnectionsPerEndpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxConnectionsPerEndpoint = maxConnectionsPerEndpoint;
}

std::chrono::seconds ConnectionPool::getConnectionTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connectionTimeout;
}

void ConnectionPool::setConnectionTimeout(std::chrono::seconds connectionTimeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connectionTimeout = connectionTimeout;
}

std::chrono::seconds ConnectionPool::getIdleTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_idleTimeout;
}

void ConnectionPool::setIdleTimeout(std::chrono::seconds idleTimeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_idleTimeout = idleTimeout;
}

size_t ConnectionPool::getActiveConnections(const EndPoint& endpoint) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_connections.find(endpoint);
    if (it != m_connections.end()) {
        return it->second.size();
    }

    return 0;
}

size_t ConnectionPool::getTotalActiveConnections() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t total = 0;
    for (const auto& pair : m_connections) {
        total += pair.second.size();
    }

    return total;
}

void ConnectionPool::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Close all connections
    for (auto& pair : m_connections) {
        for (auto& connection : pair.second) {
            connection.socket->close();
        }
    }

    m_connections.clear();

    // Call all pending callbacks with failure
    for (auto& pair : m_pendingCallbacks) {
        for (auto& callback : pair.second) {
            callback(false, nullptr, pair.first);
        }
    }

    m_pendingCallbacks.clear();
}

void ConnectionPool::createConnection(const EndPoint& endpoint, ConnectionCallback callback) {
    // Create a socket
    auto socket = Socket::create(SocketType::TCP, endpoint.getAddress().getFamily());
    if (!socket) {
        callback(false, nullptr, endpoint);
        return;
    }

    // Set the socket to non-blocking mode
    socket->setNonBlocking(true);

    // Store the connection
    Connection connection;
    connection.socket = socket;
    connection.lastUsedTime = std::chrono::steady_clock::now();
    connection.inUse = true;
    m_connections[endpoint].push_back(connection);

    // Call the callback
    callback(true, socket, endpoint);
}

void ConnectionPool::removeExpiredConnections(std::chrono::steady_clock::time_point currentTime) {
    for (auto it = m_connections.begin(); it != m_connections.end();) {
        auto& connections = it->second;

        // Remove expired connections
        for (auto connIt = connections.begin(); connIt != connections.end();) {
            if (!connIt->inUse && currentTime - connIt->lastUsedTime > m_idleTimeout) {
                // Close the socket
                connIt->socket->close();

                // Remove the connection
                connIt = connections.erase(connIt);
            } else {
                ++connIt;
            }
        }

        // Remove empty endpoints
        if (connections.empty()) {
            it = m_connections.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dht_hunter::network
