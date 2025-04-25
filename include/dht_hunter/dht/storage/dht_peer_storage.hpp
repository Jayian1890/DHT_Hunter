#pragma once

#include "dht_hunter/dht/core/dht_node_config.hpp"
#include "dht_hunter/dht/core/dht_types.hpp"
#include "dht_hunter/network/network_address.hpp"
#include "dht_hunter/util/mutex_utils.hpp"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <chrono>

namespace dht_hunter::dht {

/**
 * @brief Stores peers for info hashes
 */
class DHTPeerStorage {
public:
    /**
     * @brief Constructs a peer storage
     * @param config The DHT node configuration
     */
    explicit DHTPeerStorage(const DHTNodeConfig& config);

    /**
     * @brief Destructor
     */
    ~DHTPeerStorage();

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
     * @brief Stores a peer for an info hash
     * @param infoHash The info hash
     * @param endpoint The peer's endpoint
     * @return True if the peer was stored, false otherwise
     */
    bool storePeer(const InfoHash& infoHash, const network::EndPoint& endpoint);

    /**
     * @brief Gets stored peers for an info hash
     * @param infoHash The info hash
     * @return The stored peers
     */
    std::vector<network::EndPoint> getStoredPeers(const InfoHash& infoHash) const;

    /**
     * @brief Gets the number of stored peers for an info hash
     * @param infoHash The info hash
     * @return The number of stored peers
     */
    size_t getStoredPeerCount(const InfoHash& infoHash) const;

    /**
     * @brief Gets the total number of stored peers across all info hashes
     * @return The total number of stored peers
     */
    size_t getTotalStoredPeerCount() const;

    /**
     * @brief Gets the number of info hashes with stored peers
     * @return The number of info hashes
     */
    size_t getInfoHashCount() const;

    /**
     * @brief Saves the peer cache to a file
     * @param filePath The path to the file
     * @return True if the peer cache was saved successfully, false otherwise
     */
    bool savePeerCache(const std::string& filePath) const;

    /**
     * @brief Loads the peer cache from a file
     * @param filePath The path to the file
     * @return True if the peer cache was loaded successfully, false otherwise
     */
    bool loadPeerCache(const std::string& filePath);

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
     * @brief Saves the peer cache periodically
     */
    void savePeerCachePeriodically();

    /**
     * @brief Represents a stored peer
     */
    struct StoredPeer {
        network::EndPoint endpoint;
        std::chrono::steady_clock::time_point expirationTime;
    };

    DHTNodeConfig m_config;
    std::string m_peerCachePath;
    std::unordered_map<InfoHash, std::vector<StoredPeer>> m_peers;
    mutable util::CheckedMutex m_peersMutex;
    std::atomic<bool> m_running{false};
    std::thread m_cleanupThread;
    std::thread m_saveThread;
};

} // namespace dht_hunter::dht
