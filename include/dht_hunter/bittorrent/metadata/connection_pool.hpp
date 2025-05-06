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
#include <atomic>
#include <deque>

namespace dht_hunter::bittorrent::metadata {

/**
 * @brief Connection pool statistics for monitoring
 */
struct ConnectionPoolStats {
    std::atomic<size_t> totalConnections{0};        // Total number of connections in the pool
    std::atomic<size_t> activeConnections{0};       // Number of active connections
    std::atomic<size_t> idleConnections{0};         // Number of idle connections
    std::atomic<size_t> failedConnections{0};       // Number of failed connection attempts
    std::atomic<size_t> successfulConnections{0};   // Number of successful connection attempts
    std::atomic<size_t> reuseCount{0};              // Number of times connections were reused
    std::atomic<size_t> creationCount{0};           // Number of new connections created
    std::atomic<size_t> validationFailures{0};      // Number of connections that failed validation
    std::atomic<size_t> cleanupCount{0};            // Number of connections removed during cleanup
};

/**
 * @brief Connection health metrics for an endpoint
 */
struct EndpointHealth {
    float successRate{0.5f};                        // Success rate for connections to this endpoint (0.0-1.0)
    int consecutiveFailures{0};                     // Number of consecutive connection failures
    int consecutiveSuccesses{0};                    // Number of consecutive successful connections
    std::chrono::steady_clock::time_point lastAttempt; // Time of last connection attempt
    std::deque<bool> recentResults;                 // Recent connection results (true=success, false=failure)
    static constexpr size_t MAX_HISTORY = 10;       // Maximum number of results to keep

    // Update health metrics with a connection result
    void updateHealth(bool success) {
        // Add to history
        recentResults.push_back(success);
        if (recentResults.size() > MAX_HISTORY) {
            recentResults.pop_front();
        }

        // Update success rate
        size_t successCount = 0;
        for (bool result : recentResults) {
            if (result) successCount++;
        }
        successRate = recentResults.empty() ? 0.5f : static_cast<float>(successCount) / recentResults.size();

        // Update consecutive counts
        if (success) {
            consecutiveSuccesses++;
            consecutiveFailures = 0;
        } else {
            consecutiveFailures++;
            consecutiveSuccesses = 0;
        }

        // Update last attempt time
        lastAttempt = std::chrono::steady_clock::now();
    }

    // Check if this endpoint is in a circuit-breaker state (too many failures)
    bool isCircuitBroken() const {
        // Circuit is broken if success rate is below 20% with at least 3 attempts
        // or if there have been 5 consecutive failures
        return (successRate < 0.2f && recentResults.size() >= 3) || consecutiveFailures >= 5;
    }

    // Get backoff time in milliseconds based on consecutive failures
    int getBackoffTimeMs() const {
        if (consecutiveFailures <= 0) return 0;
        // Exponential backoff: 100ms, 200ms, 400ms, 800ms, etc. up to 30 seconds
        int backoff = 100 * (1 << (consecutiveFailures - 1));
        return std::min(backoff, 30000); // Cap at 30 seconds
    }
};

/**
 * @brief An enhanced pool of TCP connections with health tracking and circuit breaker pattern
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
     * @brief Gets a connection from the pool or creates a new one with configurable timeout
     * @param endpoint The endpoint to connect to
     * @param dataCallback The data received callback
     * @param errorCallback The error callback
     * @param closedCallback The connection closed callback
     * @param timeoutSec Connection timeout in seconds (default: 5)
     * @return A shared pointer to the connection
     */
    std::shared_ptr<network::TCPClient> getConnection(
        const network::EndPoint& endpoint,
        std::function<void(const uint8_t*, size_t)> dataCallback,
        std::function<void(const std::string&)> errorCallback,
        std::function<void()> closedCallback,
        int timeoutSec = 5);

    /**
     * @brief Gets the connection pool statistics
     * @return The connection pool statistics
     */
    const ConnectionPoolStats& getStats() const;

    /**
     * @brief Gets the health metrics for a specific endpoint
     * @param endpoint The endpoint to get health metrics for
     * @return The endpoint health metrics
     */
    EndpointHealth getEndpointHealth(const network::EndPoint& endpoint) const;

    /**
     * @brief Returns a connection to the pool
     * @param endpoint The endpoint the connection was for
     * @param connection The connection to return
     */
    void returnConnection(
        const network::EndPoint& endpoint,
        std::shared_ptr<network::TCPClient> connection);

    /**
     * @brief Checks if an endpoint is in circuit breaker state
     * @param endpoint The endpoint to check
     * @return True if the circuit is broken (connections should not be attempted)
     */
    bool isCircuitBroken(const network::EndPoint& endpoint) const;

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
     * @brief Represents a pooled connection with enhanced tracking
     */
    struct PooledConnection {
        std::shared_ptr<network::TCPClient> client;
        std::chrono::steady_clock::time_point lastUsed;
        std::chrono::steady_clock::time_point createdTime;
        bool inUse;
        int useCount;
        float quality; // Connection quality score (0.0-1.0)

        PooledConnection(std::shared_ptr<network::TCPClient> c)
            : client(c),
              lastUsed(std::chrono::steady_clock::now()),
              createdTime(std::chrono::steady_clock::now()),
              inUse(true),
              useCount(1),
              quality(1.0f) {}

        // Update connection quality based on recent activity
        void updateQuality() {
            if (!client) {
                quality = 0.0f;
                return;
            }

            // Get connection stats
            const auto& stats = client->getStats();

            // Calculate quality factors
            float errorFactor = stats.errorCount > 0 ? 1.0f / (1.0f + stats.errorCount) : 1.0f;
            float ageFactor = 1.0f; // Decrease quality for very old connections

            auto now = std::chrono::steady_clock::now();
            auto ageHours = std::chrono::duration_cast<std::chrono::hours>(now - createdTime).count();
            if (ageHours > 24) {
                ageFactor = 0.8f; // Older than 24 hours
            }

            // Calculate overall quality
            quality = errorFactor * ageFactor;
        }
    };

    /**
     * @brief Updates endpoint health with connection result
     * @param endpoint The endpoint
     * @param success Whether the connection was successful
     */
    void updateEndpointHealth(const network::EndPoint& endpoint, bool success);



    // Static instance for singleton pattern
    static std::shared_ptr<ConnectionPool> s_instance;
    static std::mutex s_instanceMutex;

    // Connection pool
    std::unordered_map<std::string, std::vector<std::shared_ptr<PooledConnection>>> m_connections;
    std::mutex m_connectionsMutex;

    // Endpoint health tracking
    std::unordered_map<std::string, EndpointHealth> m_endpointHealth;
    mutable std::mutex m_healthMutex;

    // Cleanup thread
    std::thread m_cleanupThread;
    std::atomic<bool> m_running{false};
    std::condition_variable m_cleanupCondition;
    std::mutex m_cleanupMutex;

    // Statistics
    ConnectionPoolStats m_stats;

    // Constants
    static constexpr int MAX_IDLE_TIME_SECONDS = 60;
    static constexpr int CLEANUP_INTERVAL_SECONDS = 30;
    static constexpr int MAX_CONNECTIONS_PER_ENDPOINT = 5;
    static constexpr int MAX_TOTAL_CONNECTIONS = 100;
    static constexpr int CIRCUIT_BREAKER_RESET_SECONDS = 60; // Time before retrying a broken circuit

    /**
     * @brief Cleans up idle connections periodically
     */
    void cleanupIdleConnectionsPeriodically();
};

} // namespace dht_hunter::bittorrent::metadata
