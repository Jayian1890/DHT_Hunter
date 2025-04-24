#pragma once

#include <chrono>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace dht_hunter::network {

/**
 * @brief Implements a token bucket algorithm for rate limiting
 * 
 * This class provides a mechanism to limit the rate of data transfer
 * using a token bucket algorithm. Tokens are added to the bucket at
 * a constant rate, and each data transfer consumes tokens equal to
 * the amount of data transferred.
 */
class RateLimiter {
public:
    /**
     * @brief Constructs a rate limiter with the specified rate
     * @param bytesPerSecond The maximum number of bytes per second to allow
     * @param burstSize The maximum burst size in bytes (default: 2x rate)
     */
    RateLimiter(size_t bytesPerSecond, size_t burstSize = 0);

    /**
     * @brief Destructor
     */
    ~RateLimiter() = default;

    /**
     * @brief Request permission to transfer bytes
     * 
     * This method checks if there are enough tokens available to transfer
     * the requested number of bytes. If there are, it consumes the tokens
     * and returns true. If not, it returns false.
     * 
     * @param bytes The number of bytes to transfer
     * @return True if the transfer is allowed, false otherwise
     */
    bool request(size_t bytes);

    /**
     * @brief Request permission to transfer bytes, waiting if necessary
     * 
     * This method is similar to request(), but it will wait until enough
     * tokens are available if there aren't enough initially.
     * 
     * @param bytes The number of bytes to transfer
     * @param maxWaitTime The maximum time to wait for tokens
     * @return True if the transfer is allowed, false if the wait timed out
     */
    bool requestWithWait(size_t bytes, std::chrono::milliseconds maxWaitTime);

    /**
     * @brief Set the rate limit
     * @param bytesPerSecond The new rate limit in bytes per second
     */
    void setRate(size_t bytesPerSecond);

    /**
     * @brief Get the current rate limit
     * @return The current rate limit in bytes per second
     */
    size_t getRate() const;

    /**
     * @brief Set the burst size
     * @param burstSize The new burst size in bytes
     */
    void setBurstSize(size_t burstSize);

    /**
     * @brief Get the current burst size
     * @return The current burst size in bytes
     */
    size_t getBurstSize() const;

    /**
     * @brief Get the number of available tokens
     * @return The number of available tokens
     */
    size_t getAvailableTokens() const;

private:
    /**
     * @brief Update the token count based on elapsed time
     */
    void updateTokens();

    std::atomic<size_t> m_bytesPerSecond;      ///< Rate limit in bytes per second
    std::atomic<size_t> m_burstSize;           ///< Maximum burst size in bytes
    size_t m_availableTokens;                  ///< Current number of available tokens
    std::chrono::steady_clock::time_point m_lastUpdate; ///< Time of last token update
    mutable std::mutex m_mutex;                ///< Mutex for thread safety
    std::condition_variable m_cv;              ///< Condition variable for waiting
};

/**
 * @brief Implements a connection throttler to limit concurrent connections
 * 
 * This class provides a mechanism to limit the number of concurrent
 * connections to prevent resource exhaustion.
 */
class ConnectionThrottler {
public:
    /**
     * @brief Constructs a connection throttler with the specified limit
     * @param maxConnections The maximum number of concurrent connections
     */
    explicit ConnectionThrottler(size_t maxConnections);

    /**
     * @brief Destructor
     */
    ~ConnectionThrottler() = default;

    /**
     * @brief Request permission to create a new connection
     * @return True if the connection is allowed, false otherwise
     */
    bool requestConnection();

    /**
     * @brief Request permission to create a new connection, waiting if necessary
     * @param maxWaitTime The maximum time to wait for a connection slot
     * @return True if the connection is allowed, false if the wait timed out
     */
    bool requestConnectionWithWait(std::chrono::milliseconds maxWaitTime);

    /**
     * @brief Release a connection
     */
    void releaseConnection();

    /**
     * @brief Set the maximum number of concurrent connections
     * @param maxConnections The new maximum number of concurrent connections
     */
    void setMaxConnections(size_t maxConnections);

    /**
     * @brief Get the maximum number of concurrent connections
     * @return The maximum number of concurrent connections
     */
    size_t getMaxConnections() const;

    /**
     * @brief Get the current number of active connections
     * @return The current number of active connections
     */
    size_t getActiveConnections() const;

private:
    std::atomic<size_t> m_maxConnections;      ///< Maximum number of concurrent connections
    std::atomic<size_t> m_activeConnections;   ///< Current number of active connections
    mutable std::mutex m_mutex;                ///< Mutex for thread safety
    std::condition_variable m_cv;              ///< Condition variable for waiting
};

/**
 * @brief Implements a burst controller to handle traffic bursts
 * 
 * This class provides a mechanism to handle traffic bursts by
 * smoothing out the rate of data transfer over time.
 */
class BurstController {
public:
    /**
     * @brief Constructs a burst controller with the specified parameters
     * @param targetRate The target rate in bytes per second
     * @param maxBurstRate The maximum burst rate in bytes per second
     * @param burstDuration The maximum duration of a burst in milliseconds
     */
    BurstController(size_t targetRate, size_t maxBurstRate, std::chrono::milliseconds burstDuration);

    /**
     * @brief Destructor
     */
    ~BurstController() = default;

    /**
     * @brief Process a data transfer and get the recommended delay
     * 
     * This method processes a data transfer and returns the recommended
     * delay before the next transfer to maintain the target rate.
     * 
     * @param bytes The number of bytes transferred
     * @return The recommended delay before the next transfer
     */
    std::chrono::milliseconds processTransfer(size_t bytes);

    /**
     * @brief Set the target rate
     * @param targetRate The new target rate in bytes per second
     */
    void setTargetRate(size_t targetRate);

    /**
     * @brief Get the current target rate
     * @return The current target rate in bytes per second
     */
    size_t getTargetRate() const;

    /**
     * @brief Set the maximum burst rate
     * @param maxBurstRate The new maximum burst rate in bytes per second
     */
    void setMaxBurstRate(size_t maxBurstRate);

    /**
     * @brief Get the current maximum burst rate
     * @return The current maximum burst rate in bytes per second
     */
    size_t getMaxBurstRate() const;

    /**
     * @brief Set the burst duration
     * @param burstDuration The new burst duration in milliseconds
     */
    void setBurstDuration(std::chrono::milliseconds burstDuration);

    /**
     * @brief Get the current burst duration
     * @return The current burst duration in milliseconds
     */
    std::chrono::milliseconds getBurstDuration() const;

private:
    std::atomic<size_t> m_targetRate;          ///< Target rate in bytes per second
    std::atomic<size_t> m_maxBurstRate;        ///< Maximum burst rate in bytes per second
    std::atomic<std::chrono::milliseconds> m_burstDuration; ///< Maximum burst duration
    
    size_t m_bytesInCurrentBurst;              ///< Bytes transferred in the current burst
    std::chrono::steady_clock::time_point m_burstStartTime; ///< Start time of the current burst
    mutable std::mutex m_mutex;                ///< Mutex for thread safety
};

} // namespace dht_hunter::network
