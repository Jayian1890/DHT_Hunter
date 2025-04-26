#pragma once

#include "dht_hunter/dht/core/dht_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/event/logger.hpp"
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>

namespace dht_hunter::dht {

/**
 * @brief Stores peers for info hashes
 */
class PeerStorage {
public:
    /**
     * @brief Constructs a peer storage
     * @param config The DHT configuration
     */
    explicit PeerStorage(const DHTConfig& config);

    /**
     * @brief Destructor
     */
    ~PeerStorage();

    /**
     * @brief Starts the peer storage
     * @return True if the peer storage was started successfully, false otherwise
     */
    bool start();

    /**
     * @brief Stops the peer storage
     */
    void stop();

    /**
     * @brief Checks if the peer storage is running
     * @return True if the peer storage is running, false otherwise
     */
    bool isRunning() const;

    /**
     * @brief Adds a peer for an info hash
     * @param infoHash The info hash
     * @param endpoint The peer endpoint
     */
    void addPeer(const InfoHash& infoHash, const network::EndPoint& endpoint);

    /**
     * @brief Gets peers for an info hash
     * @param infoHash The info hash
     * @return The peers
     */
    std::vector<network::EndPoint> getPeers(const InfoHash& infoHash);

    /**
     * @brief Gets the number of info hashes
     * @return The number of info hashes
     */
    size_t getInfoHashCount() const;

    /**
     * @brief Gets the number of peers for an info hash
     * @param infoHash The info hash
     * @return The number of peers
     */
    size_t getPeerCount(const InfoHash& infoHash) const;

    /**
     * @brief Gets the total number of peers
     * @return The total number of peers
     */
    size_t getTotalPeerCount() const;

private:
    /**
     * @brief Cleans up expired peers
     */
    void cleanupExpiredPeers();

    /**
     * @brief Cleans up expired peers periodically
     */
    void cleanupExpiredPeersPeriodically();

    /**
     * @brief A peer with a timestamp
     */
    struct TimestampedPeer {
        network::EndPoint endpoint;
        std::chrono::steady_clock::time_point timestamp;
        
        TimestampedPeer(const network::EndPoint& ep)
            : endpoint(ep), timestamp(std::chrono::steady_clock::now()) {}
    };

    DHTConfig m_config;
    std::unordered_map<InfoHash, std::vector<TimestampedPeer>> m_peers;
    std::atomic<bool> m_running;
    std::thread m_cleanupThread;
    mutable std::mutex m_mutex;
    event::Logger m_logger;
};

} // namespace dht_hunter::dht
