#include "dht_hunter/bittorrent/metadata/peer_health_tracker.hpp"
#include <algorithm>
#include <random>

namespace dht_hunter::bittorrent {

std::shared_ptr<PeerHealthTracker> PeerHealthTracker::s_instance = nullptr;
std::mutex PeerHealthTracker::s_instanceMutex;

std::shared_ptr<PeerHealthTracker> PeerHealthTracker::getInstance() {
    std::lock_guard<std::mutex> lock(s_instanceMutex);
    if (!s_instance) {
        s_instance = std::shared_ptr<PeerHealthTracker>(new PeerHealthTracker());
    }
    return s_instance;
}

PeerHealthTracker::PeerHealthTracker() {
    unified_event::logInfo("BitTorrent.PeerHealthTracker", "Initialized peer health tracker");
}

void PeerHealthTracker::recordSuccess(const dht_hunter::network::EndPoint& peer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto& health = m_peerHealth[peerKey];
    health.successCount++;
    health.lastSeenTime = std::chrono::system_clock::now();
}

void PeerHealthTracker::recordFailure(const dht_hunter::network::EndPoint& peer, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto& health = m_peerHealth[peerKey];
    health.failureCount++;
    health.lastSeenTime = std::chrono::system_clock::now();
    health.lastFailureReason = reason;
}

float PeerHealthTracker::getSuccessRate(const dht_hunter::network::EndPoint& peer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto it = m_peerHealth.find(peerKey);
    if (it == m_peerHealth.end()) {
        return 0.5f; // Default to 50% success rate for unknown peers
    }

    const auto& health = it->second;
    int totalAttempts = health.successCount + health.failureCount;
    if (totalAttempts == 0) {
        return 0.5f; // Default to 50% success rate if no attempts
    }

    return static_cast<float>(health.successCount) / totalAttempts;
}

std::chrono::system_clock::time_point PeerHealthTracker::getLastSeenTime(const dht_hunter::network::EndPoint& peer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto it = m_peerHealth.find(peerKey);
    if (it == m_peerHealth.end()) {
        return std::chrono::system_clock::time_point(); // Return epoch time for unknown peers
    }

    return it->second.lastSeenTime;
}

bool PeerHealthTracker::isHealthy(const dht_hunter::network::EndPoint& peer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto it = m_peerHealth.find(peerKey);
    if (it == m_peerHealth.end()) {
        return true; // Assume unknown peers are healthy
    }

    const auto& health = it->second;

    // If we've never succeeded with this peer and failed more than 3 times, consider it unhealthy
    if (health.successCount == 0 && health.failureCount > 3) {
        return false;
    }

    // If the success rate is less than 20%, consider it unhealthy
    float successRate = static_cast<float>(health.successCount) / (health.successCount + health.failureCount);
    if (successRate < 0.2f && health.failureCount > 2) {
        return false;
    }

    return true;
}

std::vector<dht_hunter::network::EndPoint> PeerHealthTracker::prioritizePeers(const std::vector<dht_hunter::network::EndPoint>& peers) {
    std::vector<dht_hunter::network::EndPoint> prioritizedPeers;
    std::vector<dht_hunter::network::EndPoint> unknownPeers;
    std::vector<dht_hunter::network::EndPoint> healthyPeers;
    std::vector<dht_hunter::network::EndPoint> unhealthyPeers;

    // Categorize peers
    for (const auto& peer : peers) {
        std::string peerKey = peer.toString();
        auto it = m_peerHealth.find(peerKey);

        if (it == m_peerHealth.end()) {
            unknownPeers.push_back(peer);
        } else if (isHealthy(peer)) {
            healthyPeers.push_back(peer);
        } else {
            unhealthyPeers.push_back(peer);
        }
    }

    // Sort healthy peers by success rate
    std::sort(healthyPeers.begin(), healthyPeers.end(), [this](const network::EndPoint& a, const network::EndPoint& b) {
        return getSuccessRate(a) > getSuccessRate(b);
    });

    // Shuffle unknown peers to try different ones each time
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(unknownPeers.begin(), unknownPeers.end(), g);

    // Combine the lists: healthy peers first, then unknown peers, then unhealthy peers
    prioritizedPeers.insert(prioritizedPeers.end(), healthyPeers.begin(), healthyPeers.end());
    prioritizedPeers.insert(prioritizedPeers.end(), unknownPeers.begin(), unknownPeers.end());

    // Only include unhealthy peers if we have very few options
    if (prioritizedPeers.size() < 3) {
        prioritizedPeers.insert(prioritizedPeers.end(), unhealthyPeers.begin(), unhealthyPeers.end());
    }

    return prioritizedPeers;
}

} // namespace dht_hunter::bittorrent
