#include "dht_hunter/network/udp_reliability.hpp"
#include "dht_hunter/logforge/logforge.hpp"

#include <algorithm>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.UDPReliability");
}

UDPReliabilityEnhancer::UDPReliabilityEnhancer(std::shared_ptr<Socket> socket, const UDPReliabilityConfig& config)
    : m_socket(socket),
      m_config(config),
      m_running(false),
      m_duplicateCount(0),
      m_retryCount(0),
      m_timeoutCount(0) {
    
    // Initialize random number generator
    std::random_device rd;
    m_rng.seed(rd());
    
    // Create rate limiter if rate limiting is enabled
    if (m_config.maxBytesPerSecond > 0) {
        m_rateLimiter = std::make_unique<RateLimiter>(m_config.maxBytesPerSecond, m_config.burstSize);
    }
    
    // Create connection throttler
    m_throttler = std::make_unique<ConnectionThrottler>(m_config.maxConcurrentTransactions);
    
    logger->debug("Created UDPReliabilityEnhancer with max retries: {}, initial timeout: {} ms",
                 m_config.maxRetries, m_config.initialTimeout.count());
}

UDPReliabilityEnhancer::~UDPReliabilityEnhancer() {
    stop();
}

bool UDPReliabilityEnhancer::start() {
    if (m_running) {
        logger->warning("UDPReliabilityEnhancer already running");
        return false;
    }
    
    if (!m_socket) {
        logger->error("No socket provided");
        return false;
    }
    
    m_running = true;
    
    // Start timeout thread
    m_timeoutThread = std::thread(&UDPReliabilityEnhancer::timeoutThread, this);
    
    logger->debug("UDPReliabilityEnhancer started");
    return true;
}

void UDPReliabilityEnhancer::stop() {
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    // Wait for timeout thread to finish
    if (m_timeoutThread.joinable()) {
        m_timeoutThread.join();
    }
    
    // Clear all transactions
    {
        std::lock_guard<std::mutex> lock(m_transactionsMutex);
        m_transactions.clear();
    }
    
    // Clear cached transactions
    {
        std::lock_guard<std::mutex> lock(m_cachedTransactionsMutex);
        m_cachedTransactions.clear();
    }
    
    logger->debug("UDPReliabilityEnhancer stopped");
}

bool UDPReliabilityEnhancer::isRunning() const {
    return m_running;
}

bool UDPReliabilityEnhancer::sendMessage(const uint8_t* data, int size, const EndPoint& endpoint,
                                        const std::string& transactionId,
                                        UDPResponseCallback responseCallback,
                                        UDPTimeoutCallback timeoutCallback,
                                        UDPErrorCallback errorCallback) {
    if (!m_running) {
        logger->error("UDPReliabilityEnhancer not running");
        return false;
    }
    
    if (!m_socket) {
        logger->error("No socket provided");
        return false;
    }
    
    if (size <= 0 || !data) {
        logger->error("Invalid data");
        return false;
    }
    
    // Check if we're below the connection limit
    if (!m_throttler->requestConnection()) {
        logger->warning("Connection limit reached, message not sent");
        
        // Call error callback if provided
        if (errorCallback) {
            errorCallback(transactionId, -1, "Connection limit reached");
        }
        
        return false;
    }
    
    // Check rate limit if enabled
    if (m_rateLimiter) {
        if (!m_rateLimiter->request(static_cast<size_t>(size))) {
            logger->warning("Rate limit reached, message not sent");
            
            // Release connection
            m_throttler->releaseConnection();
            
            // Call error callback if provided
            if (errorCallback) {
                errorCallback(transactionId, -2, "Rate limit reached");
            }
            
            return false;
        }
    }
    
    // Generate transaction ID if not provided
    std::string actualTransactionId = transactionId.empty() ? generateTransactionId() : transactionId;
    
    // Create transaction
    auto transaction = std::make_shared<UDPTransaction>();
    transaction->transactionId = actualTransactionId;
    transaction->data.assign(data, data + size);
    transaction->endpoint = endpoint;
    transaction->sentTime = std::chrono::steady_clock::now();
    transaction->expiryTime = transaction->sentTime + m_config.initialTimeout;
    transaction->retries = 0;
    transaction->responseCallback = responseCallback;
    transaction->timeoutCallback = timeoutCallback;
    transaction->errorCallback = errorCallback;
    
    // Add transaction to map
    {
        std::lock_guard<std::mutex> lock(m_transactionsMutex);
        m_transactions[actualTransactionId] = transaction;
    }
    
    // Send the message
    if (!m_socket->sendTo(data, size, endpoint)) {
        logger->error("Failed to send message: {}", m_socket->getErrorString(m_socket->getLastError()));
        
        // Remove transaction
        {
            std::lock_guard<std::mutex> lock(m_transactionsMutex);
            m_transactions.erase(actualTransactionId);
        }
        
        // Release connection
        m_throttler->releaseConnection();
        
        // Call error callback if provided
        if (errorCallback) {
            errorCallback(actualTransactionId, m_socket->getLastError(), 
                         m_socket->getErrorString(m_socket->getLastError()));
        }
        
        return false;
    }
    
    logger->debug("Sent message with transaction ID: {}", actualTransactionId);
    return true;
}

