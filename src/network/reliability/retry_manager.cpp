#include "dht_hunter/network/reliability/retry_manager.hpp"
#include <algorithm>

namespace dht_hunter::network {

RetryManager::RetryManager(size_t maxRetries, std::chrono::milliseconds initialDelay, std::chrono::milliseconds maxDelay, std::chrono::milliseconds timeout)
    : m_maxRetries(maxRetries), m_initialDelay(initialDelay), m_maxDelay(maxDelay), m_timeout(timeout), m_nextOperationId(1) {
}

RetryManager::~RetryManager() {
    clear();
}

uint32_t RetryManager::startRetry(RetryCallback callback, CompletionCallback completionCallback) {
    if (!callback) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Generate an operation ID
    uint32_t operationId = m_nextOperationId++;

    // Try the operation immediately
    bool success = callback();
    if (success) {
        // Operation succeeded on the first try
        if (completionCallback) {
            completionCallback(true, operationId);
        }
        return operationId;
    }

    // Store the retry operation
    RetryOperation operation;
    operation.callback = callback;
    operation.completionCallback = completionCallback;
    operation.startTime = std::chrono::steady_clock::now();
    operation.nextRetryTime = operation.startTime + m_initialDelay;
    operation.retryCount = 0;

    m_operations[operationId] = operation;

    return operationId;
}

bool RetryManager::cancelRetry(uint32_t operationId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_operations.find(operationId);
    if (it != m_operations.end()) {
        // Call the completion callback with failure
        if (it->second.completionCallback) {
            it->second.completionCallback(false, operationId);
        }
        m_operations.erase(it);
        return true;
    }

    return false;
}

void RetryManager::update(std::chrono::steady_clock::time_point currentTime) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto it = m_operations.begin(); it != m_operations.end();) {
        auto& operation = it->second;
        uint32_t operationId = it->first;

        // Check if the operation has timed out
        if (currentTime - operation.startTime > m_timeout) {
            // Operation timed out
            if (operation.completionCallback) {
                operation.completionCallback(false, operationId);
            }
            it = m_operations.erase(it);
            continue;
        }

        // Check if it's time for a retry
        if (currentTime >= operation.nextRetryTime) {
            // Increment the retry count
            ++operation.retryCount;

            // Try the operation
            bool success = operation.callback();
            if (success) {
                // Operation succeeded
                if (operation.completionCallback) {
                    operation.completionCallback(true, operationId);
                }
                it = m_operations.erase(it);
                continue;
            }

            // Check if we've reached the maximum number of retries
            if (operation.retryCount >= m_maxRetries) {
                // Operation failed after all retries
                if (operation.completionCallback) {
                    operation.completionCallback(false, operationId);
                }
                it = m_operations.erase(it);
                continue;
            }

            // Calculate the next retry time
            operation.nextRetryTime = currentTime + calculateDelay(operation.retryCount);
        }

        ++it;
    }
}

size_t RetryManager::getMaxRetries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxRetries;
}

void RetryManager::setMaxRetries(size_t maxRetries) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxRetries = maxRetries;
}

std::chrono::milliseconds RetryManager::getInitialDelay() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialDelay;
}

void RetryManager::setInitialDelay(std::chrono::milliseconds initialDelay) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_initialDelay = initialDelay;
}

std::chrono::milliseconds RetryManager::getMaxDelay() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxDelay;
}

void RetryManager::setMaxDelay(std::chrono::milliseconds maxDelay) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxDelay = maxDelay;
}

std::chrono::milliseconds RetryManager::getTimeout() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_timeout;
}

void RetryManager::setTimeout(std::chrono::milliseconds timeout) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_timeout = timeout;
}

size_t RetryManager::getActiveOperationCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_operations.size();
}

void RetryManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Call completion callbacks with failure
    for (auto& pair : m_operations) {
        if (pair.second.completionCallback) {
            pair.second.completionCallback(false, pair.first);
        }
    }

    m_operations.clear();
}

std::chrono::milliseconds RetryManager::calculateDelay(size_t retryCount) const {
    // Exponential backoff: initialDelay * 2^retryCount
    auto delay = m_initialDelay;
    for (size_t i = 0; i < retryCount; ++i) {
        delay *= 2;
        if (delay > m_maxDelay) {
            return m_maxDelay;
        }
    }
    return delay;
}

} // namespace dht_hunter::network
