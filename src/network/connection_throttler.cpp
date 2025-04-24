#include "dht_hunter/network/rate_limiter.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.ConnectionThrottler");
}

ConnectionThrottler::ConnectionThrottler(size_t maxConnections)
    : m_maxConnections(maxConnections),
      m_activeConnections(0) {
    
    logger->debug("Created ConnectionThrottler with max connections: {}", maxConnections);
}

bool ConnectionThrottler::requestConnection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Check if we're below the connection limit
    if (m_activeConnections.load() < m_maxConnections.load()) {
        // Increment the active connection count
        m_activeConnections++;
        return true;
    }
    
    // Connection limit reached
    return false;
}

bool ConnectionThrottler::requestConnectionWithWait(std::chrono::milliseconds maxWaitTime) {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // Check if we're below the connection limit immediately
    if (m_activeConnections.load() < m_maxConnections.load()) {
        // Increment the active connection count
        m_activeConnections++;
        return true;
    }
    
    // Wait for a connection slot to become available
    auto waitUntil = std::chrono::steady_clock::now() + maxWaitTime;
    
    // Use a condition variable to wait
    bool success = m_cv.wait_until(lock, waitUntil, [this]() {
        return m_activeConnections.load() < m_maxConnections.load();
    });
    
    if (success) {
        // Increment the active connection count
        m_activeConnections++;
    }
    
    return success;
}

void ConnectionThrottler::releaseConnection() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Decrement the active connection count
    if (m_activeConnections.load() > 0) {
        m_activeConnections--;
        
        // Notify any waiting threads
        m_cv.notify_one();
    }
}

void ConnectionThrottler::setMaxConnections(size_t maxConnections) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Set the new maximum
    m_maxConnections.store(maxConnections);
    
    // If the new maximum is higher than the old one, notify waiting threads
    if (m_activeConnections.load() < m_maxConnections.load()) {
        m_cv.notify_all();
    }
    
    logger->debug("Max connections updated to {}", maxConnections);
}

size_t ConnectionThrottler::getMaxConnections() const {
    return m_maxConnections.load();
}

size_t ConnectionThrottler::getActiveConnections() const {
    return m_activeConnections.load();
}

} // namespace dht_hunter::network
