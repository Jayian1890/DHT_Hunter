#include "dht_hunter/bittorrent/metadata/connection_pool.hpp"
#include <algorithm>

namespace dht_hunter::bittorrent::metadata {

// Initialize static members
std::shared_ptr<ConnectionPool> ConnectionPool::s_instance = nullptr;
std::mutex ConnectionPool::s_instanceMutex;

std::shared_ptr<ConnectionPool> ConnectionPool::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<ConnectionPool>(new ConnectionPool());
    }
    return s_instance;
}

ConnectionPool::ConnectionPool()
    : m_running(false) {
}

ConnectionPool::~ConnectionPool() {
    stop();
}

void ConnectionPool::start() {
    if (m_running) {
        return;
    }

    m_running = true;

    // Start the cleanup thread
    m_cleanupThread = std::thread(&ConnectionPool::cleanupIdleConnectionsPeriodically, this);

    unified_event::logInfo("BitTorrent.ConnectionPool", "Started connection pool");
}

void ConnectionPool::stop() {
    if (!m_running) {
        return;
    }

    m_running = false;

    // Notify the cleanup thread to exit
    {
        std::lock_guard<std::mutex> lock(m_cleanupMutex);
        m_cleanupCondition.notify_all();
    }

    // Wait for the cleanup thread to exit
    if (m_cleanupThread.joinable()) {
        m_cleanupThread.join();
    }

    // Close all connections
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        for (auto& pair : m_connections) {
            for (auto& connection : pair.second) {
                if (connection->client) {
                    connection->client->disconnect();
                }
            }
        }
        m_connections.clear();
    }

    unified_event::logInfo("BitTorrent.ConnectionPool", "Stopped connection pool");
}

