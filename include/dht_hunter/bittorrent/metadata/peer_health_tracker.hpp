#pragma once

#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/unified_event/unified_event.hpp"
#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>
#include <memory>

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
     */
    void recordSuccess(const dht_hunter::network::EndPoint& peer);

    /**
     * @brief Record a failed connection to a peer
     * @param peer The peer that failed to connect
     * @param reason The reason for the failure
     */
    void recordFailure(const dht_hunter::network::EndPoint& peer, const std::string& reason);

    /**
     * @brief Get the success rate for a peer
     * @param peer The peer to get the success rate for
     * @return The success rate (0.0 - 1.0)
     */
    float getSuccessRate(const dht_hunter::network::EndPoint& peer);

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
     * @brief Struct to store peer health information
     */
    struct PeerHealth {
        int successCount = 0;
        int failureCount = 0;
        std::chrono::system_clock::time_point lastSeenTime;
        std::string lastFailureReason;
    };

    std::unordered_map<std::string, PeerHealth> m_peerHealth;
    std::mutex m_mutex;
    static std::shared_ptr<PeerHealthTracker> s_instance;
    static std::mutex s_instanceMutex;
};

} // namespace dht_hunter::bittorrent
