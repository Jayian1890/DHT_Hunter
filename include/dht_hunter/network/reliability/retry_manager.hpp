#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dht_hunter::network {

/**
 * @class RetryManager
 * @brief Manages retries for operations.
 *
 * This class provides a mechanism for retrying operations with exponential backoff,
 * timeouts, and maximum retry limits.
 */
class RetryManager {
public:
    /**
     * @brief Callback for retry operations.
     * @return True if the operation succeeded, false if it should be retried.
     */
    using RetryCallback = std::function<bool()>;

    /**
     * @brief Callback for completion.
     * @param success True if the operation succeeded, false if it failed after all retries.
     * @param operationId The ID of the operation.
     */
    using CompletionCallback = std::function<void(bool success, uint32_t operationId)>;

    /**
     * @brief Constructs a retry manager.
     * @param maxRetries The maximum number of retries.
     * @param initialDelay The initial delay between retries.
     * @param maxDelay The maximum delay between retries.
     * @param timeout The timeout for the entire operation.
     */
    RetryManager(size_t maxRetries = 3,
                 std::chrono::milliseconds initialDelay = std::chrono::milliseconds(100),
                 std::chrono::milliseconds maxDelay = std::chrono::milliseconds(5000),
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(30000));

    /**
     * @brief Destructor.
     */
    ~RetryManager();

    /**
     * @brief Starts a retry operation.
     * @param callback The retry callback.
     * @param completionCallback The completion callback.
     * @return The operation ID.
     */
    uint32_t startRetry(RetryCallback callback, CompletionCallback completionCallback = nullptr);

    /**
     * @brief Cancels a retry operation.
     * @param operationId The ID of the operation to cancel.
     * @return True if the operation was cancelled, false if it wasn't found.
     */
    bool cancelRetry(uint32_t operationId);

    /**
     * @brief Updates the retry manager.
     * @param currentTime The current time.
     */
    void update(std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now());

    /**
     * @brief Gets the maximum number of retries.
     * @return The maximum number of retries.
     */
    size_t getMaxRetries() const;

    /**
     * @brief Sets the maximum number of retries.
     * @param maxRetries The maximum number of retries.
     */
    void setMaxRetries(size_t maxRetries);

    /**
     * @brief Gets the initial delay between retries.
     * @return The initial delay.
     */
    std::chrono::milliseconds getInitialDelay() const;

    /**
     * @brief Sets the initial delay between retries.
     * @param initialDelay The initial delay.
     */
    void setInitialDelay(std::chrono::milliseconds initialDelay);

    /**
     * @brief Gets the maximum delay between retries.
     * @return The maximum delay.
     */
    std::chrono::milliseconds getMaxDelay() const;

    /**
     * @brief Sets the maximum delay between retries.
     * @param maxDelay The maximum delay.
     */
    void setMaxDelay(std::chrono::milliseconds maxDelay);

    /**
     * @brief Gets the timeout for the entire operation.
     * @return The timeout.
     */
    std::chrono::milliseconds getTimeout() const;

    /**
     * @brief Sets the timeout for the entire operation.
     * @param timeout The timeout.
     */
    void setTimeout(std::chrono::milliseconds timeout);

    /**
     * @brief Gets the number of active retry operations.
     * @return The number of active operations.
     */
    size_t getActiveOperationCount() const;

    /**
     * @brief Clears all retry operations.
     */
    void clear();

private:
    /**
     * @brief Calculates the delay for a retry.
     * @param retryCount The number of retries so far.
     * @return The delay.
     */
    std::chrono::milliseconds calculateDelay(size_t retryCount) const;

    struct RetryOperation {
        RetryCallback callback;
        CompletionCallback completionCallback;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point nextRetryTime;
        size_t retryCount;
    };

    size_t m_maxRetries;
    std::chrono::milliseconds m_initialDelay;
    std::chrono::milliseconds m_maxDelay;
    std::chrono::milliseconds m_timeout;
    std::unordered_map<uint32_t, RetryOperation> m_operations;
    uint32_t m_nextOperationId;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