std::shared_ptr<network::TCPClient> ConnectionPool::getConnection(
    const network::EndPoint& endpoint,
    std::function<void(const uint8_t*, size_t)> dataCallback,
    std::function<void(const std::string&)> errorCallback,
    std::function<void()> closedCallback) {

    std::string endpointStr = endpoint.toString();

    // Try to get a connection from the pool
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);

        auto it = m_connections.find(endpointStr);
        if (it != m_connections.end()) {
            // Find an idle connection and validate it
            auto connIt = it->second.begin();
            while (connIt != it->second.end()) {
                auto& connection = *connIt;

                // Check if the connection is valid
                if (!connection->client || !connection->client->isConnected()) {
                    // Remove invalid connection
                    unified_event::logDebug("BitTorrent.ConnectionPool", "Removing invalid connection to " + endpointStr);

                    // Close the connection if it exists
                    if (connection->client) {
                        connection->client->disconnect();
                    }

                    // Remove from the pool
                    connIt = it->second.erase(connIt);
                    continue;
                }

                // Check if the connection is idle
                if (!connection->inUse) {
                    // Perform a health check on the connection
                    if (validateConnection(connection->client)) {
                        // Update the callbacks
                        connection->client->setDataReceivedCallback(dataCallback);
                        connection->client->setErrorCallback([this, connection, errorCallback, endpointStr](const std::string& error) {
                            // Mark the connection as invalid
                            connection->inUse = false;

                            // Call the original error callback
                            if (errorCallback) {
                                errorCallback(error);
                            }

                            unified_event::logWarning("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " reported error: " + error);
                        });
                        connection->client->setConnectionClosedCallback([this, connection, closedCallback, endpointStr]() {
                            // Mark the connection as invalid
                            connection->inUse = false;

                            // Call the original closed callback
                            if (closedCallback) {
                                closedCallback();
                            }

                            unified_event::logDebug("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " closed");
                        });

                        // Mark as in use
                        connection->inUse = true;
                        connection->lastUsed = std::chrono::steady_clock::now();

                        unified_event::logDebug("BitTorrent.ConnectionPool", "Reusing connection to " + endpointStr);
                        return connection->client;
                    } else {
                        // Connection failed validation, remove it
                        unified_event::logDebug("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " failed validation");

                        // Close the connection
                        connection->client->disconnect();

                        // Remove from the pool
                        connIt = it->second.erase(connIt);
                        continue;
                    }
                }

                // Move to the next connection
                ++connIt;
            }

            // Check if we've reached the maximum number of connections for this endpoint
            if (it->second.size() >= MAX_CONNECTIONS_PER_ENDPOINT) {
                // Remove the oldest idle connection
                auto oldestIt = std::min_element(it->second.begin(), it->second.end(),
                    [](const std::shared_ptr<PooledConnection>& a, const std::shared_ptr<PooledConnection>& b) {
                        return a->lastUsed < b->lastUsed;
                    });

                if (oldestIt != it->second.end() && !(*oldestIt)->inUse) {
                    // Close the connection
                    if ((*oldestIt)->client) {
                        (*oldestIt)->client->disconnect();
                    }

                    // Remove from the pool
                    it->second.erase(oldestIt);
                    unified_event::logDebug("BitTorrent.ConnectionPool", "Removed oldest connection to " + endpointStr + " to make room for new connection");
                }
            }

            // If the endpoint has no more connections, remove it from the map
            if (it->second.empty()) {
                m_connections.erase(it);
            }
        }

        // Check if we've reached the maximum total number of connections
        size_t totalConnections = 0;
        for (const auto& pair : m_connections) {
            totalConnections += pair.second.size();
        }

        if (totalConnections >= MAX_TOTAL_CONNECTIONS) {
            // Find the oldest idle connection across all endpoints
            std::shared_ptr<PooledConnection> oldestConnection;
            std::string oldestEndpoint;

            for (const auto& pair : m_connections) {
                for (const auto& connection : pair.second) {
                    if (!connection->inUse && (!oldestConnection || connection->lastUsed < oldestConnection->lastUsed)) {
                        oldestConnection = connection;
                        oldestEndpoint = pair.first;
                    }
                }
            }

            if (oldestConnection) {
                // Close the connection
                if (oldestConnection->client) {
                    oldestConnection->client->disconnect();
                }

                // Remove from the pool
                auto oldestIt = m_connections.find(oldestEndpoint);
                if (oldestIt != m_connections.end()) {
                    oldestIt->second.erase(std::remove(oldestIt->second.begin(), oldestIt->second.end(), oldestConnection), oldestIt->second.end());
                    if (oldestIt->second.empty()) {
                        m_connections.erase(oldestIt);
                    }
                }

                unified_event::logDebug("BitTorrent.ConnectionPool", "Removed oldest connection to " + oldestEndpoint + " to make room for new connection");
            }
        }
    }

    // Create a new connection
    auto client = std::make_shared<network::TCPClient>();

    // Set up error handling with proper connection lifecycle management
    client->setDataReceivedCallback(dataCallback);
    client->setErrorCallback([this, client, errorCallback, endpointStr](const std::string& error) {
        // Call the original error callback
        if (errorCallback) {
            errorCallback(error);
        }

        unified_event::logWarning("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " reported error: " + error);

        // Ensure the connection is properly removed from the pool
        removeConnectionByClient(endpointStr, client);
    });
    client->setConnectionClosedCallback([this, client, closedCallback, endpointStr]() {
        // Call the original closed callback
        if (closedCallback) {
            closedCallback();
        }

        unified_event::logDebug("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " closed");

        // Ensure the connection is properly removed from the pool
        removeConnectionByClient(endpointStr, client);
    });

    // Connect to the endpoint with timeout handling
    unified_event::logDebug("BitTorrent.ConnectionPool", "Connecting to " + endpointStr);

    // Attempt to connect with timeout
    if (!client->connect(endpoint.getAddress().toString(), endpoint.getPort())) {
        unified_event::logWarning("BitTorrent.ConnectionPool", "Failed to connect to " + endpointStr);
        return nullptr;
    }

    // Verify the connection is actually established
    if (!client->isConnected()) {
        unified_event::logWarning("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " failed verification");
        return nullptr;
    }

    // Add to the pool
    auto connection = std::make_shared<PooledConnection>(client);
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);
        m_connections[endpointStr].push_back(connection);
    }

    unified_event::logDebug("BitTorrent.ConnectionPool", "Created new connection to " + endpointStr);
    return client;
}

void ConnectionPool::returnConnection(
    const network::EndPoint& endpoint,
    std::shared_ptr<network::TCPClient> connection) {

    if (!connection) {
        return;
    }

    std::string endpointStr = endpoint.toString();

    // Validate the connection before returning it to the pool
    if (!connection->isConnected() || !validateConnection(connection)) {
        unified_event::logDebug("BitTorrent.ConnectionPool", "Not returning invalid connection to " + endpointStr + " to the pool");
        connection->disconnect();

        // Remove the connection from the pool
        removeConnectionByClient(endpointStr, connection);
        return;
    }

    // Mark the connection as idle
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);

        auto it = m_connections.find(endpointStr);
        if (it != m_connections.end()) {
            for (auto& pooledConnection : it->second) {
                if (pooledConnection->client.get() == connection.get()) {
                    // Reset the callbacks to prevent memory leaks
                    connection->setDataReceivedCallback(nullptr);
                    connection->setErrorCallback([this, connection, endpointStr](const std::string& error) {
                        unified_event::logWarning("BitTorrent.ConnectionPool", "Idle connection to " + endpointStr + " reported error: " + error);
                        removeConnectionByClient(endpointStr, connection);
                    });
                    connection->setConnectionClosedCallback([this, connection, endpointStr]() {
                        unified_event::logDebug("BitTorrent.ConnectionPool", "Idle connection to " + endpointStr + " closed");
                        removeConnectionByClient(endpointStr, connection);
                    });

                    pooledConnection->inUse = false;
                    pooledConnection->lastUsed = std::chrono::steady_clock::now();
                    unified_event::logDebug("BitTorrent.ConnectionPool", "Returned connection to " + endpointStr + " to the pool");
                    return;
                }
            }
        }
    }

    // If we get here, the connection wasn't found in the pool
    // This can happen if the connection was removed from the pool while it was in use
    // Just close it
    unified_event::logDebug("BitTorrent.ConnectionPool", "Connection to " + endpointStr + " not found in pool, closing it");
    connection->disconnect();
}

