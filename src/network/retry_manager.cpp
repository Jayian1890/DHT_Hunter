#include "dht_hunter/network/retry_manager.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <algorithm>
#include <random>
#include <chrono>

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.RetryManager");
}

//
// RetryOperation implementation
//

RetryOperation::RetryOperation(std::function<bool()> operation, const RetryConfig& config)
    : m_operation(operation),
      m_config(config),
      m_retryCount(0),
      m_running(false),
      m_cancelled(false) {
}

RetryOperation::~RetryOperation() {
    cancel();
}

bool RetryOperation::start() {
    if (m_running) {
        logger->warning("Retry operation already running");
        return false;
    }
    
    m_running = true;
    m_cancelled = false;
    m_retryCount = 0;
    
    // Execute the operation immediately
    if (execute()) {
        // Operation succeeded
        m_running = false;
        
        if (m_successCallback) {
            m_successCallback();
        }
        
        return true;
    }
    
    // Operation failed, schedule a retry
    scheduleRetry();
    return true;
}

void RetryOperation::cancel() {
    if (!m_running) {
        return;
    }
    
    m_cancelled = true;
    
    if (m_thread.joinable()) {
        m_thread.join();
    }
    
    m_running = false;
}

size_t RetryOperation::getRetryCount() const {
    return m_retryCount;
}

RetryConfig RetryOperation::getConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_config;
}

void RetryOperation::setConfig(const RetryConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void RetryOperation::setSuccessCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_successCallback = callback;
}

void RetryOperation::setFailureCallback(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_failureCallback = callback;
}

void RetryOperation::setRetryCallback(std::function<void(size_t)> callback) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_retryCallback = callback;
}

bool RetryOperation::execute() {
    try {
        return m_operation();
    } catch (const std::exception& e) {
        logger->error("Exception in retry operation: {}", e.what());
        return false;
    } catch (...) {
        logger->error("Unknown exception in retry operation");
        return false;
    }
}

void RetryOperation::scheduleRetry() {
    // Check if we've reached the maximum number of retries
    if (m_retryCount >= m_config.maxRetries) {
        logger->debug("Maximum number of retries reached ({})", m_config.maxRetries);
        
        m_running = false;
        
        if (m_failureCallback) {
            m_failureCallback();
        }
        
        return;
    }
    
    // Increment the retry count
    m_retryCount++;
    
    // Calculate the delay for the next retry
    auto delay = calculateDelay();
    
    logger->debug("Scheduling retry {} of {} in {} ms", 
                 m_retryCount, m_config.maxRetries, delay.count());
    
    // Notify the retry callback
    if (m_retryCallback) {
        m_retryCallback(m_retryCount);
    }
    
    // Schedule the retry
    m_thread = std::thread([this, delay]() {
        // Sleep for the delay
        std::this_thread::sleep_for(delay);
        
        // Check if the operation has been cancelled
        if (m_cancelled) {
            return;
        }
        
        // Execute the operation
        if (execute()) {
            // Operation succeeded
            m_running = false;
            
            if (m_successCallback) {
                m_successCallback();
            }
            
            return;
        }
        
        // Operation failed, schedule another retry
        scheduleRetry();
    });
    
    // Detach the thread
    m_thread.detach();
}

std::chrono::milliseconds RetryOperation::calculateDelay() const {
    // Calculate the delay using exponential backoff
    auto delay = m_config.initialDelay;
    
    for (size_t i = 1; i < m_retryCount; i++) {
        delay = std::chrono::milliseconds(
            static_cast<int>(delay.count() * m_config.backoffFactor));
    }
    
    // Cap the delay at the maximum
    if (delay > m_config.maxDelay) {
        delay = m_config.maxDelay;
    }
    
    // Add jitter if enabled
    if (m_config.useJitter && m_config.jitterFactor > 0.0f) {
        // Create a random number generator
        static std::random_device rd;
        static std::mt19937 gen(rd());
        
        // Calculate the jitter range
        int jitterRange = static_cast<int>(delay.count() * m_config.jitterFactor);
        
        // Create a distribution for the jitter
        std::uniform_int_distribution<int> dist(-jitterRange, jitterRange);
        
        // Add the jitter to the delay
        delay += std::chrono::milliseconds(dist(gen));
        
        // Ensure the delay is not negative
        if (delay.count() < 0) {
            delay = std::chrono::milliseconds(0);
        }
    }
    
    return delay;
}

//
// RetryManager implementation
//

RetryManager::RetryManager()
    : m_defaultConfig() {
    
    logger->debug("Created RetryManager");
}

RetryManager::~RetryManager() {
    cancelAll();
    
    logger->debug("Destroyed RetryManager");
}

RetryManager& RetryManager::getInstance() {
    static RetryManager instance;
    return instance;
}

std::shared_ptr<RetryOperation> RetryManager::createOperation(std::function<bool()> operation, 
                                                            const RetryConfig& config) {
    auto retryOperation = std::make_shared<RetryOperation>(operation, config);
    
    // Add the operation to the list
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_operations.push_back(retryOperation);
    }
    
    return retryOperation;
}

std::shared_ptr<RetryOperation> RetryManager::startOperation(std::function<bool()> operation, 
                                                           const RetryConfig& config) {
    auto retryOperation = createOperation(operation, config);
    
    // Start the operation
    retryOperation->start();
    
    return retryOperation;
}

void RetryManager::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    for (auto& operation : m_operations) {
        operation->cancel();
    }
    
    m_operations.clear();
    
    logger->debug("Cancelled all retry operations");
}

size_t RetryManager::getActiveOperationCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_operations.size();
}

void RetryManager::setDefaultConfig(const RetryConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultConfig = config;
}

RetryConfig RetryManager::getDefaultConfig() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_defaultConfig;
}

void RetryManager::removeCompletedOperations() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove operations that are no longer running
    m_operations.erase(
        std::remove_if(m_operations.begin(), m_operations.end(),
                      [](const std::shared_ptr<RetryOperation>& operation) {
                          return !operation->getRetryCount() == 0;
                      }),
        m_operations.end());
}

} // namespace dht_hunter::network
