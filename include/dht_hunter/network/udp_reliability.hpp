#pragma once

#include "dht_hunter/network/socket.hpp"
#include "dht_hunter/network/rate_limiter.hpp"
#include "dht_hunter/network/network_address.hpp"

#include <chrono>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <string>
#include <queue>
#include <random>
#include <thread>

namespace dht_hunter::network {

/**
 * @brief Configuration for UDP reliability enhancements
 */
struct UDPReliabilityConfig {
    // Retry parameters
    int maxRetries = 3;                                 ///< Maximum number of retries
    std::chrono::milliseconds initialTimeout{500};      ///< Initial timeout before first retry
    std::chrono::milliseconds maxTimeout{8000};         ///< Maximum timeout
    double timeoutMultiplier = 2.0;                     ///< Multiplier for exponential backoff

    // Duplicate detection parameters
    std::chrono::milliseconds duplicateDetectionWindow{60000}; ///< Window for duplicate detection (1 minute)
    size_t maxCachedTransactions = 1000;                ///< Maximum number of cached transactions for duplicate detection

    // Rate limiting parameters
    size_t maxBytesPerSecond = 0;                       ///< Maximum bytes per second (0 = unlimited)
    size_t burstSize = 0;                               ///< Burst size (0 = 2x maxBytesPerSecond)

    // Connection throttling parameters
    size_t maxConcurrentTransactions = 100;             ///< Maximum number of concurrent transactions
};

/**
 * @brief Callback for UDP message responses
 * @param data The response data
 * @param size The size of the response data
 * @param endpoint The endpoint that sent the response
 */
using UDPResponseCallback = std::function<void(const uint8_t* data, int size, const network::EndPoint& endpoint)>;

/**
 * @brief Callback for UDP message timeouts
 * @param transactionId The transaction ID that timed out
 */
using UDPTimeoutCallback = std::function<void(const std::string& transactionId)>;

/**
 * @brief Callback for UDP message errors
 * @param transactionId The transaction ID that had an error
 * @param errorCode The error code
 * @param errorMessage The error message
 */
using UDPErrorCallback = std::function<void(const std::string& transactionId, int errorCode, const std::string& errorMessage)>;

/**
 * @brief Represents a UDP transaction
 */
struct UDPTransaction {
    std::string transactionId;                          ///< Transaction ID
    std::vector<uint8_t> data;                          ///< Message data
    network::EndPoint endpoint;                         ///< Destination endpoint
    std::chrono::steady_clock::time_point sentTime;     ///< Time when the message was sent
    std::chrono::steady_clock::time_point expiryTime;   ///< Time when the transaction expires
    int retries = 0;                                    ///< Number of retries
    UDPResponseCallback responseCallback;               ///< Callback for responses
    UDPTimeoutCallback timeoutCallback;                 ///< Callback for timeouts
    UDPErrorCallback errorCallback;                     ///< Callback for errors
};

/**
 * @brief Enhances UDP reliability with retries, duplicate detection, and rate limiting
 */
class UDPReliabilityEnhancer {
public:
    /**
     * @brief Constructs a UDP reliability enhancer
     * @param socket The UDP socket to use
     * @param config The configuration
     */
    UDPReliabilityEnhancer(std::shared_ptr<Socket> socket, const UDPReliabilityConfig& config = UDPReliabilityConfig());

    /**
     * @brief Destructor
     */
    ~UDPReliabilityEnhancer();

    /**
     * @brief Starts the reliability enhancer
     * @return True if started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the reliability enhancer
     */
    void stop();

    /**
     * @brief Checks if the reliability enhancer is running
     * @return True if running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Sends a UDP message with reliability enhancements
     * @param data The message data
     * @param size The size of the message data
     * @param endpoint The destination endpoint
     * @param transactionId The transaction ID (if empty, a random one will be generated)
     * @param responseCallback The callback for responses
     * @param timeoutCallback The callback for timeouts
     * @param errorCallback The callback for errors
     * @return True if the message was sent successfully, false otherwise
     */
    bool sendMessage(const uint8_t* data, int size, const network::EndPoint& endpoint,
                    const std::string& transactionId = "",
                    UDPResponseCallback responseCallback = nullptr,
                    UDPTimeoutCallback timeoutCallback = nullptr,
                    UDPErrorCallback errorCallback = nullptr);

    /**
     * @brief Processes a received UDP message
     * @param data The message data
     * @param size The size of the message data
     * @param endpoint The source endpoint
     * @return True if the message was processed successfully, false otherwise
     */
    bool processReceivedMessage(const uint8_t* data, int size, const network::EndPoint& endpoint);

    /**
     * @brief Sets the configuration
     * @param config The new configuration
     */
    void setConfig(const UDPReliabilityConfig& config);

    /**
     * @brief Gets the current configuration
     * @return The current configuration
     */
    UDPReliabilityConfig getConfig() const;

    /**
     * @brief Gets the number of active transactions
     * @return The number of active transactions
     */
    size_t getActiveTransactionCount() const;

    /**
     * @brief Gets the number of duplicate messages detected
     * @return The number of duplicate messages detected
     */
    size_t getDuplicateCount() const;

    /**
     * @brief Gets the number of retries performed
     * @return The number of retries performed
     */
    size_t getRetryCount() const;

    /**
     * @brief Gets the number of timeouts
     * @return The number of timeouts
     */
    size_t getTimeoutCount() const;

private:
    /**
     * @brief Generates a random transaction ID
     * @return A random transaction ID
     */
    std::string generateTransactionId();

    /**
     * @brief Checks for timed out transactions
     */
    void checkTimeouts();

    /**
     * @brief Thread function for checking timeouts
     */
    void timeoutThread();

    /**
     * @brief Retries a transaction
     * @param transaction The transaction to retry
     */
    void retryTransaction(std::shared_ptr<UDPTransaction> transaction);

    /**
     * @brief Removes a transaction
     * @param transactionId The transaction ID to remove
     */
    void removeTransaction(const std::string& transactionId);

    /**
     * @brief Cleans up old cached transactions
     */
    void cleanupCachedTransactions();

    std::shared_ptr<Socket> m_socket;                   ///< The UDP socket
    UDPReliabilityConfig m_config;                      ///< Configuration

    std::unordered_map<std::string, std::shared_ptr<UDPTransaction>> m_transactions; ///< Active transactions
    std::mutex m_transactionsMutex;                     ///< Mutex for transactions

    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_cachedTransactions; ///< Cached transactions for duplicate detection
    std::mutex m_cachedTransactionsMutex;               ///< Mutex for cached transactions

    std::unique_ptr<RateLimiter> m_rateLimiter;         ///< Rate limiter
    std::unique_ptr<ConnectionThrottler> m_throttler;   ///< Connection throttler

    std::atomic<bool> m_running;                        ///< Whether the enhancer is running
    std::thread m_timeoutThread;                        ///< Thread for checking timeouts

    std::atomic<size_t> m_duplicateCount;               ///< Number of duplicate messages detected
    std::atomic<size_t> m_retryCount;                   ///< Number of retries performed
    std::atomic<size_t> m_timeoutCount;                 ///< Number of timeouts

    std::mt19937 m_rng;                                 ///< Random number generator
};

} // namespace dht_hunter::network
