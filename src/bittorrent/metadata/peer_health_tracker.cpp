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

void PeerHealthTracker::recordSuccess(const dht_hunter::network::EndPoint& peer, int latencyMs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto& health = m_peerHealth[peerKey];

    // Use the enhanced update method
    health.updateWithResult(true, latencyMs);

    // Log significant improvements
    if (health.consecutiveSuccesses == 3 && health.failureCount > 0) {
        unified_event::logInfo("BitTorrent.PeerHealthTracker", "Peer " + peerKey +
                             " has become reliable with 3 consecutive successes");
    }
}

void PeerHealthTracker::recordFailure(const dht_hunter::network::EndPoint& peer, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto& health = m_peerHealth[peerKey];

    // Store the failure reason
    health.lastFailureReason = reason;

    // Use the enhanced update method
    health.updateWithResult(false);

    // Log significant degradation
    if (health.consecutiveFailures == 3) {
        unified_event::logWarning("BitTorrent.PeerHealthTracker", "Peer " + peerKey +
                               " has become unreliable with 3 consecutive failures: " + reason);
    }
}

float PeerHealthTracker::getSuccessRate(const dht_hunter::network::EndPoint& peer, bool useWeighted) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto it = m_peerHealth.find(peerKey);
    if (it == m_peerHealth.end()) {
        return 0.5f; // Default to 50% success rate for unknown peers
    }

    const auto& health = it->second;

    if (useWeighted) {
        // Return the pre-calculated weighted success rate
        return health.weightedSuccessRate;
    } else {
        // Calculate traditional success rate
        int totalAttempts = health.successCount + health.failureCount;
        if (totalAttempts == 0) {
            return 0.5f; // Default to 50% success rate if no attempts
        }
        return static_cast<float>(health.successCount) / totalAttempts;
    }
}

float PeerHealthTracker::getHealthScore(const dht_hunter::network::EndPoint& peer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto it = m_peerHealth.find(peerKey);
    if (it == m_peerHealth.end()) {
        return 0.5f; // Default health score for unknown peers
    }

    return it->second.getHealthScore();
}

float PeerHealthTracker::getAverageLatency(const dht_hunter::network::EndPoint& peer) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string peerKey = peer.toString();
    auto it = m_peerHealth.find(peerKey);
    if (it == m_peerHealth.end() || it->second.latencyHistory.empty()) {
        return 0.0f; // No latency data available
    }

    return it->second.averageLatencyMs;
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

    // Use the health score for a more comprehensive evaluation
    float healthScore = health.getHealthScore();

    // Consider a peer unhealthy if:
    // 1. Health score is below 0.3
    // 2. We've never succeeded with this peer and failed more than 3 times
    // 3. We have 5 or more consecutive failures

    if (healthScore < 0.3f) {
        return false;
    }

    if (health.successCount == 0 && health.failureCount > 3) {
        return false;
    }

    if (health.consecutiveFailures >= 5) {
        return false;
    }

    return true;
}

std::vector<dht_hunter::network::EndPoint> PeerHealthTracker::prioritizePeers(const std::vector<dht_hunter::network::EndPoint>& peers) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create vectors for different peer categories
    std::vector<dht_hunter::network::EndPoint> prioritizedPeers;
    std::vector<dht_hunter::network::EndPoint> unknownPeers;
    std::vector<dht_hunter::network::EndPoint> healthyPeers;
    std::vector<dht_hunter::network::EndPoint> unhealthyPeers;
    std::vector<std::pair<dht_hunter::network::EndPoint, float>> scoredPeers; // For sorting by health score

    // Categorize peers
    for (const auto& peer : peers) {
        std::string peerKey = peer.toString();
        auto it = m_peerHealth.find(peerKey);

        if (it == m_peerHealth.end()) {
            unknownPeers.push_back(peer);
        } else {
            // Get health score and categorize
            float score = it->second.getHealthScore();
            scoredPeers.emplace_back(peer, score);

            if (isHealthy(peer)) {
                healthyPeers.push_back(peer);
            } else {
                unhealthyPeers.push_back(peer);
            }
        }
    }

    // Sort peers by health score (best first)
    std::sort(scoredPeers.begin(), scoredPeers.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Extract sorted peers
    std::vector<dht_hunter::network::EndPoint> sortedHealthyPeers;
    for (const auto& [peer, score] : scoredPeers) {
        if (score >= 0.3f) { // Only include reasonably healthy peers
            sortedHealthyPeers.push_back(peer);
        }
    }

    // Shuffle unknown peers to try different ones each time
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(unknownPeers.begin(), unknownPeers.end(), g);

    // Combine the lists: healthy peers first, then unknown peers, then unhealthy peers
    prioritizedPeers.insert(prioritizedPeers.end(), sortedHealthyPeers.begin(), sortedHealthyPeers.end());
    prioritizedPeers.insert(prioritizedPeers.end(), unknownPeers.begin(), unknownPeers.end());

    // Only include unhealthy peers if we have very few options
    if (prioritizedPeers.size() < 3) {
        // Sort unhealthy peers by score (best first)
        std::vector<std::pair<dht_hunter::network::EndPoint, float>> sortedUnhealthy;
        for (const auto& peer : unhealthyPeers) {
            auto it = m_peerHealth.find(peer.toString());
            if (it != m_peerHealth.end()) {
                sortedUnhealthy.emplace_back(peer, it->second.getHealthScore());
            }
        }

        std::sort(sortedUnhealthy.begin(), sortedUnhealthy.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        // Add sorted unhealthy peers
        for (const auto& [peer, _] : sortedUnhealthy) {
            prioritizedPeers.push_back(peer);
        }
    }

    // Log prioritization results
    if (!peers.empty()) {
        unified_event::logDebug("BitTorrent.PeerHealthTracker", "Prioritized " + std::to_string(peers.size()) +
                              " peers: " + std::to_string(sortedHealthyPeers.size()) + " healthy, " +
                              std::to_string(unknownPeers.size()) + " unknown, " +
                              std::to_string(unhealthyPeers.size()) + " unhealthy");
    }

    return prioritizedPeers;
}

} // namespace dht_hunter::bittorrent
