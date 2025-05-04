#pragma once

#include "dht_hunter/network/tcp_client.hpp"
#include "dht_hunter/types/endpoint.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <memory>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>
#include <functional>

namespace dht_hunter::bittorrent::metadata {

/**
 * @brief A pool of TCP connections for reuse
 */
class ConnectionPool {
public:
    /**
     * @brief Get the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<ConnectionPool> getInstance();

    /**
     * @brief Destructor
     */
    ~ConnectionPool();

    /**
     * @brief Gets a connection from the pool or creates a new one
     * @param endpoint The endpoint to connect to
     * @param dataCallback The data received callback
     * @param errorCallback The error callback
     * @param closedCallback The connection closed callback
     * @return A shared pointer to the connection
     */
    std::shared_ptr<network::TCPClient> getConnection(
        const network::EndPoint& endpoint,
        std::function<void(const uint8_t*, size_t)> dataCallback,
        std::function<void(const std::string&)> errorCallback,
        std::function<void()> closedCallback);

    /**
     * @brief Returns a connection to the pool
     * @param endpoint The endpoint the connection was for
     * @param connection The connection to return
     */
    void returnConnection(
        const network::EndPoint& endpoint,
        std::shared_ptr<network::TCPClient> connection);

    /**
     * @brief Cleans up idle connections
     */
    void cleanupIdleConnections();

    /**
     * @brief Starts the connection pool
     */
    void start();

    /**
     * @brief Stops the connection pool
     */
    void stop();

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    ConnectionPool();

    /**
     * @brief Validates a connection to ensure it's healthy
     * @param connection The connection to validate
     * @return True if the connection is valid, false otherwise
     */
    bool validateConnection(std::shared_ptr<network::TCPClient> connection);

    /**
     * @brief Removes a connection from the pool by client pointer
     * @param endpointStr The endpoint string
     * @param client The client pointer
     */
    void removeConnectionByClient(const std::string& endpointStr, std::shared_ptr<network::TCPClient> client);

    /**
     * @brief Represents a pooled connection
     */
    struct PooledConnection {
        std::shared_ptr<network::TCPClient> client;
        std::chrono::steady_clock::time_point lastUsed;
        bool inUse;

        PooledConnection(std::shared_ptr<network::TCPClient> c)
            : client(c), lastUsed(std::chrono::steady_clock::now()), inUse(true) {}
    };

    // Static instance for singleton pattern
    static std::shared_ptr<ConnectionPool> s_instance;
    static std::mutex s_instanceMutex;

    // Connection pool
    std::unordered_map<std::string, std::vector<std::shared_ptr<PooledConnection>>> m_connections;
    std::mutex m_connectionsMutex;

    // Cleanup thread
    std::thread m_cleanupThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_cleanupCondition;
    std::mutex m_cleanupMutex;

    // Constants
    static constexpr int MAX_IDLE_TIME_SECONDS = 60;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 30;
    static constexpr int MAX_CONNECTIONS_PER_ENDPOINT = 5;
    static constexpr int MAX_TOTAL_CONNECTIONS = 100;

    /**
     * @brief Cleans up idle connections periodically
     */
    void cleanupIdleConnectionsPeriodically();
};

} // namespace dht_hunter::bittorrent::metadata
