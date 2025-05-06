#pragma once

#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>
#include <memory>
#include <deque>
#include <vector>
#include <algorithm>

namespace dht_hunter::bittorrent {

/**
 * @brief Class to track the health of peers for metadata acquisition
 */
class PeerHealthTracker {
public:
    /**
     * @brief Get the singleton instance
     * @return The singleton instance
     */
    static std::shared_ptr<PeerHealthTracker> getInstance();

    /**
     * @brief Record a successful connection to a peer
     * @param peer The peer that was successfully connected to
     * @param latencyMs Connection latency in milliseconds (optional)
     */
    void recordSuccess(const dht_hunter::network::EndPoint& peer, int latencyMs = 0);

    /**
     * @brief Record a failed connection to a peer
     * @param peer The peer that failed to connect
     * @param reason The reason for the failure
     */
    void recordFailure(const dht_hunter::network::EndPoint& peer, const std::string& reason);

    /**
     * @brief Get the success rate for a peer
     * @param peer The peer to get the success rate for
     * @param useWeighted Whether to use the weighted success rate (default: true)
     * @return The success rate (0.0 - 1.0)
     */
    float getSuccessRate(const dht_hunter::network::EndPoint& peer, bool useWeighted = true);

    /**
     * @brief Get the health score for a peer
     * @param peer The peer to get the health score for
     * @return The health score (0.0 - 1.0)
     */
    float getHealthScore(const dht_hunter::network::EndPoint& peer);

    /**
     * @brief Get the average latency for a peer
     * @param peer The peer to get the average latency for
     * @return The average latency in milliseconds
     */
    float getAverageLatency(const dht_hunter::network::EndPoint& peer);

    /**
     * @brief Get the last seen time for a peer
     * @param peer The peer to get the last seen time for
     * @return The last seen time
     */
    std::chrono::system_clock::time_point getLastSeenTime(const dht_hunter::network::EndPoint& peer);

    /**
     * @brief Check if a peer is healthy
     * @param peer The peer to check
     * @return True if the peer is healthy, false otherwise
     */
    bool isHealthy(const dht_hunter::network::EndPoint& peer);

    /**
     * @brief Prioritize a list of peers based on health
     * @param peers The list of peers to prioritize
     * @return The prioritized list of peers
     */
    std::vector<dht_hunter::network::EndPoint> prioritizePeers(const std::vector<dht_hunter::network::EndPoint>& peers);

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    PeerHealthTracker();

    /**
     * @brief Struct to store peer health information with enhanced metrics
     */
    struct PeerHealth {
        int successCount = 0;
        int failureCount = 0;
        int consecutiveSuccesses = 0;
        int consecutiveFailures = 0;
        std::chrono::system_clock::time_point lastSeenTime;
        std::chrono::system_clock::time_point lastSuccessTime;
        std::chrono::system_clock::time_point lastFailureTime;
        std::string lastFailureReason;
        std::deque<bool> recentResults;        // Recent connection results (true=success, false=failure)
        std::deque<int> latencyHistory;        // Recent connection latencies in milliseconds
        float averageLatencyMs = 0.0f;         // Average connection latency
        float weightedSuccessRate = 0.5f;      // Success rate with more weight given to recent results

        // Constants
        static constexpr size_t MAX_HISTORY = 10;
        static constexpr float RECENCY_WEIGHT = 0.7f;  // Weight given to recent results vs. overall history

        // Update health metrics with a new result
        void updateWithResult(bool success, int latencyMs = 0) {
            // Update basic counters
            if (success) {
                successCount++;
                consecutiveSuccesses++;
                consecutiveFailures = 0;
                lastSuccessTime = std::chrono::system_clock::now();
            } else {
                failureCount++;
                consecutiveFailures++;
                consecutiveSuccesses = 0;
                lastFailureTime = std::chrono::system_clock::now();
            }

            lastSeenTime = std::chrono::system_clock::now();

            // Update recent results history
            recentResults.push_back(success);
            if (recentResults.size() > MAX_HISTORY) {
                recentResults.pop_front();
            }

            // Update latency history if this was a success
            if (success && latencyMs > 0) {
                latencyHistory.push_back(latencyMs);
                if (latencyHistory.size() > MAX_HISTORY) {
                    latencyHistory.pop_front();
                }

                // Recalculate average latency
                if (!latencyHistory.empty()) {
                    int sum = 0;
                    for (int latency : latencyHistory) {
                        sum += latency;
                    }
                    averageLatencyMs = static_cast<float>(sum) / latencyHistory.size();
                }
            }

            // Calculate weighted success rate
            calculateWeightedSuccessRate();
        }

        // Calculate weighted success rate giving more importance to recent results
        void calculateWeightedSuccessRate() {
            if (recentResults.empty()) {
                // If no history, use overall success rate or default to 0.5
                int totalAttempts = successCount + failureCount;
                weightedSuccessRate = (totalAttempts > 0) ?
                    static_cast<float>(successCount) / totalAttempts : 0.5f;
                return;
            }

            // Calculate recent success rate
            size_t recentSuccesses = 0;
            for (bool result : recentResults) {
                if (result) recentSuccesses++;
            }
            float recentRate = static_cast<float>(recentSuccesses) / recentResults.size();

            // Calculate overall success rate
            int totalAttempts = successCount + failureCount;
            float overallRate = (totalAttempts > 0) ?
                static_cast<float>(successCount) / totalAttempts : 0.5f;

            // Combine with weighting factor
            weightedSuccessRate = (RECENCY_WEIGHT * recentRate) + ((1.0f - RECENCY_WEIGHT) * overallRate);
        }

        // Get a health score from 0.0 (unhealthy) to 1.0 (very healthy)
        float getHealthScore() const {
            // Start with weighted success rate
            float score = weightedSuccessRate;

            // Penalize for consecutive failures
            if (consecutiveFailures > 0) {
                float failurePenalty = std::min(0.5f, 0.1f * consecutiveFailures);
                score -= failurePenalty;
            }

            // Bonus for consecutive successes
            if (consecutiveSuccesses > 0) {
                float successBonus = std::min(0.3f, 0.05f * consecutiveSuccesses);
                score += successBonus;
            }

            // Penalize for high latency (if we have latency data)
            if (!latencyHistory.empty()) {
                // Normalize latency penalty: 0ms = no penalty, 1000ms+ = 0.2 penalty
                float latencyPenalty = std::min(0.2f, averageLatencyMs / 5000.0f);
                score -= latencyPenalty;
            }

            // Clamp to valid range
            return std::max(0.0f, std::min(1.0f, score));
        }
    };

    std::unordered_map<std::string, PeerHealth> m_peerHealth;
    std::mutex m_mutex;
    static std::shared_ptr<PeerHealthTracker> s_instance;
    static std::mutex s_instanceMutex;
};

} // namespace dht_hunter::bittorrent