bool UDPReliabilityEnhancer::processReceivedMessage(const uint8_t* data, int size, const EndPoint& endpoint) {
    if (!m_running) {
        logger->error("UDPReliabilityEnhancer not running");
        return false;
    }
    
    if (size <= 0 || !data) {
        logger->error("Invalid data");
        return false;
    }
    
    // Extract transaction ID from the message
    // This is application-specific, so we'll assume the first 4 bytes are the transaction ID
    // In a real implementation, this would be extracted based on the protocol
    if (size < 4) {
        logger->error("Message too small to contain transaction ID");
        return false;
    }
    
    // For demonstration purposes, we'll use the first 4 bytes as a hex string
    std::stringstream ss;
    for (int i = 0; i < 4; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
    }
    std::string transactionId = ss.str();
    
    // Check for duplicate message
    {
        std::lock_guard<std::mutex> lock(m_cachedTransactionsMutex);
        
        auto it = m_cachedTransactions.find(transactionId);
        if (it != m_cachedTransactions.end()) {
            // This is a duplicate message
            m_duplicateCount++;
            logger->debug("Duplicate message detected with transaction ID: {}", transactionId);
            return true; // We've handled it by ignoring it
        }
        
        // Add to cached transactions
        m_cachedTransactions[transactionId] = std::chrono::steady_clock::now();
        
        // Clean up old cached transactions if we've reached the limit
        if (m_cachedTransactions.size() > m_config.maxCachedTransactions) {
            cleanupCachedTransactions();
        }
    }
    
    // Find the transaction
    std::shared_ptr<UDPTransaction> transaction;
    {
        std::lock_guard<std::mutex> lock(m_transactionsMutex);
        
        auto it = m_transactions.find(transactionId);
        if (it != m_transactions.end()) {
            transaction = it->second;
            
            // Remove the transaction from the map
            m_transactions.erase(it);
        }
    }
    
    // If we found a transaction, call the response callback
    if (transaction && transaction->responseCallback) {
        transaction->responseCallback(data, size, endpoint);
        
        // Release connection
        m_throttler->releaseConnection();
        
        logger->debug("Processed response for transaction ID: {}", transactionId);
        return true;
    }
    
    // If we didn't find a transaction, this might be a new message
    // In a real implementation, we would check if this is a query or a response
    // For now, we'll just return true
    logger->debug("Received message with unknown transaction ID: {}", transactionId);
    return true;
}

void UDPReliabilityEnhancer::setConfig(const UDPReliabilityConfig& config) {
    m_config = config;
    
    // Update rate limiter if rate limiting is enabled
    if (m_config.maxBytesPerSecond > 0) {
        if (m_rateLimiter) {
            m_rateLimiter->setRate(m_config.maxBytesPerSecond);
            m_rateLimiter->setBurstSize(m_config.burstSize);
        } else {
            m_rateLimiter = std::make_unique<RateLimiter>(m_config.maxBytesPerSecond, m_config.burstSize);
        }
    } else {
        m_rateLimiter.reset();
    }
    
    // Update connection throttler
    m_throttler->setMaxConnections(m_config.maxConcurrentTransactions);
    
    logger->debug("Updated configuration with max retries: {}, initial timeout: {} ms",
                 m_config.maxRetries, m_config.initialTimeout.count());
}

UDPReliabilityConfig UDPReliabilityEnhancer::getConfig() const {
    return m_config;
}

