#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <atomic>
#include <condition_variable>

namespace dht_hunter::network {

/**
 * @brief Configuration for the retry manager
 */
struct RetryConfig {
    size_t maxRetries = 3;                                  ///< Maximum number of retries
    std::chrono::milliseconds initialDelay{100};            ///< Initial delay before first retry
    std::chrono::milliseconds maxDelay{5000};               ///< Maximum delay between retries
    float backoffFactor = 2.0f;                             ///< Backoff factor for exponential backoff
    bool useJitter = true;                                  ///< Whether to use jitter to avoid thundering herd
    float jitterFactor = 0.1f;                              ///< Jitter factor (0.0 - 1.0)
};

/**
 * @brief A retry operation
 */
class RetryOperation {
public:
    /**
     * @brief Constructs a retry operation
     * @param operation The operation to retry
     * @param config The retry configuration
     */
    RetryOperation(std::function<bool()> operation, const RetryConfig& config = RetryConfig());
    
    /**
     * @brief Destructor
     */
    ~RetryOperation();
    
    /**
     * @brief Starts the retry operation
     * @return True if the operation was started, false otherwise
     */
    bool start();
    
    /**
     * @brief Cancels the retry operation
     */
    void cancel();
    
    /**
     * @brief Gets the number of retries
     * @return The number of retries
     */
    size_t getRetryCount() const;
    
    /**
     * @brief Gets the retry configuration
     * @return The retry configuration
     */
    RetryConfig getConfig() const;
    
    /**
     * @brief Sets the retry configuration
     * @param config The retry configuration
     */
    void setConfig(const RetryConfig& config);
    
    /**
     * @brief Sets the success callback
     * @param callback The callback to invoke when the operation succeeds
     */
    void setSuccessCallback(std::function<void()> callback);
    
    /**
     * @brief Sets the failure callback
     * @param callback The callback to invoke when the operation fails
     */
    void setFailureCallback(std::function<void()> callback);
    
    /**
     * @brief Sets the retry callback
     * @param callback The callback to invoke before each retry
     */
    void setRetryCallback(std::function<void(size_t)> callback);
    
private:
    /**
     * @brief Executes the operation
     * @return True if the operation succeeded, false otherwise
     */
    bool execute();
    
    /**
     * @brief Schedules the next retry
     */
    void scheduleRetry();
    
    /**
     * @brief Calculates the delay for the next retry
     * @return The delay for the next retry
     */
    std::chrono::milliseconds calculateDelay() const;
    
    std::function<bool()> m_operation;                      ///< The operation to retry
    std::function<void()> m_successCallback;                ///< The success callback
    std::function<void()> m_failureCallback;                ///< The failure callback
    std::function<void(size_t)> m_retryCallback;            ///< The retry callback
    
    RetryConfig m_config;                                   ///< The retry configuration
    size_t m_retryCount;                                    ///< The number of retries
    std::atomic<bool> m_running;                            ///< Whether the operation is running
    std::atomic<bool> m_cancelled;                          ///< Whether the operation has been cancelled
    
    std::thread m_thread;                                   ///< The retry thread
    std::mutex m_mutex;                                     ///< Mutex for thread safety
};

/**
 * @brief Manages retry operations
 */
class RetryManager {
public:
    /**
     * @brief Gets the singleton instance
     * @return The singleton instance
     */
    static RetryManager& getInstance();
    
    /**
     * @brief Destructor
     */
    ~RetryManager();
    
    /**
     * @brief Creates a retry operation
     * @param operation The operation to retry
     * @param config The retry configuration
     * @return A shared pointer to the retry operation
     */
    std::shared_ptr<RetryOperation> createOperation(std::function<bool()> operation, 
                                                  const RetryConfig& config = RetryConfig());
    
    /**
     * @brief Starts a retry operation
     * @param operation The operation to retry
     * @param config The retry configuration
     * @return A shared pointer to the retry operation
     */
    std::shared_ptr<RetryOperation> startOperation(std::function<bool()> operation, 
                                                 const RetryConfig& config = RetryConfig());
    
    /**
     * @brief Cancels all retry operations
     */
    void cancelAll();
    
    /**
     * @brief Gets the number of active operations
     * @return The number of active operations
     */
    size_t getActiveOperationCount() const;
    
    /**
     * @brief Sets the default retry configuration
     * @param config The default retry configuration
     */
    void setDefaultConfig(const RetryConfig& config);
    
    /**
     * @brief Gets the default retry configuration
     * @return The default retry configuration
     */
    RetryConfig getDefaultConfig() const;
    
private:
    /**
     * @brief Constructs a retry manager
     */
    RetryManager();
    
    /**
     * @brief Copy constructor (deleted)
     */
    RetryManager(const RetryManager&) = delete;
    
    /**
     * @brief Assignment operator (deleted)
     */
    RetryManager& operator=(const RetryManager&) = delete;
    
    /**
     * @brief Removes completed operations
     */
    void removeCompletedOperations();
    
    std::vector<std::shared_ptr<RetryOperation>> m_operations;  ///< Active retry operations
    mutable std::mutex m_mutex;                                ///< Mutex for thread safety
    RetryConfig m_defaultConfig;                               ///< Default retry configuration
};

} // namespace dht_hunter::network
