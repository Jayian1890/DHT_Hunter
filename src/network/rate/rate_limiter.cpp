#include "dht_hunter/network/rate/rate_limiter.hpp"
#include <algorithm>
#include <thread>

namespace dht_hunter::network {

RateLimiter::RateLimiter(double maxOperationsPerSecond, size_t burstSize)
    : m_maxOperationsPerSecond(maxOperationsPerSecond), m_burstSize(burstSize), m_tokens(burstSize), m_operationCount(0) {
    m_lastUpdateTime = std::chrono::steady_clock::now();
}

RateLimiter::~RateLimiter() {
}

bool RateLimiter::tryAcquire() {
    return tryAcquire(1);
}

bool RateLimiter::tryAcquire(size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    updateTokens();
    
    if (m_tokens >= count) {
        m_tokens -= count;
        m_operationCount += count;
        return true;
    }
    
    return false;
}

void RateLimiter::acquire() {
    acquire(1);
}

void RateLimiter::acquire(size_t count) {
    while (!tryAcquire(count)) {
        // Sleep for a short time to avoid busy waiting
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

double RateLimiter::getMaxOperationsPerSecond() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxOperationsPerSecond;
}

void RateLimiter::setMaxOperationsPerSecond(double maxOperationsPerSecond) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxOperationsPerSecond = maxOperationsPerSecond;
}

size_t RateLimiter::getBurstSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_burstSize;
}

void RateLimiter::setBurstSize(size_t burstSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_burstSize = burstSize;
    m_tokens = std::min(m_tokens, static_cast<double>(burstSize));
}

size_t RateLimiter::getOperationCount() const {
    return m_operationCount.load();
}

void RateLimiter::resetOperationCount() {
    m_operationCount.store(0);
}

void RateLimiter::updateTokens() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_lastUpdateTime).count();
    
    // Add tokens based on the elapsed time and rate
    double newTokens = elapsed * m_maxOperationsPerSecond;
    m_tokens = std::min(m_tokens + newTokens, static_cast<double>(m_burstSize));
    
    m_lastUpdateTime = now;
}

} // namespace dht_hunter::network