void ConnectionPool::cleanupIdleConnections() {
    std::vector<std::string> emptyEndpoints;
    size_t closedCount = 0;
    size_t invalidCount = 0;

    // Find idle connections that have been idle for too long or are invalid
    {
        std::lock_guard<std::mutex> lock(m_connectionsMutex);

        auto now = std::chrono::steady_clock::now();
        for (auto& pair : m_connections) {
            auto& connections = pair.second;
            std::string endpointStr = pair.first;

            // Remove idle connections that have been idle for too long or are invalid
            connections.erase(
                std::remove_if(connections.begin(), connections.end(),
                    [&now, &closedCount, &invalidCount, this](const std::shared_ptr<PooledConnection>& connection) {
                        // Check if the connection is invalid
                        if (!connection->client || !connection->client->isConnected() || !validateConnection(connection->client)) {
                            // Close the connection if it exists
                            if (connection->client) {
                                connection->client->disconnect();
                            }
                            invalidCount++;
                            return true;
                        }

                        // Check if the connection has been idle for too long
                        if (!connection->inUse) {
                            auto idleTime = std::chrono::duration_cast<std::chrono::seconds>(now - connection->lastUsed).count();
                            if (idleTime > MAX_IDLE_TIME_SECONDS) {
                                // Close the connection
                                if (connection->client) {
                                    connection->client->disconnect();
                                }
                                closedCount++;
                                return true;
                            }
                        }
                        return false;
                    }),
                connections.end());

            // Check if this endpoint has no more connections
            if (connections.empty()) {
                emptyEndpoints.push_back(pair.first);
            }
        }

        // Remove empty endpoints
        for (const auto& endpoint : emptyEndpoints) {
            m_connections.erase(endpoint);
        }
    }

    if (closedCount > 0 || invalidCount > 0) {
        unified_event::logInfo("BitTorrent.ConnectionPool",
            "Cleaned up connections: " +
            std::to_string(closedCount) + " idle, " +
            std::to_string(invalidCount) + " invalid");
    }
}

void ConnectionPool::cleanupIdleConnectionsPeriodically() {
    while (m_running) {
        // Wait for the cleanup interval or until stop() is called
        {
            std::unique_lock<std::mutex> lock(m_cleanupMutex);
            m_cleanupCondition.wait_for(lock, std::chrono::seconds(CLEANUP_INTERVAL_SECONDS), [this]() { return !m_running; });
        }

        if (!m_running) {
            break;
        }

        // Cleanup idle connections
        cleanupIdleConnections();
    }
}

bool ConnectionPool::validateConnection(std::shared_ptr<network::TCPClient> connection) {
    if (!connection || !connection->isConnected()) {
        return false;
    }

    // In a real implementation, we could send a small ping/pong message to verify the connection
    // For now, we'll just check if the socket is connected
    return true;
}

void ConnectionPool::removeConnectionByClient(const std::string& endpointStr, std::shared_ptr<network::TCPClient> client) {
    if (!client) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_connectionsMutex);

    auto it = m_connections.find(endpointStr);
    if (it != m_connections.end()) {
        auto& connections = it->second;

        // Find the connection with the matching client pointer
        auto connIt = std::find_if(connections.begin(), connections.end(),
            [&client](const std::shared_ptr<PooledConnection>& conn) {
                return conn->client.get() == client.get();
            });

        if (connIt != connections.end()) {
            // Remove the connection from the pool
            connections.erase(connIt);
            unified_event::logDebug("BitTorrent.ConnectionPool", "Removed connection to " + endpointStr + " from pool");

            // If there are no more connections to this endpoint, remove the endpoint from the map
            if (connections.empty()) {
                m_connections.erase(it);
            }
        }
    }
}

} // namespace dht_hunter::bittorrent::metadata
