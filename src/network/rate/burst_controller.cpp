#include "dht_hunter/network/rate/burst_controller.hpp"

namespace dht_hunter::network {

BurstController::BurstController(size_t maxBurstSize, std::chrono::milliseconds burstWindow)
    : m_maxBurstSize(maxBurstSize), m_burstWindow(burstWindow) {
}

BurstController::~BurstController() {
}

bool BurstController::tryAcquire(const EndPoint& endpoint) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Remove expired bursts
    removeExpiredBursts();
    
    // Get or create the burst for this endpoint
    auto& burst = m_bursts[endpoint];
    
    // Check if the burst is expired
    auto now = std::chrono::steady_clock::now();
    if (now - burst.timestamp > m_burstWindow) {
        // Reset the burst
        burst.count = 0;
        burst.timestamp = now;
    }
    
    // Check if we've reached the maximum burst size
    if (burst.count >= m_maxBurstSize) {
        return false;
    }
    
    // Increment the burst count
    ++burst.count;
    
    return true;
}

size_t BurstController::getMaxBurstSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxBurstSize;
}

void BurstController::setMaxBurstSize(size_t maxBurstSize) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxBurstSize = maxBurstSize;
}

std::chrono::milliseconds BurstController::getBurstWindow() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_burstWindow;
}

void BurstController::setBurstWindow(std::chrono::milliseconds burstWindow) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_burstWindow = burstWindow;
}

size_t BurstController::getBurstCount(const EndPoint& endpoint) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_bursts.find(endpoint);
    if (it != m_bursts.end()) {
        // Check if the burst is expired
        auto now = std::chrono::steady_clock::now();
        if (now - it->second.timestamp <= m_burstWindow) {
            return it->second.count;
        }
    }
    
    return 0;
}

void BurstController::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bursts.clear();
}

void BurstController::removeExpiredBursts() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_bursts.begin(); it != m_bursts.end();) {
        if (now - it->second.timestamp > m_burstWindow) {
            it = m_bursts.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace dht_hunter::network
