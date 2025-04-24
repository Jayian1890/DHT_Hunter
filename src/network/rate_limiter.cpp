#include "dht_hunter/network/rate_limiter.hpp"
#include "dht_hunter/logforge/logforge.hpp"
#include "dht_hunter/logforge/logger_macros.hpp"
#include <algorithm>
#include <thread>

DEFINE_COMPONENT_LOGGER("Network", "RateLimiter")

namespace dht_hunter::network {
//
// RateLimiter implementation
//
RateLimiter::RateLimiter(size_t bytesPerSecond, size_t burstSize)
    : m_bytesPerSecond(bytesPerSecond),
      m_burstSize(burstSize == 0 ? bytesPerSecond * 2 : burstSize),
      m_availableTokens(bytesPerSecond), // Initialize with rate limit, not burst size
      m_lastUpdate(std::chrono::steady_clock::now()) {
    getLogger()->debug("Created RateLimiter with rate: {} bytes/sec, burst size: {} bytes, initial tokens: {}",
                 bytesPerSecond, m_burstSize.load(), m_availableTokens);
}
bool RateLimiter::request(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Update the token count based on elapsed time
    size_t beforeTokens = m_availableTokens;
    updateTokens();
    size_t afterTokens = m_availableTokens;
    
    // Check if there are enough tokens available
    if (bytes <= m_availableTokens) {
        // Consume tokens
        m_availableTokens -= bytes;
        getLogger()->debug("Request for {} bytes granted. Tokens: {} -> {} -> {}", 
                     bytes, beforeTokens, afterTokens, m_availableTokens);
        return true;
    }
    // Not enough tokens available
    getLogger()->debug("Request for {} bytes denied. Tokens: {} -> {}, needed: {}", 
                 bytes, beforeTokens, afterTokens, bytes);
    return false;
}
bool RateLimiter::requestWithWait(size_t bytes, std::chrono::milliseconds maxWaitTime) {
    std::unique_lock<std::mutex> lock(m_mutex);
    // Update the token count based on elapsed time
    updateTokens();
    // Check if there are enough tokens available immediately
    if (bytes <= m_availableTokens) {
        // Consume tokens
        m_availableTokens -= bytes;
        return true;
    }
    // Calculate how long we need to wait for enough tokens
    size_t tokensNeeded = bytes - m_availableTokens;
    auto timeNeeded = std::chrono::milliseconds(
        static_cast<int64_t>((static_cast<double>(tokensNeeded) / m_bytesPerSecond.load()) * 1000));
    // If the wait time is longer than the maximum, return false
    if (timeNeeded > maxWaitTime) {
        return false;
    }
    // Wait for tokens to become available
    auto waitUntil = std::chrono::steady_clock::now() + timeNeeded;
    // Use a condition variable to wait
    bool success = m_cv.wait_until(lock, waitUntil, [this, bytes]() {
        updateTokens();
        return bytes <= m_availableTokens;
    });
    if (success) {
        // Consume tokens
        m_availableTokens -= bytes;
    }
    return success;
}
void RateLimiter::setRate(size_t bytesPerSecond) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Update tokens before changing the rate
    updateTokens();
    // Set the new rate
    m_bytesPerSecond.store(bytesPerSecond);
    // Adjust burst size if necessary
    if (m_burstSize.load() < bytesPerSecond) {
        m_burstSize.store(bytesPerSecond * 2);
    }
    getLogger()->debug("Rate limit updated to {} bytes/sec", bytesPerSecond);
}
size_t RateLimiter::getRate() const {
    return m_bytesPerSecond.load();
}
void RateLimiter::setBurstSize(size_t burstSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Update tokens before changing the burst size
    updateTokens();
    // Set the new burst size
    m_burstSize.store(burstSize);
    // Cap the available tokens to the new burst size
    m_availableTokens = std::min(m_availableTokens, burstSize);
    getLogger()->debug("Burst size updated to {} bytes", burstSize);
}
size_t RateLimiter::getBurstSize() const {
    return m_burstSize.load();
}
size_t RateLimiter::getAvailableTokens() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Update tokens before returning the count
    const_cast<RateLimiter*>(this)->updateTokens();
    return m_availableTokens;
}
void RateLimiter::updateTokens() {
    // Get the current time
    auto now = std::chrono::steady_clock::now();
    // Calculate the elapsed time since the last update
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastUpdate);
    // If no time has elapsed, there's nothing to update
    if (elapsed.count() <= 0) {
        return;
    }
    
    // Calculate how many tokens to add based on the elapsed time
    double secondsElapsed = elapsed.count() / 1000.0;
    size_t tokensToAdd = static_cast<size_t>(secondsElapsed * m_bytesPerSecond.load());
    
    size_t oldTokens = m_availableTokens;
    // Add tokens, but don't exceed the burst size
    m_availableTokens = std::min(m_availableTokens + tokensToAdd, m_burstSize.load());
    
    getLogger()->debug("Updated tokens: +{} tokens after {:.3f}s. {} -> {}", 
                 tokensToAdd, secondsElapsed, oldTokens, m_availableTokens);
    
    // Update the last update time
    m_lastUpdate = now;
    // Notify any waiting threads
    m_cv.notify_all();
}
} // namespace dht_hunter::network