size_t UDPReliabilityEnhancer::getActiveTransactionCount() const {
    std::lock_guard<std::mutex> lock(m_transactionsMutex);
    return m_transactions.size();
}

size_t UDPReliabilityEnhancer::getDuplicateCount() const {
    return m_duplicateCount.load();
}

size_t UDPReliabilityEnhancer::getRetryCount() const {
    return m_retryCount.load();
}

size_t UDPReliabilityEnhancer::getTimeoutCount() const {
    return m_timeoutCount.load();
}

std::string UDPReliabilityEnhancer::generateTransactionId() {
    // Generate a random 4-byte transaction ID
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
    uint32_t id = dist(m_rng);
    
    // Convert to hex string
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << id;
    return ss.str();
}

void UDPReliabilityEnhancer::checkTimeouts() {
    auto now = std::chrono::steady_clock::now();
    
    // Get a copy of the transactions to avoid holding the lock while calling callbacks
    std::vector<std::shared_ptr<UDPTransaction>> expiredTransactions;
    
    {
        std::lock_guard<std::mutex> lock(m_transactionsMutex);
        
        for (auto it = m_transactions.begin(); it != m_transactions.end();) {
            auto& transaction = it->second;
            
            if (now >= transaction->expiryTime) {
                // Transaction has expired
                
                if (transaction->retries < m_config.maxRetries) {
                    // Retry the transaction
                    retryTransaction(transaction);
                    ++it;
                } else {
                    // Max retries reached, remove the transaction
                    expiredTransactions.push_back(transaction);
                    it = m_transactions.erase(it);
                }
            } else {
                ++it;
            }
        }
    }
    
    // Process expired transactions
    for (auto& transaction : expiredTransactions) {
        // Increment timeout count
        m_timeoutCount++;
        
        // Call timeout callback if provided
        if (transaction->timeoutCallback) {
            transaction->timeoutCallback(transaction->transactionId);
        }
        
        // Release connection
        m_throttler->releaseConnection();
        
        logger->debug("Transaction timed out: {}", transaction->transactionId);
    }
}

void UDPReliabilityEnhancer::timeoutThread() {
    logger->debug("Timeout thread started");
    
    while (m_running) {
        // Check for timeouts
        checkTimeouts();
        
        // Sleep for a short time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    logger->debug("Timeout thread stopped");
}

void UDPReliabilityEnhancer::retryTransaction(std::shared_ptr<UDPTransaction> transaction) {
    // Increment retry count
    transaction->retries++;
    m_retryCount++;
    
    // Calculate new expiry time with exponential backoff
    auto timeout = m_config.initialTimeout;
    for (int i = 0; i < transaction->retries; ++i) {
        timeout = std::chrono::milliseconds(
            static_cast<int64_t>(timeout.count() * m_config.timeoutMultiplier));
    }
    
    // Cap at max timeout
    if (timeout > m_config.maxTimeout) {
        timeout = m_config.maxTimeout;
    }
    
    // Update expiry time
    transaction->expiryTime = std::chrono::steady_clock::now() + timeout;
    
    // Resend the message
    if (!m_socket->sendTo(transaction->data.data(), static_cast<int>(transaction->data.size()), transaction->endpoint)) {
        logger->error("Failed to resend message: {}", m_socket->getErrorString(m_socket->getLastError()));
        
        // Call error callback if provided
        if (transaction->errorCallback) {
            transaction->errorCallback(transaction->transactionId, m_socket->getLastError(),
                                     m_socket->getErrorString(m_socket->getLastError()));
        }
        
        // Remove the transaction
        removeTransaction(transaction->transactionId);
        
        // Release connection
        m_throttler->releaseConnection();
    } else {
        logger->debug("Retried transaction: {} (retry {}/{})",
                     transaction->transactionId, transaction->retries, m_config.maxRetries);
    }
}

void UDPReliabilityEnhancer::removeTransaction(const std::string& transactionId) {
    std::lock_guard<std::mutex> lock(m_transactionsMutex);
    m_transactions.erase(transactionId);
}

void UDPReliabilityEnhancer::cleanupCachedTransactions() {
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(m_cachedTransactionsMutex);
    
    for (auto it = m_cachedTransactions.begin(); it != m_cachedTransactions.end();) {
        if (now - it->second > m_config.duplicateDetectionWindow) {
            it = m_cachedTransactions.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dht_hunter::network
