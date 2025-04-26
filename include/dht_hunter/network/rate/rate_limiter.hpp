#pragma once

#include <chrono>
#include <mutex>
#include <atomic>

namespace dht_hunter::network {

/**
 * @class RateLimiter
 * @brief Limits the rate of operations.
 *
 * This class provides a mechanism for limiting the rate of operations,
 * such as network requests or message sending.
 */
class RateLimiter {
public:
    /**
     * @brief Constructs a rate limiter.
     * @param maxOperationsPerSecond The maximum number of operations allowed per second.
     * @param burstSize The maximum number of operations allowed in a burst.
     */
    RateLimiter(double maxOperationsPerSecond, size_t burstSize = 1);

    /**
     * @brief Destructor.
     */
    ~RateLimiter();

    /**
     * @brief Tries to acquire permission for an operation.
     * @return True if permission was granted, false otherwise.
     */
    bool tryAcquire();

    /**
     * @brief Tries to acquire permission for multiple operations.
     * @param count The number of operations to acquire permission for.
     * @return True if permission was granted, false otherwise.
     */
    bool tryAcquire(size_t count);

    /**
     * @brief Acquires permission for an operation, blocking if necessary.
     */
    void acquire();

    /**
     * @brief Acquires permission for multiple operations, blocking if necessary.
     * @param count The number of operations to acquire permission for.
     */
    void acquire(size_t count);

    /**
     * @brief Gets the maximum number of operations allowed per second.
     * @return The maximum number of operations allowed per second.
     */
    double getMaxOperationsPerSecond() const;

    /**
     * @brief Sets the maximum number of operations allowed per second.
     * @param maxOperationsPerSecond The maximum number of operations allowed per second.
     */
    void setMaxOperationsPerSecond(double maxOperationsPerSecond);

    /**
     * @brief Gets the maximum number of operations allowed in a burst.
     * @return The maximum number of operations allowed in a burst.
     */
    size_t getBurstSize() const;

    /**
     * @brief Sets the maximum number of operations allowed in a burst.
     * @param burstSize The maximum number of operations allowed in a burst.
     */
    void setBurstSize(size_t burstSize);

    /**
     * @brief Gets the number of operations performed.
     * @return The number of operations performed.
     */
    size_t getOperationCount() const;

    /**
     * @brief Resets the operation count.
     */
    void resetOperationCount();

private:
    /**
     * @brief Updates the token count based on the elapsed time.
     */
    void updateTokens();

    double m_maxOperationsPerSecond;
    size_t m_burstSize;
    double m_tokens;
    std::chrono::steady_clock::time_point m_lastUpdateTime;
    std::atomic<size_t> m_operationCount;
    mutable std::mutex m_mutex;
};

} // namespace dht_hunter::network
