#include "dht_hunter/network/rate_limiter.hpp"
#include "dht_hunter/logforge/logforge.hpp"

namespace dht_hunter::network {

namespace {
    auto logger = dht_hunter::logforge::LogForge::getLogger("Network.BurstController");
}

BurstController::BurstController(size_t targetRate, size_t maxBurstRate, std::chrono::milliseconds burstDuration)
    : m_targetRate(targetRate),
      m_maxBurstRate(maxBurstRate),
      m_burstDuration(burstDuration),
      m_bytesInCurrentBurst(0),
      m_burstStartTime(std::chrono::steady_clock::now()) {
    
    logger->debug("Created BurstController with target rate: {} bytes/sec, max burst rate: {} bytes/sec, burst duration: {} ms",
                 targetRate, maxBurstRate, burstDuration.count());
}

std::chrono::milliseconds BurstController::processTransfer(size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Get the current time
    auto now = std::chrono::steady_clock::now();
    
    // Calculate the elapsed time since the burst started
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_burstStartTime);
    
    // If the burst duration has elapsed, start a new burst
    if (elapsed > m_burstDuration.load()) {
        m_bytesInCurrentBurst = bytes;
        m_burstStartTime = now;
        
        // Calculate the delay based on the target rate
        auto targetDelay = std::chrono::milliseconds(
            static_cast<int64_t>((static_cast<double>(bytes) / m_targetRate.load()) * 1000));
        
        return targetDelay;
    }
    
    // Add the bytes to the current burst
    m_bytesInCurrentBurst += bytes;
    
    // Calculate the current burst rate
    double burstRate = static_cast<double>(m_bytesInCurrentBurst) / (elapsed.count() / 1000.0);
    
    // If the burst rate exceeds the maximum, calculate a delay to bring it back down
    if (burstRate > m_maxBurstRate.load()) {
        // Calculate how long we should have taken to transfer this many bytes at the max burst rate
        auto expectedTime = std::chrono::milliseconds(
            static_cast<int64_t>((static_cast<double>(m_bytesInCurrentBurst) / m_maxBurstRate.load()) * 1000));
        
        // Calculate the delay needed to match the expected time
        if (expectedTime > elapsed) {
            return expectedTime - elapsed;
        }
    }
    
    // Calculate the delay based on the target rate
    auto targetDelay = std::chrono::milliseconds(
        static_cast<int64_t>((static_cast<double>(bytes) / m_targetRate.load()) * 1000));
    
    return targetDelay;
}

void BurstController::setTargetRate(size_t targetRate) {
    m_targetRate.store(targetRate);
    logger->debug("Target rate updated to {} bytes/sec", targetRate);
}

size_t BurstController::getTargetRate() const {
    return m_targetRate.load();
}

void BurstController::setMaxBurstRate(size_t maxBurstRate) {
    m_maxBurstRate.store(maxBurstRate);
    logger->debug("Max burst rate updated to {} bytes/sec", maxBurstRate);
}

size_t BurstController::getMaxBurstRate() const {
    return m_maxBurstRate.load();
}

void BurstController::setBurstDuration(std::chrono::milliseconds burstDuration) {
    m_burstDuration.store(burstDuration);
    logger->debug("Burst duration updated to {} ms", burstDuration.count());
}

std::chrono::milliseconds BurstController::getBurstDuration() const {
    return m_burstDuration.load();
}

} // namespace dht_hunter::network
